# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO-based ESP32-S3 firmware project for a CAN bus master node controller. The project implements a CAN communication system to send activate/deactivate commands to other nodes on the network.

## Development Commands

**Build the project:**
```bash
# Note: Requires PlatformIO to be installed
pio run
```

**Upload to device:**
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

## Architecture

### Hardware Configuration
- **Target Board**: ESP32-S3-DevKitC-1
- **CAN Interface**: Uses ESP32's TWAI (Two-Wire Automotive Interface) driver
- **CAN Pins**: TX on GPIO4, RX on GPIO3
- **Baud Rate**: 500 kbps
- **Serial Monitor**: 115200 baud

### Code Structure
- **src/main.cpp**: Contains all application logic including:
  - CAN initialization and configuration
  - Message sending functions (`activatePort`, `deactivatePort`)
  - Message receiving and processing
  - Protocol definitions for command IDs

### CAN Protocol
- **Standard 11-bit CAN IDs** (not extended)
- **Message Format**: 3-byte payload
  - Byte 0: Command ID (0x01 = ACTIVATE, 0x02 = DEACTIVATE)
  - Byte 1: Port number
  - Byte 2: Reserved (0x00)
- **Response Handling**: Processes acknowledgments and error responses from target nodes

### Key Functions
- `initializeCAN()`: Sets up TWAI driver with 500kbps timing
- `activatePort(address, port)`: Sends activation command to specific CAN address
- `deactivatePort(address, port)`: Sends deactivation command
- `receiveCANMessages()`: Non-blocking message reception
- `processReceivedMessage()`: Handles incoming responses and acknowledgments

## PlatformIO Configuration

The project uses two build environments in `platformio.ini`:
- `esp32-s3-devkitc-1`: Standard development build
- `esp32-builds`: Alternative build configuration

Key settings:
- 240MHz CPU frequency
- QIO flash mode at 80MHz
- Hardware CDC and JTAG USB mode
- Exception decoder for debugging