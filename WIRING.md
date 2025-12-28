# 🔌 Wiring Guide - ESP32-CAM Video Streaming

This guide shows how to connect the hardware components for the ESP32-CAM streaming project.

---

## 📸 Complete Wiring Diagram

![Wiring Diagram](images/wiring.jpg)

---

## 🔧 Pin Connections

### OLED Display SSD1306 → ESP32-CAM

| OLED Pin | ESP32-CAM Pin | Description |
|----------|---------------|-------------|
| VCC      | 5V            | Power supply |
| GND      | GND           | Ground |
| SDA      | GPIO 14 (IO14) | I2C Data |
| SCL      | GPIO 15 (IO15) | I2C Clock |

### Power Supply

| Component | Connection |
|-----------|------------|
| 5V Power Supply | ESP32-CAM 5V pin |
| GND | ESP32-CAM GND pin |

---

## ⚠️ Important Notes

### I2C Pins
- **GPIO 14** and **GPIO 15** are used for I2C communication with the OLED display
- These pins are not used by the camera module, so there's no conflict
- Make sure to use pull-up resistors if your OLED module doesn't have them built-in (most SSD1306 modules include them)

### Power Requirements
- The ESP32-CAM requires a stable **5V power supply**
- Minimum current: **500mA** (peaks up to 800mA during WiFi transmission)
- USB powerbanks work great for portable operation
- Don't power from PC USB ports during operation (insufficient current)

### OLED Display
- Use a **5V compatible** SSD1306 display
- I2C address is typically **0x3C** (default in code)
- If display doesn't work, verify I2C address with an I2C scanner sketch

---

## 🔨 Assembly Steps

### 1. USB Programmer Connection (for uploading code only)

When uploading code, you need the USB-to-Serial programmer:

| Programmer | ESP32-CAM |
|------------|-----------|
| 5V         | 5V        |
| GND        | GND       |
| TXD        | U0R (RX)  |
| RXD        | U0T (TX)  |

**Important:** Connect **GPIO 0 to GND** during code upload, then disconnect after uploading.

### 2. OLED Display Connection

After uploading the code:
1. Remove the USB programmer (optional, but recommended for final assembly)
2. Connect the OLED display as shown in the diagram above
3. Connect 5V power supply

### 3. Final Assembly

Once everything is connected:
1. Power on the ESP32-CAM
2. Wait 2-3 seconds for boot
3. Check the OLED display - it will show WiFi credentials and IP address
4. Connect to the WiFi hotspot and access the web interface

---

## ⚡ Power Supply Options

### Option 1: USB Powerbank (Recommended for portable use)
- Cut a USB cable and connect wires to 5V and GND pins
- Provides stable power and portability
- Great for moving the camera between rooms

### Option 2: 5V Wall Adapter
- Use a reliable 5V adapter (phone charger works)
- Minimum 1A current rating
- Best for fixed installations

### Option 3: Bench Power Supply
- Set to 5V, current limit to 1A
- Perfect for development and testing

---

## 🔙 Back to Main Documentation

[← Back to README](README.md)
