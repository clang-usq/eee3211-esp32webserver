# Garden Monitoring Station

ESP32-C6 project using ESP-IDF with Arduino as a component.

This project does two main things:
- connects to Wi-Fi and serves a small web dashboard
- receives sensor data over ESP-NOW and exposes it through `/v1/data/`

## Features

- Web dashboard served directly from the ESP32
- JSON endpoint at `/v1/data/`
- mDNS hostname: `http://gardenmonitoringstation.local`
- ESP-NOW receiver support for humidity, rainfall and wind data sensor nodes
- Current sensor handling for:
  - wind speed
  - wind direction
  - humidity
  - rainfall

## Project Layout

- `main/esp32_webserver.cpp` main application
- `main/idf_component.yml` component dependencies
- `CMakeLists.txt` project build config

## Requirements

- ESP-IDF `v5.5.4`
- ESP32-C6 board
- Arduino-ESP32 as an ESP-IDF component

## Build

Open an ESP-IDF terminal in the project folder and run:

```powershell
idf.py build
```

Flash and monitor:

```powershell
idf.py -p COM4 flash monitor
```

## Web Access

After boot, the device logs its IP and mDNS name.

Open either:

- `http://gardenmonitoringstation.local`
- `http://<device-ip>/`

JSON endpoint:

- `http://gardenmonitoringstation.local/v1/data/`
- `http://<device-ip>/v1/data/`

## ESP-NOW

This device acts as an ESP-NOW receiver.

Sensor nodes send packets with a sensor ID and values. The receiver updates the latest sensor state and the web page fetches that data from `/v1/data/`.

Current sensor ID mapping:

- `1` wind
- `2` humidity
- `3` rainfall

Make sure sender and receiver use the exact same packet structure.

## Notes

![SerialMonitor](image.png)

- `managed_components/`, `build/`, and `sdkconfig` are ignored in Git
- if `.local` does not resolve on your PC, use the direct IP address
