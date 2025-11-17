#ifndef CAN_COMM_H
#define CAN_COMM_H

#include <Arduino.h>
#include <driver/twai.h>

// CAN Pin Definitions for M5Stack Dial
// Using PORT.B GPIO pins (GPIO1 and GPIO2)
//
// FOR TESTING (NO TRANSCEIVER):
//   - Connect GPIO1 to GPIO2 with a jumper wire for loopback testing
//   - Set mode to TWAI_MODE_NO_ACK in can_comm.cpp
//
// FOR PRODUCTION (WITH CAN TRANSCEIVER):
//   - Remove jumper wire between GPIO1 and GPIO2
//   - Connect GPIO1 to transceiver CTX/TXD
//   - Connect GPIO2 to transceiver CRX/RXD
//   - Set mode to TWAI_MODE_NORMAL in can_comm.cpp
//
#define CAN_TX_PIN GPIO_NUM_1
#define CAN_RX_PIN GPIO_NUM_2

// Alternative pins if GPIO1/2 don't work (uncomment to try):
// #define CAN_TX_PIN GPIO_NUM_15
// #define CAN_RX_PIN GPIO_NUM_16

// CAN Configuration
#define CAN_BAUD_RATE TWAI_TIMING_CONFIG_500KBITS()

// Command definitions for output control (matching master node)
#define ACTIVATE_CMD   0x01
#define DEACTIVATE_CMD 0x02

// Sensor request command definitions
#define READ_ANALOG_CMD       0x03
#define READ_DIGITAL_CMD      0x04
#define READ_ALL_ANALOG_CMD   0x05
#define READ_ALL_DIGITAL_CMD  0x06

// Motor command definitions
#define MOTOR_SET_RPM_CMD     0x30
#define MOTOR_SET_DIRECTION_CMD 0x31
#define MOTOR_STOP_CMD        0x32
#define MOTOR_STATUS_CMD      0x33

// Response command definitions
#define ACK_ACTIVATE          0x10
#define ACK_DEACTIVATE        0x11
#define ANALOG_DATA           0x20
#define DIGITAL_DATA          0x21
#define ALL_ANALOG_DATA       0x22
#define ALL_DIGITAL_DATA      0x23

// Motor response definitions
#define ACK_MOTOR_RPM         0x40
#define ACK_MOTOR_DIRECTION   0x41
#define ACK_MOTOR_STOP        0x42
#define MOTOR_STATUS_DATA     0x43
#define ERROR_RESPONSE        0xFF

// Function declarations
bool initializeCAN();
bool sendCANMessage(uint16_t address, uint8_t command, uint8_t data = 0);
// bool sendMotorCommand(uint8_t command, uint8_t data = 0);
// bool sendWashCommand();
// bool sendDryCommand();
// bool sendStopCommand();
void processCANMessages();

// Pin control functions (from master node)
bool sendOutputCommand(uint16_t device_address, uint8_t command, uint8_t port);
bool requestAnalogReading(uint16_t device_address, uint8_t pin);
bool requestDigitalReading(uint16_t device_address, uint8_t pin);
bool requestAllAnalogReadings(uint16_t device_address);
bool requestAllDigitalReadings(uint16_t device_address);
bool sendMotorRPM(uint16_t device_address, uint16_t rpm);
bool sendMotorDirection(uint16_t device_address, bool clockwise);
bool sendMotorStop(uint16_t device_address);
bool requestMotorStatus(uint16_t device_address);
bool sendGenericCANMessage(uint16_t address, uint8_t* data, uint8_t data_length);


// // CAN Node Addresses (matching master node)
// #define MASTER_NODE_ADDRESS     0x300
// #define MOTOR_CONTROL_ADDRESS   0x311
// #define CONTROLLER_120V_ADDRESS 0x543


#endif // CAN_COMM_H