
---

# ESPHome — Pysoon Irrigation Controller (ESP32) + Web UI

This repo now contains **two independent ESPHome setups**:

1. **Pysoon Irrigation Controller** (this section)
2. **ESP32 BLE Thermometers (ATC)** (see next section)

---

## Pysoon — Overview

An ESP32-based irrigation/dosing controller with:

* **Dual-route diverter**: BIG/SMALL outlet (1 relay)
* **Two supply valves**: IN + OUT (2 relays)
* **Main pump** (1 relay)
* **Flow meter (pulse)**–based dosing with per-pulse calibration (`mL/imp`)
* **Tail compensation** to offset overrun/inertia (configurable `tail_ml`)
* **Watchdogs/failsafes**:

  * **Valves watchdog** (shared): closes any open valve/diverter after `max_work_time_valve`
  * **Pump watchdog**: stops pump (or the whole watering session) after `max_work_time_pump`
* **Session metrics**: pulses & water per session
* **Lifetime totals in liters**: per route (BIG/SMALL) and combined
* **Web UI v3** + Wi-Fi info (SSID/IP) + native **Home Assistant** API
* **Modular packages** (in `packages_pysoon/`) for readability and reuse

> No behavior changes vs your latest config — only structured into packages and documented.

---

## Pysoon — Directory layout

```
.
├── pysoon.yaml
├── packages_pysoon/
│   ├── core.yaml
│   ├── globals.yaml
│   ├── numbers.yaml
│   ├── buttons.yaml
│   ├── status.yaml
│   ├── sensors.yaml
│   ├── switches.yaml
│   └── scripts.yaml
└── secrets.yaml        # fill with your Wi-Fi/API/OTA/Web auth
```

---

## Pysoon — Requirements

* ESPHome (CLI or HA add-on)
* ESP32 board (`esp32dev`, ESP-IDF framework)
* Flow sensor (pulse output)
* 4 relays:

  * Relay5: **Diverter** (BIG/SMALL)
  * Relay6: **Valve OUT**
  * Relay7: **Valve IN**
  * Relay8: **Pump**

---

## Pysoon — Quick start

1. **Create `secrets.yaml`** (example):

   ```yaml
   wifi1_ssid: "YOUR-SSID-1"
   wifi1_pass: "YOUR-PASS-1"
   wifi2_ssid: "YOUR-SSID-2"
   wifi2_pass: "YOUR-PASS-2"
   wifi3_ssid: "YOUR-SSID-3"
   wifi3_pass: "YOUR-PASS-3"

   ap_pass: ""                # or set a password

   api_password: "strong-api-pass"
   ota_password: "strong-ota-pass"

   web_user: "zabidok"
   web_pass: "strong-web-pass"
   ```

2. **Review substitutions** in `pysoon.yaml` (pins, timeouts, calibration):

   * Pins:

     ```
     relay_5_pin: GPIO21  # Diverter BIG/SMALL
     relay_6_pin: GPIO19  # Valve OUT
     relay_7_pin: GPIO18  # Valve IN
     relay_8_pin: GPIO05  # Pump
     flow_pin:   GPIO22   # Flow pulse input
     ```
   * Safety/timeouts:
     `max_work_time_valve: "210s"`, `max_work_time_pump: "200s"`
   * Dosing:

     * `default_ml_per_pulse: "0.182"`
     * `tail_ml: "5.0"` (overrun compensation)
     * `valve_delay_ms: "1000"` (pump→valves stop gap)

3. **Flash & run**:

   ```bash
   esphome run pysoon.yaml
   ```

4. Open the **web UI** (from HA device page or by IP). You’ll see **Wi-Fi SSID/IP**, route label (BIG/SMALL), and entities in HA.

---

## Pysoon — Exported entities (HA)

* **Numbers (config)**:

  * `ML per Pulse` (mL/imp)
  * `Dose Target` (mL)
  * `Dose Big` (mL)
  * `Dose Small` (mL)
* **Buttons**:

  * `Start Watering`, `Stop Watering`
  * `Dose Test` (dose current route by `Dose Target`)
  * `Dose Both` (BIG by `Dose Big` → SMALL by `Dose Small`)
  * `Route → Big Plant`, `Route → Small Plant`
* **Binary sensors**:

  * `Watering Active`
  * `Valve Outlet Status` (diverter), `Valve IN Status`, `Valve OUT Status`, `Pump Status`
* **Sensors**:

  * `Session Pulses` (imp), `Session Water` (mL)
  * `Water Big` (L), `Water Small` (L), `Water Total` (L)
  * `WiFi Signal`, `Uptime`
* **Text sensors**:

  * `Route` (BIG/SMALL)
  * `WiFi SSID`, `IP Address`

> Session metrics are published frequently while watering; lifetime totals update on boot and when watering stops.

---

## Pysoon — Notes & tips

* **Tail compensation** (`tail_ml`) accounts for inertia (overrun after pump off). If you see consistent +X mL, set `tail_ml ≈ X`.
* **Watchdogs**:

  * `valves_watchdog`: shared for diverter/IN/OUT; stops after a global timeout if any of them stay on.
  * `pump_watchdog`: turns pump off; if a session is active, it calls `stop_watering`.
* **Diverter lock** while watering: route cannot be changed mid-session (prevents cross-watering).
* **Home Assistant UI**:

  * Put Numbers + Buttons on an Entities card.
  * Add the totals (L) to a History graph card.
  * Optionally, make a quick “Dose Both” button in a dedicated tile.

---

## Pysoon — Troubleshooting

* **Over/under dosing** → verify `ML per Pulse` calibration and tweak `tail_ml`.
* **Unexpected watchdog stops** → increase `max_work_time_valve/pump` slightly.
* **No pulses** → confirm `flow_pin` pull-up and signal wiring; check `internal_filter` and sensor datasheet limits.
* **Route label wrong after reboot** → it’s published on boot; if diverter relay is mechanically reversed, flip logic or wiring.

---

# ESPHome — ESP32 BLE Thermometers (ATC) + Web UI

A clean, modular ESPHome setup for ESP32 that:

* reads temperature/humidity/battery from **ATC MiThermometer** beacons (e.g., LYWSD03MMC flashed with ATC/PVVX firmware),
* serves a tiny **web UI** with the current Wi-Fi SSID and IP,
* exposes sensors to **Home Assistant** via native API,
* is organized into reusable **packages** for easy scaling.

> ⚠️ No behavior changes from stock ESPHome were introduced in this repo. The comments suggest optional tweaks you can enable later.

---

## Features

* **Modular packages**

  * `packages/base.yaml`: logging, API, OTA, web server, SNTP time.
  * `packages/wifi.yaml`: Wi-Fi networks + Wi-Fi info text sensors (SSID/IP).
  * `packages/ble_atc_pack.yaml`: a reusable pack for one ATC sensor (temp/hum/batt + “last update” timestamp).
* **Web UI** (async web_server v3) with SSID/IP visible.
* **Time-aware**: SNTP with timezone for correct timestamps.
* **Low-noise defaults**: throttled sensor updates to reduce DB churn.
* **Scalable**: add more ATC sensors with a one-liner each.

---

## Directory layout

```
.
├── packages/
│   ├── base.yaml
│   ├── wifi.yaml
│   └── ble_atc_pack.yaml
└── temp.yaml
```

---

## Requirements

* ESPHome (CLI or inside Home Assistant)
* ESP32 board (configured as `esp32dev`, ESP-IDF framework)
* ATC/PVVX-flashed BLE thermometers broadcasting **ATC** format

---

## Quick start

1. **Clone** the repo and open the folder.

2. **Create** a `secrets.yaml` next to your YAMLs (or use your global secrets):

   ```yaml
   wifi_ssid_one: "YOUR-SSID-1"
   wifi_ssid_one_pass: "YOUR-PASS-1"
   wifi_ssid_two: "YOUR-SSID-2"
   wifi_ssid_two_pass: "YOUR-PASS-2"
   wifi_ssid_three: "YOUR-SSID-3"
   wifi_ssid_three_pass: "YOUR-PASS-3"
   wifi_ssid_four: "YOUR-SSID-4"
   wifi_ssid_four_pass: "YOUR-PASS-4"

   ap_pass: "fallback-ap-password"

   # Optional hardening (uncomment in YAMLs too):
   # api_key: "32+ chars"
   # ota_pass: "set-a-strong-password"

   # Optional static IP:
   # ip_static: 192.168.1.50
   # ip_gateway: 192.168.1.1
   # ip_subnet: 255.255.255.0

   # Optional web auth:
   # web_user: "admin"
   # web_pass: "strong-password"
   ```

3. **Edit** `temp.yaml` if needed:

   * Keep `substitutions.name: temp` (or change per node).
   * Update the MAC addresses in the `vars` blocks to match your sensors.

4. **Flash & run**:

   ```bash
   esphome run temp.yaml
   ```

5. Open the device **web UI** — SSID & IP should be visible; sensors should appear in HA.

---

## Adding more sensors

Each ATC sensor is a **pack instance**. Duplicate one block in `temp.yaml` and adjust:

```yaml
ble_bedroom: !include
  file: packages/ble_atc_pack.yaml
  vars: { room: bedroom, mac: "AA:BB:CC:DD:EE:FF" }
```

Sensors exposed:

* `bedroom temp` (°C), `bedroom hum` (%), `bedroom batt` (%)
* `bedroom update` (ISO-8601 timestamp text_sensor)

---

## Adding another node

Create a new top-level file (e.g., `livingroom.yaml`):

```yaml
substitutions:
  name: livingroom

esp32_ble_tracker:
  scan_parameters:
    active: false
    interval: 320ms
    window: 30ms

logger:
  logs:
    atc_mithermometer: WARN

packages:
  base:  !include packages/base.yaml
  wifi:  !include packages/wifi.yaml
  # add ble_* packs here...
```

Flash with:

```bash
esphome run livingroom.yaml
```

---

## Configuration notes

### Wi-Fi

* Add up to four networks in `packages/wifi.yaml`.
* Optionally enable **static IP** by uncommenting `manual_ip`.
* Wi-Fi info sensors:

  * `WiFi SSID` (text_sensor), `IP` (text_sensor)

### Time

* SNTP is enabled in `packages/base.yaml` with `timezone: Europe/Sofia`.

### Security (recommended)

* In `packages/base.yaml`, uncomment:

  * `api.encryption.key: !secret api_key`
  * `ota.password: !secret ota_pass`
  * `web_server.auth` (username/password)
* Change the fallback AP password (`ap_pass`) from the default.

---

## Performance & memory tips (optional)

* Lower logs:

  ```yaml
  logger:
    level: INFO
  ```
* Reduce DB churn:

  ```yaml
  temperature:
    filters: [ { throttle: 60s }, { delta: 0.2 } ]
  humidity:
    filters: [ { throttle: 60s }, { delta: 1.0 } ]
  ```
* Wi-Fi power save:

  ```yaml
  wifi:
    power_save_mode: LIGHT
  ```

---

## Troubleshooting

* **No sensors in HA** → `esphome logs temp.yaml`, same subnet, encryption keys match.
* **Web UI not loading** → check IP/mDNS (`http://<node>.local/`), lower log level.
* **Wrong time** → verify timezone, Internet for NTP.
* **No BLE readings** → ATC format broadcasting, tracker passive, avoid RF noise.
* **Frequent reconnects** → try static IP, check Wi-Fi signal.

---

## Customization ideas

* Add `delta` filters & `accuracy_decimals` to cut DB writes.
* “Heartbeat” battery publishes.
* Per-room Lovelace dashboards.
* More device packs with the same structure.

---
