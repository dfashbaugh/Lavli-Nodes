# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a multi-node ESP32-S3 based system implementing a distributed CAN bus network for motor control and MQTT communication. The system consists of several ESP32-S3 nodes that communicate via CAN bus for real-time control and MQTT for remote command/status reporting.

## Architecture

### Node Types
- **Lavli Master Node**: CAN bus master with MQTT client, NeoPixel interface, and rotary encoder
- **Lavli Motor Control Node**: CAN slave controlling motor hardware via serial communication  
- **Lavli MQTT Provisioner**: WiFi provisioning and MQTT subscriber for `/lavli` topic
- **Lavli BLE Provisioner**: Bluetooth Low Energy provisioning alternative
- **Lavli Sensor Node**: Environmental sensor data collection
- **Lavli 12v Node**: High-power device control
- **LavliM5StackDial**: M5Stack Dial-based UI node with wash/dry mode selection
- **CAN Pin Tester**: Hardware testing utility
- **ESP LED and Encoder Control**: LED strip and encoder testing utility

### Communication Architecture
1. **CAN Bus Network**: 500kbps, standard 11-bit IDs, 3-byte message protocol
   - Command types: ACTIVATE (0x01), DEACTIVATE (0x02), MOTOR_SET_RPM (0x30), etc.
   - Master node (0x300) controls motor nodes (0x311) and other devices
2. **MQTT Integration**: AWS IoT broker connectivity with SSL/TLS
   - Topics: `/lavli/dry`, `/lavli/wash`, `/lavli/stop`, `/lavli/can`
   - Authentication via username/password (AWS IoT broker)
   - Development broker: broker.hivemq.com (port 1883)
3. **Serial Communication**: Motor nodes communicate with hardware controllers via UART

### Hardware Configuration
- **Target Platform**: ESP32-S3-DevKitC-1, M5Stack Dial
- **CAN Interface**: TWAI driver, GPIO4 (TX) / GPIO5 (RX)
- **Motor Control**: UART1 for motor controller communication
- **User Interface**: NeoPixel strip (24 LEDs), rotary encoder with switch, M5Stack Dial display
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

**Environment-specific builds:**
```bash
# Master Node environments
pio run -e esp32-s3-devkitc-1
pio run -e esp32-builds  
pio run -e lavli-master

# M5Stack Dial environment
pio run -e m5dial

# Motor Control Node environment
pio run -e esp32-s3-devkitc-1
```

**Development workflow for specific nodes:**
```bash
# Master Node with multiple environments
cd "Lavli Master Node"
pio run -e lavli-master --target upload

# M5Stack Dial
cd "LavliM5StackDial" 
pio run -e m5dial --target upload
```

## Testing Utilities

**MQTT Testing** (requires Python with paho-mqtt):
```bash
cd PythonUtils
python3 MQTTSendTester.py --server broker.address --topic /lavli/wash --message "test"
python3 MQTTReceiveTester.py --server broker.address --topic /lavli/+
```

**CAN Controller GUI** (Python/Tkinter MQTT interface):
```bash
cd PythonUtils
python3 can_controller_gui.py
```

**Shell scripts for quick testing:**
```bash
cd PythonUtils
./startReceiveTester.sh
./startSendTester.sh
```

## Key Configuration Points

### CAN Protocol
- **Baud Rate**: 500 kbps across all nodes
- **Address Mapping**: Master (0x300), Motor Control (0x311)
- **Message Format**: [Command ID][Data][Reserved]
- **Pin Configuration**: TX=GPIO4, RX=GPIO5 (or GPIO3 on some nodes)

### MQTT Configuration  
- **Production Broker**: AWS IoT with SSL (port 8883)
  - Server: `b-f3e16066-c8b5-48a8-81b0-bc3347afef80-1.mq.us-east-1.amazonaws.com`
  - Username: `admin` / Password: `lavlidevbroker!321` (hardcoded, should be externalized)
- **Development Broker**: broker.hivemq.com (port 1883)
- **WiFi Provisioning**: WiFiManager creates AP "Lavli-CAN-Master" when credentials missing
- **Hardcoded WiFi**: SSID "PKFLetsKickIt" / Password "ClarkIsACat" (should be externalized)

### Motor Control Protocol
- **Serial Interface**: 115200 baud, UART1
- **Commands**: Set RPM (0x30), Direction (0x31), Stop (0x32), Status (0x33)
- **Responses**: ACK codes (0x40-0x43) and error responses (0xFF)

## Project Structure

Each node directory contains:
- `src/main.cpp`: Primary application logic
- `platformio.ini`: Build configuration with environment-specific settings
- `CLAUDE.md`: Node-specific documentation (where present)
- `.pio/`: PlatformIO build artifacts (excluded from git)
- `lib/`: Local libraries
- `include/`: Header files
- `test/`: Test files

**Key node-specific configurations:**
- **LavliM5StackDial**: Uses M5Dial libraries, `m5dial` environment, simple wash/dry UI
- **Lavli Master Node**: Multiple environments (`lavli-master`, `esp32-s3-devkitc-1`, `esp32-builds`)
- **Motor Control Nodes**: Minimal dependencies, focus on CAN communication and motor control

## Development Guidelines

### Environment Selection
- Use `lavli-master` environment for Master Node production builds
- Use `m5dial` environment for M5Stack Dial builds
- Use `esp32-s3-devkitc-1` for most other nodes
- Check `platformio.ini` in each node for available environments

### Security Considerations
- WiFi and MQTT credentials are hardcoded (should be moved to secure storage)
- AWS IoT broker credentials embedded in source code (security concern)
- Consider implementing secure credential storage for production deployment

### Hardware Requirements
- CAN bus requires proper termination resistors for reliable operation
- Motor nodes expect specific hardware controllers with defined serial protocol
- Each node targets ESP32-S3-DevKitC-1 platform (except M5Stack Dial variant)

### Build System
- Each PlatformIO project is independent and must be built separately
- No unified build system across all nodes
- Dependencies are specified per-node in respective `platformio.ini` files