# B-Watch Eusing ESP-IDF

This project demonstrates how to interface the CST816S capacitive touch controller on GC9A01 TFT-LCD with ESP-S3 using the ESP-IDF framework.
並用於開發檢測滲血傷口感測器Blood Moonitor Watch

## Requirements
_Hardware_
- [ESP32-S3 LCD ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/esp32-s3-touch-lcd-1.28.htm)
    - ESP32-S3 development board
    - GC9A01 TFT-LCD 
    - CST816S touch controller module
- Sensor PCB (from NCKU WTMH LAB)
    - PCB材料清單
    - 
_Sofrware_
- VS code IDE
- ESP32-IDF (version 5.15.5)
- LVGL library
- 

## Hardware Setup

| Function            | ESP32-S3 GPIO Pin | GC9A01 Pin   | CST816S Pin  |
|---------------------|--------------------|--------------|--------------|
| **LCD SDA**         | GPIO 21           | SDA          | -            |
| **LCD SCL**         | GPIO 22           | SCL          | -            |
| **Touch SDA**       | GPIO 18           | -            | SDA          |
| **Touch SCL**       | GPIO 19           | -            | SCL          |
| **LCD Reset (RST)** | GPIO 15           | RESET        | -            |
| **LCD DC**          | GPIO 2            | DC           | -            |
| **LCD CS**          | GPIO 5            | CS           | -            |
| **LCD BL**          | GPIO 4            | BL           | -            |
| **Touch INT**       | GPIO 13           | -            | INT          |
| **Touch RST**       | GPIO 5            | -            | RST          |

**Note:** Some GPIOs are not corrected from Wareshare's website....

## Software Configuration
Ensure that your `sdkconfig` has the necessary I2C drivers enabled.

1. **Enable I2C in ESP-IDF:**



2. **Install LVGL (if not already integrated):**
Follow LVGL integration steps via [LVGL ESP-IDF docs](https://docs.lvgl.io/latest/en/html/get-started/esp32.html).

## Project Structure



