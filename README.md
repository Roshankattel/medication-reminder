# Medication Reminder

An ESP32-based smart medication reminder system that alerts patients at scheduled times, detects when medication is taken via a PIR motion sensor, monitors ambient temperature and humidity, and sends all telemetry to a **ThingsBoard** IoT dashboard over HTTP.

---

## Table of Contents

- [Overview](#overview)
- [Features](#features)
- [Hardware](#hardware)
  - [Bill of Materials](#bill-of-materials)
  - [Pin Mapping](#pin-mapping)
  - [Wiring Diagram](#wiring-diagram)
- [Software Stack](#software-stack)
  - [Dependencies](#dependencies)
- [Project Structure](#project-structure)
- [Configuration](#configuration)
  - [Medication Schedule](#medication-schedule)
  - [Wi-Fi Setup](#wi-fi-setup)
  - [ThingsBoard Setup](#thingsboard-setup)
- [Building & Flashing](#building--flashing)
- [Usage](#usage)
  - [First Boot / Wi-Fi Provisioning](#first-boot--wi-fi-provisioning)
  - [Normal Operation](#normal-operation)
  - [LCD Layout](#lcd-layout)
  - [Help Button](#help-button)
- [ThingsBoard Telemetry](#thingsboard-telemetry)
- [Troubleshooting](#troubleshooting)
- [Security Notes](#security-notes)
- [License](#license)

---

## Overview

The device wakes up on a fixed medication schedule, sounds a buzzer, and lights an LED to remind the patient to take their medicine. A PIR sensor detects movement near the pill box — taken as a proxy for "medication consumed" — and silences the alert while logging the event to the cloud. An ambient sensor continuously tracks room temperature and humidity to flag conditions unsuitable for medication storage.

A dedicated **help button** lets the patient send an instant alert to caregivers through the ThingsBoard backend.

---

## Features

| Feature | Details |
|---|---|
| Scheduled alerts | Up to N configurable hourly slots; buzzer + LED per dose |
| Motion-based confirmation | PIR detects when patient picks up medication; auto-dismisses alert |
| Ambient monitoring | DHT22 reads temperature & humidity every 60 s; warns on out-of-range values |
| Help button | Long-press sends `help_request` to backend; all LEDs + buzzer fire |
| Cloud telemetry | HTTP POST to ThingsBoard for every event (temp, dose taken, help) |
| Wi-Fi provisioning | WiFiManager captive portal — no need to hard-code credentials |
| Real-time clock | DS3231 keeps accurate time independent of network |
| 20×4 LCD | Live display of time, next dose, temperature / humidity, and status messages |

---

## Hardware

### Bill of Materials

| Component | Specification |
|---|---|
| Microcontroller | ESP32 (ESP-WROVER-KIT or compatible WROVER module) |
| Real-Time Clock | DS3231 module (I2C) |
| Display | 20×4 I2C LCD (PCF8574 backpack, address `0x27`) |
| Humidity / Temperature | DHT22 sensor |
| Motion Sensor | HC-SR501 PIR module |
| Buzzer | 5 V active buzzer |
| LEDs | 3 × LEDs with 220 Ω current-limiting resistors |
| Button | Tactile push-button (pull-up, active LOW) |
| Power | 5 V / 2 A USB or regulated supply |

### Pin Mapping

| Signal | GPIO | Direction | Notes |
|---|---|---|---|
| Buzzer | 13 | Output | Active HIGH |
| DHT22 data | 32 | Input | 1-wire, 10 kΩ pull-up recommended |
| PIR output | 27 | Input | Active HIGH when motion detected |
| Button | 18 | Input (PULLUP) | LOW = pressed; also triggers Wi-Fi portal at boot |
| LED 1 (Dose 1) | 26 | Output | Active HIGH |
| LED 2 (Dose 2) | 25 | Output | Active HIGH |
| LED 3 (Dose 3) | 33 | Output | Active HIGH |
| LCD SDA | 21 | I2C | Shared with RTC |
| LCD SCL | 22 | I2C | Shared with RTC |
| RTC SDA | 21 | I2C | Shared with LCD |
| RTC SCL | 22 | I2C | Shared with LCD |

> **Note:** The ESP-WROVER-KIT exposes all GPIO headers on the board edge. Check your specific board's silkscreen for pin locations.

### Wiring Diagram

```
                          ESP32 (WROVER)
                        ┌──────────────┐
              DHT22 ────┤ GPIO32       │
               PIR  ────┤ GPIO27       │
            Button  ────┤ GPIO18       │  ←── GND (active LOW)
            Buzzer  ────┤ GPIO13       │
             LED 1  ────┤ GPIO26       │  ──► 220Ω ──► LED ──► GND
             LED 2  ────┤ GPIO25       │  ──► 220Ω ──► LED ──► GND
             LED 3  ────┤ GPIO33       │  ──► 220Ω ──► LED ──► GND
          SDA (I2C) ────┤ GPIO21       │  ──► LCD + DS3231
          SCL (I2C) ────┤ GPIO22       │  ──► LCD + DS3231
                        └──────────────┘
```

---

## Software Stack

- **Framework:** Arduino (via PlatformIO)
- **Platform:** `espressif32 @ 6.3.2`
- **Board target:** `esp-wrover-kit`
- **IDE:** PlatformIO (VS Code extension or CLI)

### Dependencies

Declared in `platformio.ini` and resolved automatically by PlatformIO:

| Library | Purpose |
|---|---|
| `LiquidCrystal_I2C` | Drive the 20×4 I2C LCD |
| `Adafruit Unified Sensor` | Base sensor abstraction for DHT |
| `DHT sensor library` | DHT22 temperature/humidity readings |
| `Adafruit BusIO` | I2C/SPI bus abstraction (DHT dependency) |
| `RTClib` | DS3231 real-time clock driver |
| `WiFiManager @ 2.0.16-rc.2` | Wi-Fi captive portal provisioning |
| `WiFi` *(built-in)* | ESP32 Wi-Fi stack |
| `HTTPClient` *(built-in)* | HTTP POST to ThingsBoard |

---

## Project Structure

```
medication-reminder/
├── src/
│   └── main.cpp          # All application firmware (single-file sketch)
├── include/
│   └── README            # PlatformIO placeholder (no custom headers yet)
├── lib/
│   └── README            # PlatformIO placeholder (no private libraries)
├── test/
│   └── README            # PlatformIO placeholder (no unit tests yet)
├── platformio.ini         # Build configuration, board, and library deps
├── .gitignore
└── README.md             # This file
```

---

## Configuration

All configuration lives at the top of `src/main.cpp`.

### Medication Schedule

```cpp
uint8_t mediTime[] = {13, 14, 15}; // hours in 24-hour format
uint8_t ledPins[]  = {26, 25, 33}; // one LED per dose slot
```

Edit `mediTime` to set the reminder hours. The array length determines how many doses exist; `ledPins` must have the same number of entries.

### Wi-Fi Setup

```cpp
#define AP_SSID "Medical-Kit-1"
#define AP_PASS "123456789"
```

These credentials are used for the **captive portal access point** that the device creates when provisioning Wi-Fi. The saved home-network credentials are stored in ESP32 NVMe flash by WiFiManager and survive reboots. To clear saved credentials and re-provision, hold the button down at power-on.

### ThingsBoard Setup

```cpp
#define TOKEN "YOUR_DEVICE_TOKEN_HERE"
#define URL   "http://<THINGSBOARD_HOST>:8080/api/v1/" TOKEN "/telemetry"
```

Replace `YOUR_DEVICE_TOKEN_HERE` with the access token from your ThingsBoard device and set the host IP/hostname. See [ThingsBoard telemetry](#thingsboard-telemetry) for the full setup walkthrough.

> **Important:** Do not commit real tokens to a public repository. Consider extracting secrets to a separate header file that is listed in `.gitignore`.

---

## Building & Flashing

### Prerequisites

1. Install [PlatformIO Core](https://docs.platformio.org/en/latest/core/installation/index.html) or the [PlatformIO IDE extension](https://platformio.org/install/ide?install=vscode) for VS Code.
2. Connect the ESP-WROVER-KIT via USB.

### Build

```bash
pio run
```

### Upload

```bash
pio run --target upload
```

PlatformIO will automatically resolve and install all library dependencies on the first build.

### Monitor Serial Output

```bash
pio device monitor --baud 115200
```

Debug output is compiled in when `DEBUG=1` (set in `platformio.ini` by default).

---

## Usage

### First Boot / Wi-Fi Provisioning

1. **Hold the button** while powering on (or at reset).
2. The LCD shows the AP SSID and password:
   ```
   Please connect Wi-Fi
   SSID:Medical-Kit-1
   PASS:123456789
   ```
3. Connect your phone or laptop to the `Medical-Kit-1` Wi-Fi network.
4. A captive portal opens automatically — select your home Wi-Fi and enter its password.
5. The device saves the credentials and restarts. Future boots connect automatically.
6. The portal times out after **240 seconds**; if no network is configured, the device restarts.

### Normal Operation

After connecting to Wi-Fi the LCD shows the main status screen and the device enters its loop:

- **Every second:** Time is polled from the RTC and the clock display is refreshed.
- **Every 60 seconds:** DHT22 is read; temperature/humidity are posted to ThingsBoard.
- **On the hour (for each scheduled dose):** Buzzer sounds, the corresponding LED lights up, and the LCD prompts "Pls take Medicine:N".
- **When motion is detected (PIR HIGH) during an active reminder:** Buzzer and LED are silenced, the LCD shows "Medicine N Taken", and a `med_taken` event is posted to ThingsBoard.

### LCD Layout

```
┌────────────────────┐
│WiFi           HH:MM│  ← Row 0: Connection status + live time
│                    │  ← Row 1: Active alert or help message
│T:22C,H:55%-> Good  │  ← Row 2: Latest temperature / humidity reading
│Next Medicine: 15:00│  ← Row 3: Upcoming dose time
└────────────────────┘
```

Condition labels on row 2:

| Label | Meaning |
|---|---|
| `Good` | Temperature 15–35 °C and humidity 35–70 % |
| `Low T` | Temperature below 15 °C |
| `High T` | Temperature above 35 °C |
| `Low H` | Humidity below 35 % |
| `High H` | Humidity at or above 70 % |

### Help Button

A **long-press** (≥ 2 seconds) of the button while the device is running (not at boot) toggles the help mode:

- **Activate:** All LEDs light up, buzzer sounds, LCD shows `HELP REQUESTED`, and `{"help_request": true}` is POSTed to ThingsBoard.
- **Deactivate:** Press again — LEDs and buzzer turn off, LCD clears the message.

---

## ThingsBoard Telemetry

The device sends three types of JSON telemetry payloads via HTTP POST:

| Event | Payload | Trigger |
|---|---|---|
| Environment | `{"temperature": 22, "humidity": 55}` | Every 60 s |
| Dose taken | `{"med_taken": 1, "next_med_time": 14}` | PIR detects motion during active reminder |
| Help request | `{"help_request": true}` | Long-press of help button |

### Setting Up ThingsBoard

1. Log in to your ThingsBoard instance (Community Edition is free: [thingsboard.io](https://thingsboard.io)).
2. Go to **Devices → Add Device**, give it a name (e.g. `Medical-Kit-1`).
3. Open the device and copy the **Access Token** from the *Credentials* tab.
4. Paste the token into `#define TOKEN` in `src/main.cpp`.
5. Create a **Dashboard** and add widgets for `temperature`, `humidity`, `med_taken`, and `help_request` telemetry keys.

---

## Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| LCD shows `RTC module error!` | DS3231 not found on I2C bus | Check SDA/SCL wiring; verify I2C address (default `0x68` for DS3231) |
| LCD shows `Temp sensor error!` | DHT22 not responding | Verify GPIO 32 connection; add 10 kΩ pull-up resistor between DATA and 3.3 V |
| Device never connects to Wi-Fi | Saved credentials wrong / network changed | Hold button at boot to open captive portal and re-configure |
| HTTP POST returns -1 | Wi-Fi disconnected during operation | WiFiManager `autoConnect` will reconnect on next boot; add `wm.autoConnect()` in loop for persistent reconnect |
| Time is wrong after power loss | RTC not set | Uncomment `rtc.adjust(DateTime(__DATE__, __TIME__));` in `setup()`, flash once, then comment it out again and re-flash |
| PIR fires immediately | Sensor warm-up period | HC-SR501 needs ~30 s calibration after power-on; stay clear of the sensor during this time |
| No serial output | `DEBUG` not set | Ensure `DEBUG=1` is in `build_flags` inside `platformio.ini` |

---

## Security Notes

- **Wi-Fi credentials** are managed by WiFiManager and stored in ESP32 NVMe flash — they are not exposed in source code.
- **ThingsBoard token** and **server URL** are currently hard-coded in `src/main.cpp`. For production or shared repositories:
  - Move them to a `secrets.h` file in `include/` and add `include/secrets.h` to `.gitignore`.
  - Use ThingsBoard's MQTT + TLS for encrypted transport instead of plain HTTP.
- The captive portal password (`123456789`) is intentionally simple for ease of setup; change `AP_PASS` if operating in a shared network environment.

---

## License

This project is released under the [MIT License](https://opensource.org/licenses/MIT). Feel free to use, modify, and distribute it.
