# 🐄 Cow-Stress-Tracker & Cow-Stress-Calibrator

An IoT dairy cattle locomotion (`ACT`), standing/lying posture (`Stand` / `Sleep`), leg tilt angle, and skin-surface temperature monitoring system mounted on a **single upper hind leg**. Built for the **Seeed Studio XIAO ESP32-C3**, featuring an offline real-time 50 Hz web calibration dashboard (SoftAP) and polynomial One-Time Password (OTP) authentication for secure telemetry uploads.

---

## 📁 Project Structure

```text
├── Cow-Stress-Calibrator/          # 1. Real-time 50 Hz sensor waveform & parameter tuning firmware (Offline SoftAP)
│   ├── Cow-Stress-Calibrator.ino   # Broadcasts Wi-Fi AP "Cow-Calibrator" (http://192.168.4.1)
│   ├── MotionTracker.h             # 50 Hz 3D gravity separation, hysteresis step counter & posture bout filter
│   └── WebDashboard.h              # Embedded Web UI (PROGMEM) with live dual-canvas charts & #define exporter
│
├── Cow-Stress-Tracker/             # 2. Production field tracker firmware with Deep Sleep & HTTPS OTP telemetry
│   ├── Cow-Stress-Tracker.ino      # Main firmware: WiFiManager + RTC Deep Sleep + Step/Posture tracker + API client
│   ├── OTP_Activity.h              # Polynomial modulo-prime One-Time Password (OTP) generator
│   ├── OTP_Secrets.example.h       # Template for per-device secret keys (Device IDs 1–20)
│   └── OTP_Secrets.h               # Local secret key table (excluded from Git via .gitignore)
│
├── .gitignore                      # Prevents OTP_Secrets.h and build artifacts from being committed
└── README.md                       # Project documentation
```

---

## 🛠️ Bill of Materials (Hardware List)

| # | Component | Model / Specification | Qty | Purpose |
| :-: | :--- | :--- | :-: | :--- |
| 1 | Microcontroller | **Seeed Studio XIAO ESP32-C3** (RISC-V 160 MHz, Wi-Fi 2.4 GHz) | 1 | 50 Hz sensor processing, SoftAP calibration portal, HTTPS client, and Deep Sleep management |
| 2 | IMU Sensor | **MPU6050 (GY-521)** 3-Axis Accelerometer + 3-Axis Gyroscope (I2C) | 1 | Measures leg step impact acceleration ($g$) and 3D gravitational tilt angle |
| 3 | Temperature Sensor | **DS18B20** Waterproof Probe / TO-92 (1-Wire) | 1 | Measures hind-leg skin surface temperature |
| 4 | Pull-up Resistor | **4.7 kΩ** (1/4W) | 1 | Pull-up resistor between DS18B20 `DATA (D3)` and `3V3` |
| 5 | Battery | **LiPo 1S 3.7V 600 mAh** (2.22 Wh) | 1 | Portable power supply connected to the `BAT+` / `BAT-` pads on the XIAO ESP32-C3 |
| 6 | Antenna | **2.4 GHz IPEX / U.FL Antenna** (included with XIAO ESP32-C3) | 1 | Wireless transmission (orient outward away from the cow's body) |
| 7 | Enclosure & Strap | Weatherproof compact enclosure + Velcro leg strap | 1 | Secures the sensor module to the upper hind leg |

### 📌 Pinout Mapping (Seeed Studio XIAO ESP32-C3)

| Peripheral | Peripheral Pin | XIAO ESP32-C3 Pin | Notes |
| :--- | :---: | :---: | :--- |
| **MPU6050** | `VCC` | `3V3` | 3.3V power supply |
| **MPU6050** | `GND` | `GND` | Common ground |
| **MPU6050** | `SDA` | **`D5`** (`GPIO7`) | I2C Data |
| **MPU6050** | `SCL` | **`D4`** (`GPIO6`) | I2C Clock |
| **DS18B20** | `VCC` (Red) | `3V3` | 3.3V power supply |
| **DS18B20** | `GND` (Black) | `GND` | Common ground |
| **DS18B20** | `DQ / DATA` (Yellow) | **`D3`** (`GPIO5`) | Requires a 4.7 kΩ pull-up resistor to `3V3` |
| **Config Button** | Onboard `BOOT` Button | **`GPIO9`** | Press within 3 seconds of boot/reset to open the WiFiManager portal |

---

## 🧠 Core Algorithms & Scientific Principles

The system samples 3-axis acceleration $(a_x, a_y, a_z)$ at a constant **50 Hz (every 20 ms)** using non-blocking temperature conversion (`setWaitForConversion(false)`) so no locomotion impulses are missed.

### 1. 3D Gravity Separation & Orientation Independence
Because a leg-mounted enclosure is oriented vertically or at an angle along the hind-leg muscle rather than lying flat horizontally, an Exponential Moving Average (EMA) low-pass filter ($\alpha_g = 0.04$) continuously isolates the 3D gravity vector $\vec{g} = (g_x, g_y, g_z)$ from dynamic movement acceleration:

$$\vec{g}_t = (1 - \alpha_g)\vec{g}_{t-1} + \alpha_g \vec{a}_t$$

$$a_{\mathrm{dyn}} = \sqrt{(a_x - g_x)^2 + (a_y - g_y)^2 + (a_z - g_z)^2}$$

- **360° Orientation-Free Step Detection**: Subtracting all 3 gravity components before computing the Euclidean magnitude makes $a_{\mathrm{dyn}}$ invariant to whether the enclosure is mounted vertically, diagonally, or upside-down.

### 2. 3D Dot-Product Leg Tilt & Asymmetric Transition Delays
Instead of relying on a single axis (which suffers from gimbal lock when mounted vertically), the hind-leg tilt angle ($\theta_{\mathrm{tilt}}$) is computed via the 3D dot product between the current gravity vector $\vec{g} = (g_x, g_y, g_z)$ and the calibrated standing reference vector $\vec{r} = (r_x, r_y, r_z)$ stored in `REF_GRAV_X`, `REF_GRAV_Y`, and `REF_GRAV_Z`:

$$\theta_{\mathrm{tilt}} = \arccos\left(\frac{\vec{g} \cdot \vec{r}}{\lVert\vec{g}\rVert \lVert\vec{r}\rVert}\right) \times \frac{180^\circ}{\pi}$$

- **Dual-Threshold Posture Hysteresis & Bout Filtering**:
  - **Standing → Lying**: When $\theta_{\mathrm{tilt}} \ge \theta_{\mathrm{lie}}$ (`LIE_ANGLE_DEG`, default `45°`) continuously for **`STAND_TO_LIE_DELAY_SEC`**, the state transitions to **Lying (`Sleep = 1, Stand = 0`)**.
  - **Lying → Standing**: When $\theta_{\mathrm{tilt}} \le \theta_{\mathrm{stand}}$ (`STAND_ANGLE_DEG`, default `30°`) continuously for **`LIE_TO_STAND_DELAY_SEC`**, the state transitions back to **Standing (`Stand = 1, Sleep = 0`)**.
  - *Scientific References*: Ledgerwood et al. (2010) and Ito et al. (2009), *Journal of Dairy Science*.

### 3. Posture-Gated Hysteresis Step Counting
- **Lying Lockout**: Whenever the cow is in the lying state (`last_stand == 0`), step counting is **100% locked out**, preventing false step counts from leg twitches or fly-swatting kicks while resting.
- **Two-Level Hysteresis Peak Detector**:
  1. A step is triggered when standing and smoothed dynamic acceleration crosses **`STEP_TRIGGER_G`** (default `0.35 g`).
  2. Subsequent peaks within **`STEP_DELAY_MS`** (default `500 ms`) are ignored to avoid double-counting the lift and hoof-strike phases of a single stride.
  3. The signal must drop below **`STEP_RESET_G`** (default `0.15 g`) to re-arm the detector for the next step.
  - *Scientific References*: Alsaaod et al. (2015), Chapinal et al. (2011), and Flower et al. (2005).

### 4. Polynomial Modulo-Prime OTP Authentication
In [`OTP_Activity.h`](Cow-Stress-Tracker/OTP_Activity.h), each HTTPS request is authenticated using a time-based One-Time Password derived from the 10-digit NTP Unix timestamp and the device's secret parameters (`code1`, `code2`, `prime`) via Lagrange polynomial interpolation over $\mathbb{Z}_p$:

$$\mathrm{OTP} = \left( \sum_{i=1}^{4} y_i \cdot a_i \cdot b_i^{-1} \bmod p \right) \bmod p$$

---

## 🔐 Protecting Device API Keys (`OTP_Secrets.h`)

To keep the secret `DEVICE_TABLE` (`code1`, `code2`, `prime` for Devices 1–20) out of public Git history while allowing the repository to compile out of the box:

1. **[`OTP_Activity.h`](Cow-Stress-Tracker/OTP_Activity.h)** (tracked in Git) contains only the OTP math functions and conditionally includes `OTP_Secrets.h`:
   ```cpp
   #if __has_include("OTP_Secrets.h")
     #include "OTP_Secrets.h"
   #else
     // Falls back to dummy keys so fresh clones compile cleanly
   #endif
   ```
2. **[`OTP_Secrets.h`](Cow-Stress-Tracker/OTP_Secrets.h)** (ignored by [`.gitignore`](.gitignore)) stores the real production keys locally on your machine.
3. **Setting up on a new machine**:
   ```bash
   cp Cow-Stress-Tracker/OTP_Secrets.example.h Cow-Stress-Tracker/OTP_Secrets.h
   ```
   Then populate `Cow-Stress-Tracker/OTP_Secrets.h` with your real device keys.

---

## 🔋 Battery Life Analysis (600 mAh 1S 3.7V / 2.2 Wh LiPo)

### 1. Component Current Consumption Breakdown
- **MPU6050 Module (GY-521)**: Continuous draw of **~5.5 mA** (~3.6 mA sensor core + ~1.9 mA onboard LED & LDO).
- **DS18B20 Temperature Sensor**: **~0.2 mA** average (non-blocking conversion).
- **Seeed Studio XIAO ESP32-C3**:
  - **Deep Sleep Mode**: 0.044 mA MCU + peripherals = **~5.6 mA total**
  - **Active 50 Hz Sampling (Wi-Fi Off)**: ~24 mA MCU + peripherals = **~30.0 mA total**
  - **Wi-Fi Connect + NTP Sync + HTTPS GET (`WIFI_POWER_8_5dBm`)**: **~90 – 105 mA total**

### 2. Estimated Runtime by Operating Scenario

| Scenario | Operating Behavior & Duty Cycle | Average Current ($I_{\mathrm{avg}}$) | Theoretical Full Capacity (600 mAh) | Practical Usable Capacity (520 mAh before 3.35V cutoff) |
| :--- | :--- | :---: | :---: | :---: |
| **Best Case**<br>*(Maximum power saving)* | Cow resting/standing still continuously in `bigSleepMode` (10s sample → 5s fast Wi-Fi upload → 30s deep sleep) with strong Wi-Fi RSSI | **~20.0 mA** | **~30.0 Hours**<br>*(1 Day 6 Hrs)* | **~26.0 Hours**<br>*(1 Day 2 Hrs)* |
| **Common Case**<br>*(Typical dairy farm day)* | 24h cow activity profile: 60% resting (`bigSleepMode` 30s sleep) + 25% intermittent movement (5s short sleep) + 15% continuous walking | **~30.2 mA** | **~19.9 Hours** | **~17.2 Hours** |
| **Worst Case (Tracker)**<br>*(Weak Wi-Fi + frequent stops)* | Frequent start/stop movement (5s sleep cycles) **and** weak Wi-Fi signal causing 15s connection timeouts before sleep | **~63.4 mA** | **~9.5 Hours** | **~8.2 Hours** |
| **Worst Case (Calibrator)**<br>*(Continuous SoftAP streaming)* | Running `Cow-Stress-Calibrator` with continuous Wi-Fi SoftAP + 50 Hz live web dashboard (no Deep Sleep) | **~110.0 mA** | **~5.5 Hours** | **~4.7 Hours** |

---

## 🚀 Quick Start Guide

### Step 1: Field Tuning with `Cow-Stress-Calibrator`
1. Flash [`Cow-Stress-Calibrator/Cow-Stress-Calibrator.ino`](Cow-Stress-Calibrator/Cow-Stress-Calibrator.ino) to the XIAO ESP32-C3.
2. Strap the enclosure onto the cow's **upper hind leg**.
3. Connect your phone or laptop to the Wi-Fi network **`Cow-Calibrator`** (Password: **`12345678`**) and open **`http://192.168.4.1`**.
4. While the cow is standing naturally, tap **`ตั้งท่ายืนปัจจุบันเป็น 0° (Calibrate)`** to record the 3D standing reference gravity vector (`REF_GRAV_X/Y/Z`).
5. Adjust the sliders (or click any numeric value to type directly) while observing the live waveforms, then copy the generated `#define` block at the bottom of the page.

### Step 2: Deploying `Cow-Stress-Tracker`
1. Paste the calibrated `#define` constants at the top of [`Cow-Stress-Tracker/Cow-Stress-Tracker.ino`](Cow-Stress-Tracker/Cow-Stress-Tracker.ino) (lines 16–27).
2. Ensure `Cow-Stress-Tracker/OTP_Secrets.h` is present with your device keys, then upload the sketch.
3. **Configuring Wi-Fi & Device ID (1–20)**:
   - By default, the tracker connects to `TP-Link_0A08` (`84940339`) using `Device ID = 1`.
   - To change Wi-Fi credentials or the `Device ID` in the field: press `RESET` and hold the onboard **`BOOT`** button within 3 seconds. Connect to the **`Cow-Stress-Tracker-Config`** AP and configure at `http://192.168.4.1`.
