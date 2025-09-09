// file: my_components/my_gxepd2/my_gxepd2.h
#pragma once

#include <vector>
#include <cstring>
#include <Arduino.h>
#include <SPI.h>

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/components/display/display.h"
#include "esphome/components/display/display_buffer.h"
#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/components/template/text_sensor/template_text_sensor.h"

#include <GxEPD2_3C.h>
#include <epd3c/GxEPD2_420c.h>
#include <Fonts/FreeSans12pt7b.h>

extern "C" {
  #include "esp_task_wdt.h"
}

// --- extern из main.cpp (ESPHome генерит, ты уже это используешь) ---
// Kitchen / Plant / Street: temp, hum, batt, updated
extern esphome::template_::TemplateTextSensor *k_t, *k_h, *k_b, *k_u;
extern esphome::template_::TemplateTextSensor *p_t, *p_h, *p_b, *p_u;
extern esphome::template_::TemplateTextSensor *s_t, *s_h, *s_b, *s_u;
// локальная отметка времени последнего «fetch»
extern esphome::template_::TemplateTextSensor *last_fetch;

namespace esphome {
namespace mygxdisplay {

static const char *const TAG_GX = "my_gxepd2";

class MyGxDisplay : public display::DisplayBuffer {
 public:
  MyGxDisplay(uint16_t width, uint16_t height,
              int8_t sck, int8_t mosi, int8_t miso,
              int8_t cs, int8_t dc, int8_t rst, int8_t busy)
      : width_(width), height_(height),
        sck_(sck), mosi_(mosi), miso_(miso),
        cs_(cs), dc_(dc), rst_(rst), busy_(busy),
        display_(GxEPD2_420c(cs, dc, rst, busy)) {
    const size_t bytes = ((width_ + 7) / 8) * height_;
    bw_.assign(bytes, 0x00);
    red_.assign(bytes, 0x00);
  }

  // --- Жизненный цикл компонента ---
  void setup() override {
    ESP_LOGI(TAG_GX, "Init SPI + GxEPD2 4.2\" tri-color...");

    pinMode(cs_,  OUTPUT);
    pinMode(dc_,  OUTPUT);
    pinMode(rst_, OUTPUT);
    pinMode(busy_, INPUT_PULLUP);

    digitalWrite(cs_, HIGH);
    digitalWrite(dc_, LOW);
    digitalWrite(rst_, HIGH);

    SPI.begin(sck_, miso_, mosi_, cs_);

    display_.init();
    display_.setRotation(rotation_);
    display_.setFullWindow();

    // ВАЖНО: ничего не рисуем в setup(), чтобы не ловить WDT.
    ESP_LOGI(TAG_GX, "Display ready; waiting for button to draw.");
  }

  void update() override {
    // По запросу: компонент НИЧЕГО не делает сам.
    // Оставлено пустым специально по твоему запросу.
  }

  void dump_config() override {
    ESP_LOGCONFIG(TAG_GX, "MyGxDisplay: %dx%d (tri-color), rotation=%u", width_, height_, rotation_);
  }

  display::DisplayType get_display_type() override { return display::DisplayType::DISPLAY_TYPE_COLOR; }
  int get_width_internal() override { return width_; }
  int get_height_internal() override { return height_; }

  void set_panel_rotation(uint8_t r) {
    rotation_ = (uint8_t)(r % 4);
    display_.setRotation(rotation_);
  }

  // --- публичный метод для кнопки ---
  void draw_dashboard_now() {
    ESP_LOGI(TAG_GX, "Draw dashboard (button)...");
    esp_task_wdt_delete(NULL);   // долгий full refresh — снимаем WDT
    this->draw_dashboard_();
    esp_task_wdt_add(NULL);      // возвращаем WDT
  }

 protected:
  void draw_absolute_pixel_internal(int x, int y, Color color) override {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    const uint32_t idx = (uint32_t)y * (uint32_t)width_ + (uint32_t)x;
    const uint32_t byte_index = idx >> 3;
    const uint8_t  bit_mask   = (uint8_t)(0x80u >> (idx & 0x7u));
    const bool is_black = (color.r == 0 && color.g == 0 && color.b == 0);
    const bool is_red   = (color.r > 0  && color.g == 0 && color.b == 0);

    if (is_black) { bw_[byte_index] |= bit_mask;  red_[byte_index] &= ~bit_mask; }
    else if (is_red) { red_[byte_index] |= bit_mask; bw_[byte_index] &= ~bit_mask; }
    else { bw_[byte_index] &= ~bit_mask; red_[byte_index] &= ~bit_mask; }
  }

 private:
  static std::string get_text_(text_sensor::TextSensor *ts) {
    if (ts && ts->has_state()) return std::string(ts->state.c_str());
    return std::string();
  }

  void draw_dashboard_() {
    const int colW = width_ / 3;
    const int rowH = 70;
    const int topY = 48;

    // читаем строки из text_sensor'ов
    std::string KT = get_text_(k_t), KH = get_text_(k_h), KB = get_text_(k_b), KU = get_text_(k_u);
    std::string PT = get_text_(p_t), PH = get_text_(p_h), PB = get_text_(p_b), PU = get_text_(p_u);
    std::string ST = get_text_(s_t), SH = get_text_(s_h), SB = get_text_(s_b), SU = get_text_(s_u);
    std::string LF = get_text_(last_fetch);

    ESP_LOGI(TAG_GX, "Vals: K[%s|%s|%s|%s] P[%s|%s|%s|%s] S[%s|%s|%s|%s] LF[%s]",
             KT.c_str(), KH.c_str(), KB.c_str(), KU.c_str(),
             PT.c_str(), PH.c_str(), PB.c_str(), PU.c_str(),
             ST.c_str(), SH.c_str(), SB.c_str(), SU.c_str(), LF.c_str());

    display_.setFullWindow();
    display_.firstPage();
    do {
      display_.fillScreen(GxEPD_WHITE);

      // Красная шапка
      display_.fillRect(0, 0, width_, 38, GxEPD_RED);
      display_.setFont(&FreeSans12pt7b);
      display_.setTextColor(GxEPD_WHITE);
      display_.setCursor(10, 28);
      display_.print("temp.local");
      display_.setCursor(width_ - 130, 28);
      display_.print("Updated:");
      display_.setCursor(width_ - 40, 28);
      display_.print("OK");

      // Заголовки колонок
      display_.setTextColor(GxEPD_BLACK);
      const char* titles[3] = {"Kitchen","Plant","Street"};
      for (int c = 0; c < 3; c++) {
        int x = c * colW + 8;
        display_.setCursor(x, topY);
        display_.print(titles[c]);
      }

      // Вертикальные разделители
      for (int c = 1; c < 3; c++) {
        int lx = c * colW;
        display_.drawLine(lx, topY + 6, lx, height_ - 24, GxEPD_BLACK);
      }

      auto cell = [&](int c, int r, const char* label, const std::string &val) {
        int x = c * colW + 8;
        int y = topY + 20 + r * rowH;
        display_.setCursor(x, y);
        display_.setTextColor(GxEPD_BLACK);
        display_.print(label);
        display_.setCursor(x, y + 28);
        if (!val.empty()) display_.print(val.c_str());
        else { display_.setTextColor(GxEPD_RED); display_.print("-"); display_.setTextColor(GxEPD_BLACK); }
      };

      // 3×3 + updated под колонками
      cell(0, 0, "Temp", KT);  cell(0, 1, "Hum",  KH);  cell(0, 2, "Batt", KB);
      cell(1, 0, "Temp", PT);  cell(1, 1, "Hum",  PH);  cell(1, 2, "Batt", PB);
      cell(2, 0, "Temp", ST);  cell(2, 1, "Hum",  SH);  cell(2, 2, "Batt", SB);

      auto upd = [&](int c, const std::string &u) {
        int x = c * colW + 8;
        int y = topY + 20 + 3 * rowH + 10;
        display_.setCursor(x, y);
        display_.setTextColor(GxEPD_BLACK);
        display_.print("Upd: ");
        display_.print(u.empty() ? "-" : u.c_str());
      };
      upd(0, KU); upd(1, PU); upd(2, SU);

      // низ экрана — last_fetch
      display_.setCursor(10, height_ - 4);
      display_.print("Last fetch: ");
      display_.print(LF.empty() ? "-" : LF.c_str());

      delay(1); yield();
    } while (display_.nextPage());

    ESP_LOGI(TAG_GX, "Dashboard draw done");
  }

  uint16_t width_, height_;
  int8_t sck_, mosi_, miso_;
  int8_t cs_, dc_, rst_, busy_;
  uint8_t rotation_{0};

  GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display_;
  std::vector<uint8_t> bw_;
  std::vector<uint8_t> red_;
};

}  // namespace mygxdisplay
}  // namespace esphome
