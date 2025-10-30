#ifndef CAN_COMM_H
#define CAN_COMM_H

#include <Arduino.h>
#include <driver/twai.h>

// CAN Pin Definitions for M5Stack Dial
// Using PORT.B GPIO pins (GPIO1 and GPIO2 are available)
#define CAN_TX_PIN GPIO_NUM_1
#define CAN_RX_PIN GPIO_NUM_2

// CAN Configuration
#define CAN_BAUD_RATE TWAI_TIMING_CONFIG_500KBITS()

// CAN Node Addresses (matching master node)
#define MASTER_NODE_ADDRESS     0x300
#define MOTOR_CONTROL_ADDRESS   0x311
#define CONTROLLER_120V_ADDRESS 0x543

// Command definitions (matching master node)
#define ACTIVATE_CMD   0x01
#define DEACTIVATE_CMD 0x02

// Motor command definitions
#define MOTOR_SET_RPM_CMD     0x30
#define MOTOR_SET_DIRECTION_CMD 0x31
#define MOTOR_STOP_CMD        0x32
#define MOTOR_STATUS_CMD      0x33

// Motor response definitions
#define MOTOR_ACK_RPM         0x40
#define MOTOR_ACK_DIRECTION   0x41
#define MOTOR_ACK_STOP        0x42
#define MOTOR_ACK_STATUS      0x43
#define MOTOR_ERROR_RESPONSE  0xFF

// Function declarations
bool initializeCAN();
bool sendCANMessage(uint16_t address, uint8_t command, uint8_t data = 0);
bool sendMotorCommand(uint8_t command, uint8_t data = 0);
bool sendWashCommand();
bool sendDryCommand();
bool sendStopCommand();
void processCANMessages();

#endif // CAN_COMM_H