# B-Watch using ESP-IDF

This project demonstrates how to interface the CST816S capacitive touch controller on GC9A01 TFT-LCD with ESP-S3 using the ESP-IDF framework.
並用於開發檢測滲血傷口感測器Blood Monitor Watch

## Requirements
### Hardware
- [ESP32-S3 LCD ESP32-S3-Touch-LCD-1.28](https://www.waveshare.com/esp32-s3-touch-lcd-1.28.htm)
    - ESP32-S3 development board
    - GC9A01 TFT-LCD 
    - CST816S touch controller module
- Sensor PCB (from NCKU WTMH LAB)
    - PCB材料清單
    - 
### Sofrware
- VS code IDE
- ESP32-IDF (version 5.15.5)
- LVGL library
- 

## Hardware Setup

| Function            | ESP32-S3 GPIO Pin | GC9A01 Pin   | CST816S Pin  |
|---------------------|--------------------|--------------|--------------|
| **LCD SDA**         | GPIO 21           | SDA          | -            |
| **LCD SCL**         | GPIO 22           | SCL          | -            |
| **LCD Reset (RST)** | GPIO 15           | RESET        | -            |
| **LCD DC**          | GPIO 2            | DC           | -            |
| **LCD CS**          | GPIO 5            | CS           | -            |
| **LCD BL**          | GPIO 4            | BL           | -            |
| **Touch INT**       | GPIO 13           | -            | INT          |
| **Touch RST**       | GPIO 5            | -            | RST          |
| **Touch SDA**       | GPIO 18           | -            | SDA          |
| **Touch SCL**       | GPIO 19           | -            | SCL          |

**Note:** Some GPIOs are not corrected from Waveshare's website....

## Software Configuration
Ensure that your `sdkconfig` has the necessary I2C drivers enabled.

- **Install ESP-IDF on VScode IDE**


- **Install Library provided by ESpressif:**
1. LVGL 

2. GC9a01

3. CST816S

- **Install other Library:**
- 
all the needed arduino library

**Note:** Your ESP-IDF version is limited in >=5.1 and <=5.2



## Project Structure



## Code Overview
### I2C Initialization
123test

### SPI Initialization
asdtwst

### LVGL Touch Integration
qwewqeas


## Troubleshooting
### 螢幕pixel random initialization

### ESP-IDF Monitor
Error loop about [I2C Read Failed (IDFGH-11933) #13013](https://github.com/espressif/esp-idf/issues/13013)
```
Loop{
    E (5274) CST816S: read_data(108): I2C read failed
    E (5306) lcd_panel.io.i2c: panel_io_i2c_rx_buffer(135): i2c transaction failed
}
```







