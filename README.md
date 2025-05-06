| Supported Targets | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C5 | ESP32-C6 | ESP32-C61 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 | Linux |
| ----------------- | ----- | -------- | -------- | -------- | -------- | --------- | -------- | -------- | -------- | -------- | ----- |

# Pfeiffer Gauge Controller

ESP32-C6 based display and data server for Pfeiffer vacuum gauge (CMR-362 by default) with LCD display.

## Requirements
- ESP-IDF v5.4.1
- Visual Studio Code with ESP-IDF Extension

## Building
```bash
idf.py build
```

## Flashing
```bash
idf.py -p COMx flash monitor
```

## Connections
Pressure gauge signal -> `ADC 0` & `GND`  [2025.05.06: NOT TESTED yet]     
WiFi credentials reset button -> `GPIO 5` (normal HIGH) & `GND`      

## Navigation
Navigate to `192.168.4.1` on first use to connect to WiFi.      
Credentials are stored in the NVS partition. Long-pressing WiFi reset button, well, resets creds.

On subsequent uses, the board connects to the WiFi, if available (otherwise you must reset; no autoreset logic implemented).     
IP address is displayed on the board. So is the SoC temperature.    

Navigate to `IPADDR:80/data` for latest pressure measurement.    
Navigate to `IPADDR:80/api/data` for JSON array of latest measurements (buffer size 100).    
Fetching the API data resets the buffer.    
Pressure measurements are JSON of pressure and timestamp in ms (API keys `"p"` and `"t"`).

Sampling interval is 100 ms; display refresh rate is twice that.

P.S.: Code generation has been heavily AI-assisted (Gemini, DeepSeek, Copilot). I'm just now learning this stuff, and really what I needed was the thing to work.