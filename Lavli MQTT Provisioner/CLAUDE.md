# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-S3 PlatformIO project that creates a WiFi provisioning and MQTT client device. The device creates an access point for WiFi configuration, then connects to an MQTT broker to subscribe to the `/lavli` topic and print received messages.

## Build and Development Commands

**Build the project:**
```bash
platformio run -e esp32-s3-devkitc-1
```

**Upload to device:**
```bash
platformio run -e esp32-s3-devkitc-1 -t upload
```

**Monitor serial output:**
```bash
platformio device monitor -b 115200
```

**Clean build:**
```bash
platformio run -e esp32-s3-devkitc-1 -t clean
```

## Architecture

### Hardware Target
- ESP32-S3 DevKit C-1 board
- 240MHz CPU, 4MB flash memory
- USB CDC enabled for serial communication

### Core Functionality
1. **WiFi Provisioning**: Uses WiFiManager library to create an AP named "Lavli-Provisioner" when no saved WiFi credentials exist
2. **MQTT Client**: Connects to broker.hivemq.com:1883 and subscribes to `/lavli` topic
3. **Message Handling**: Prints all received MQTT messages to serial console

### Key Configuration
- **MQTT_BROKER**: Set to "broker.hivemq.com" (can be changed in main.cpp:6)
- **MQTT_TOPIC**: Set to "/lavli" (can be changed in main.cpp:8) 
- **AP_NAME**: Access point name "Lavli-Provisioner" (can be changed in main.cpp:9)

### Dependencies
- WiFiManager@^0.16.0 - For WiFi provisioning
- PubSubClient@^2.8 - For MQTT client functionality
- ESP32 Arduino framework

### Environment Configurations
- `esp32-s3-devkitc-1`: Main development environment
- `esp32-builds`: Alternative build environment  
- `lavli-master`: Board configuration with PSRAM support

## Development Notes

The device will automatically:
1. Start WiFi provisioning if no credentials are saved
2. Connect to the configured WiFi network
3. Connect to the MQTT broker
4. Subscribe to the `/lavli` topic
5. Print any received messages to serial console

When WiFi credentials need to be reset, the device creates an access point that can be accessed via web browser for configuration.