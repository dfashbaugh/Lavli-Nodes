# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a multi-node ESP32-S3 based system implementing a distributed CAN bus network for motor control and MQTT communication. The system consists of several ESP32-S3 nodes that communicate via CAN bus for real-time control and MQTT for remote command/status reporting.

## Architecture

### Node Types
- **Lavli Master Node**: CAN bus master with MQTT client, NeoPixel interface, and rotary encoder
- **Lavli Motor Control Node**: CAN slave controlling motor hardware via serial communication  
- **Lavli MQTT Provisioner**: WiFi provisioning and MQTT subscriber for `/lavli` topic
- **Lavli Sensor Node**: Environmental sensor data collection
- **Lavli 12v Node**: High-power device control
- **CAN Pin Tester**: Hardware testing utility

### Communication Architecture
1. **CAN Bus Network**: 500kbps, standard 11-bit IDs, 3-byte message protocol
   - Command types: ACTIVATE (0x01), DEACTIVATE (0x02), MOTOR_SET_RPM (0x30), etc.
   - Master node (0x300) controls motor nodes (0x311) and other devices
2. **MQTT Integration**: AWS IoT broker connectivity with SSL/TLS
   - Topics: `/lavli/dry`, `/lavli/wash`, `/lavli/stop`
   - Authentication via username/password
3. **Serial Communication**: Motor nodes communicate with hardware controllers via UART

### Hardware Configuration
- **Target Platform**: ESP32-S3-DevKitC-1 
- **CAN Interface**: TWAI driver, GPIO4 (TX) / GPIO5 (RX)
- **Motor Control**: UART1 for motor controller communication
- **User Interface**: NeoPixel strip (24 LEDs), rotary encoder with switch
- **WiFi**: 2.4GHz with WiFiManager for provisioning

## Development Commands

**Build any node:**
```bash
cd "Node Directory"
pio run
```

**Upload firmware:**
```bash
pio run --target upload
```

**Monitor serial output:**
```bash
pio device monitor
```

**Clean build files:**
```bash
pio run --target clean
```

**Multiple environment builds** (where applicable):
```bash
pio run -e esp32-s3-devkitc-1
pio run -e esp32-builds
pio run -e lavli-master
```

## Testing Utilities

**MQTT Testing** (requires Python with paho-mqtt):
```bash
cd PythonUtils
python3 MQTTSendTester.py --server broker.address --topic /lavli/wash --message "test"
python3 MQTTReceiveTester.py --server broker.address --topic /lavli/+
```

## Key Configuration Points

### CAN Protocol
- **Baud Rate**: 500 kbps across all nodes
- **Address Mapping**: Master (0x300), Motor Control (0x311)
- **Message Format**: [Command ID][Data][Reserved]
- **Pin Configuration**: TX=GPIO4, RX=GPIO5 (or GPIO3 on some nodes)

### MQTT Configuration  
- **Production Broker**: AWS IoT with SSL (port 8883)
- **Development Broker**: broker.hivemq.com (port 1883)
- **Authentication**: Username/password stored in source (should be externalized)
- **WiFi Provisioning**: WiFiManager creates AP when credentials missing

### Motor Control Protocol
- **Serial Interface**: 115200 baud, UART1
- **Commands**: Set RPM (0x30), Direction (0x31), Stop (0x32), Status (0x33)
- **Responses**: ACK codes (0x40-0x43) and error responses (0xFF)

## Project Structure

Each node directory contains:
- `src/main.cpp`: Primary application logic
- `platformio.ini`: Build configuration with multiple environments
- `CLAUDE.md`: Node-specific documentation (where present)
- `.pio/`: PlatformIO build artifacts
- `lib/`: Local libraries
- `include/`: Header files

## Important Notes

- WiFi credentials are hardcoded in master node (should be moved to secure storage)
- MQTT credentials are embedded in source code (security concern)
- CAN bus requires proper termination resistors for reliable operation
- Motor nodes expect specific hardware controllers with defined serial protocol
- Each PlatformIO project is independent and must be built separately