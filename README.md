# ESPHome — ESP32 BLE Thermometers (ATC) + Web UI

A clean, modular ESPHome setup for ESP32 that:
- reads temperature/humidity/battery from **ATC MiThermometer** beacons (e.g., LYWSD03MMC flashed with ATC/PVVX firmware),
- serves a tiny **web UI** with the current Wi-Fi SSID and IP,
- exposes sensors to **Home Assistant** via native API,
- is organized into reusable **packages** for easy scaling.

> ⚠️ No behavior changes from stock ESPHome were introduced in this repo. The comments suggest optional tweaks you can enable later.

---

## Features

- **Modular packages**
  - `packages/base.yaml`: logging, API, OTA, web server, SNTP time.
  - `packages/wifi.yaml`: Wi-Fi networks + Wi-Fi info text sensors (SSID/IP).
  - `packages/ble_atc_pack.yaml`: a reusable pack for one ATC sensor (temp/hum/batt + “last update” timestamp).
- **Web UI** (async web_server v3) with SSID/IP visible.
- **Time-aware**: SNTP with timezone for correct timestamps.
- **Low-noise defaults**: throttled sensor updates to reduce DB churn.
- **Scalable**: add more ATC sensors with a one-liner each.

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

- ESPHome (CLI or inside Home Assistant)
- ESP32 board (configured as `esp32dev`, ESP-IDF framework)
- ATC/PVVX-flashed BLE thermometers broadcasting **ATC** format

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
   - Keep `substitutions.name: temp` (or change per node).
   - Update the MAC addresses in the `vars` blocks to match your sensors.

4. **Flash & run** (choose one):
   - **ESPHome CLI**
     ```bash
     esphome run temp.yaml
     ```
   - **Home Assistant → ESPHome add-on**
     - Add a new device from `temp.yaml` and install.

5. Open the device **web UI** (from HA or by IP) — SSID & IP should be visible; sensors should appear in HA.

---

## Adding more sensors

Each ATC sensor is a **pack instance**. Duplicate one block in `temp.yaml` and adjust:

```yaml
ble_bedroom: !include
  file: packages/ble_atc_pack.yaml
  vars: { room: bedroom, mac: "AA:BB:CC:DD:EE:FF" }
```

Sensors exposed:
- `bedroom temp` (°C), `bedroom hum` (%), `bedroom batt` (%)
- `bedroom update` (ISO-8601 timestamp text_sensor)

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
- Add up to four networks in `packages/wifi.yaml`.
- Optionally enable **static IP** (faster reconnects) by uncommenting `manual_ip`.
- Wi-Fi info sensors:
  - `WiFi SSID` (text_sensor)
  - `IP` (text_sensor)
  - Mark them `internal: true` if you don’t want them in HA (still visible in Web UI).

### Time
- SNTP is enabled in `packages/base.yaml` with `timezone: Europe/Sofia`.
- The “last update” timestamp uses local time but formats as ISO string. You can switch to including the local offset if you prefer (see comment in code).

### Security (recommended)
- In `packages/base.yaml`, uncomment:
  - `api.encryption.key: !secret api_key`
  - `ota.password: !secret ota_pass`
  - `web_server.auth` (username/password)
- Change the fallback AP password (`ap_pass`) from the default.

---

## Performance & memory tips (optional)

> These are **off by default** to keep behavior unchanged. Enable only if you want the trade-offs.

- Lower log verbosity in `packages/base.yaml`:
  ```yaml
  logger:
    level: INFO
    # baud_rate: 0   # disable UART logs to free the serial port & a bit of CPU
  ```
- Reduce DB noise by publishing only meaningful changes (in `packages/ble_atc_pack.yaml`):
  ```yaml
  temperature:
    filters:
      - throttle: 60s
      - delta: 0.2       # publish only if Δ≥0.2 °C
    # accuracy_decimals: 1
  humidity:
    filters:
      - throttle: 60s
      - delta: 1.0
  ```
- Wi-Fi power saving (may add latency to web/API):
  ```yaml
  wifi:
    power_save_mode: LIGHT
  ```
- If the device is close to the AP:
  ```yaml
  wifi:
    output_power: 17dB
  ```

---

## Troubleshooting

- **No sensors in Home Assistant**
  - Check logs: `esphome logs temp.yaml`
  - Ensure the device and HA are on the same subnet.
  - If using API encryption, the key must match in HA.

- **Web UI not loading**
  - Confirm the IP in your router or via mDNS: `http://temp.local/` (replace with your node name).
  - Try reducing log level or disabling heavy logs.

- **Wrong time / timestamps**
  - Verify `timezone` in `packages/base.yaml`.
  - Ensure the device has Internet access to reach NTP servers.

- **No BLE readings**
  - Make sure your thermometers broadcast **ATC** format (ATC/PVVX firmware).
  - Keep `esp32_ble_tracker` **passive** (as configured) for stability.
  - Avoid placing the ESP32 too close to strong 2.4 GHz interference sources.

- **Frequent reconnects**
  - Consider enabling a static IP (`manual_ip`).
  - Check Wi-Fi signal quality; reduce `output_power` only if you are close to the AP.

---

## Customization ideas (opt-in)

- Add `delta` filters & `accuracy_decimals` to cut database writes.
- Periodic battery heartbeat (e.g., every 30 min) to verify the sensor is alive.
- Per-room dashboards in HA using these entities.
- Additional device packs reusing the same structure (CO₂ sensors, relays, etc.).

---
