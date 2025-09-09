// custom_epd.h
#pragma once

#include <vector>
#include <cstring>

#include "esphome.h"
#include "esphome/core/log.h"
#include "esphome/components/display/display_buffer.h"

#include <GxEPD2_3C.h>
#include <GxEPD2_420c.h>   // 4.2" 400x300 B/W/Red
#include <SPI.h>

static const char *const TAG_GX = "gxepd2_custom";

// ==== Класс триколорного дисплея под ESPHome ====
class MyGxDisplay : public esphome::PollingComponent, public esphome::display::DisplayBuffer {
 public:
  // width/height — логический размер буфера (обычно 400x300 для 4.2")
  // sck/mosi/miso — пины VSPI; cs/dc/rst/busy — как в твоей конфигурации
  MyGxDisplay(uint16_t width, uint16_t height,
              int8_t sck, int8_t mosi, int8_t miso,
              int8_t cs, int8_t dc, int8_t rst, int8_t busy)
      : esphome::PollingComponent(ULONG_MAX) // отключаем авто-апдейты; обновляем вручную
      , esphome::display::DisplayBuffer(width, height)
      , sck_(sck), mosi_(mosi), miso_(miso), cs_(cs), dc_(dc), rst_(rst), busy_(busy)
      , display_(GxEPD2_420c(cs, dc, rst, busy)) {
    const size_t bytes = ((width + 7) / 8) * height;
    bw_.assign(bytes, 0x00);
    red_.assign(bytes, 0x00);
  }

  // ===== ESPHome lifecycle =====
  void setup() override {
    ESP_LOGI(TAG_GX, "Init SPI and GxEPD2 (4.2in tri-color) ...");
    // Инициализируем VSPI под твои пины
    SPI.end();                   // на всякий
    SPI.begin(sck_, miso_, mosi_, cs_);

    display_.init(115200);       // debug speed на UART
    display_.setRotation(0);
    display_.setFullWindow();

    // Первый чистый белый кадр
    full_white_();
  }

  // Вызывается при component.update: epd
  void update() override {
    flush_buffers_to_panel_();
  }

  // ===== DisplayBuffer hooks =====
  // установка одного пикселя в наши 2 буфера (ч/б и красный)
  void draw_absolute_pixel_internal(int x, int y, esphome::Color color) override {
    if (x < 0 || y < 0 || x >= this->get_width_internal() || y >= this->get_height_internal()) return;

    const uint32_t idx = (uint32_t)y * this->get_width_internal() + (uint32_t)x;
    const uint32_t byte_index = idx >> 3;                 // /8
    const uint8_t  bit_mask   = 0x80 >> (idx & 0x7);      // MSB first

    const bool is_black = (color.r == 0 && color.g == 0 && color.b == 0);        // Color::BLACK
    const bool is_red   = (color.r > 0  && color.g == 0 && color.b == 0);        // Color::RED

    if (is_black) {
      bw_[byte_index]  |=  bit_mask;   // 1 = «есть черный»
      red_[byte_index] &= ~bit_mask;   // убираем красный в этом пикселе
    } else if (is_red) {
      red_[byte_index] |=  bit_mask;   // 1 = «есть красный»
      bw_[byte_index]  &= ~bit_mask;   // убираем черный
    } else {
      // белый — нули в обоих буферах
      bw_[byte_index]  &= ~bit_mask;
      red_[byte_index] &= ~bit_mask;
    }
  }

  // заливка всего кадра одним цветом
  void fill_internal(esphome::Color color) override {
    const bool is_black = (color.r == 0 && color.g == 0 && color.b == 0);
    const bool is_red   = (color.r > 0  && color.g == 0 && color.b == 0);

    if (is_black) {
      std::memset(bw_.data(),  0xFF, bw_.size());
      std::memset(red_.data(), 0x00, red_.size());
    } else if (is_red) {
      std::memset(red_.data(), 0xFF, red_.size());
      std::memset(bw_.data(),  0x00, bw_.size());
    } else { // белый
      std::memset(bw_.data(),  0x00, bw_.size());
      std::memset(red_.data(), 0x00, red_.size());
    }
  }

  // по желанию — смена поворота из кода
  void set_panel_rotation(uint8_t r) {
    rotation_ = (uint8_t)(r % 4);
    display_.setRotation(rotation_);
  }

 protected:
  // ===== Helpers =====
  void full_white_() {
    display_.setFullWindow();
    display_.firstPage();
    do {
      display_.fillScreen(GxEPD_WHITE);
    } while (display_.nextPage());
  }

  // Выгружаем наши RAM-буферы на панель.
  // Важно: используем drawInvertedBitmap, т.к. в e-paper «0» часто трактуется как «краска включена».
  void flush_buffers_to_panel_() {
    const int W = this->get_width_internal();
    const int H = this->get_height_internal();

    display_.setRotation(rotation_);
    display_.setFullWindow();

    display_.firstPage();
    do {
      display_.fillScreen(GxEPD_WHITE);

      // ЧЕРНЫЙ слой
      display_.drawInvertedBitmap(
        /*x*/ 0, /*y*/ 0,
        bw_.data(),
        W, H,
        GxEPD_BLACK
      );

      // КРАСНЫЙ слой
      display_.drawInvertedBitmap(
        /*x*/ 0, /*y*/ 0,
        red_.data(),
        W, H,
        GxEPD_RED
      );

    } while (display_.nextPage());

    ESP_LOGD(TAG_GX, "Full refresh done.");
  }

  // ===== Поля =====
  int8_t sck_, mosi_, miso_;
  int8_t cs_, dc_, rst_, busy_;
  uint8_t rotation_{0};

  // Сам драйвер панели
  GxEPD2_3C<GxEPD2_420c, GxEPD2_420c::HEIGHT> display_;

  // Два буфера 1bpp: бит=1 означает «этот пиксель активен в данном слое»
  std::vector<uint8_t> bw_;
  std::vector<uint8_t> red_;
};
