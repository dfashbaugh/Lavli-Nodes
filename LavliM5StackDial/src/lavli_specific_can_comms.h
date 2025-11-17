#ifndef LAVLI_SPECIFIC_CAN_COMMS_H
#define LAVLI_SPECIFIC_CAN_COMMS_H

#include "can_comm.h"

// CAN Node Addresses (matching master node)
#define MASTER_NODE_ADDRESS     0x300
#define MOTOR_CONTROL_ADDRESS   0x311
#define CONTROLLER_120V_ADDRESS 0x543
#define SENSOR_NODE_ADDRESS     0x124  // Updated to match actual sensor node

// Clean Tank Water Level Sensor Pin Definitions
#define CLEAN_TANK_LOW_PIN      0  // Digital pin 0 - Low water level
#define CLEAN_TANK_MID_PIN      1  // Digital pin 1 - Mid water level
#define CLEAN_TANK_HIGH_PIN     2  // Digital pin 2 - High water level

// 120V Controller Port Definitions
#define DRYER_PORT              1  // Port 1 controls the dryer

// ============================================================================
// High-Level Lavli-Specific CAN Functions
// ============================================================================

/**
 * Toggle the dryer on or off
 * @param on true to activate dryer, false to deactivate
 * @return true if command sent successfully
 */
bool toggleDryer(bool on);

/**
 * Set motor RPM and direction
 * @param rpm Positive values = clockwise, negative = counter-clockwise, 0 = stop
 * @return true if command(s) sent successfully
 */
bool setMotorRPM(int rpm);

/**
 * Request clean tank low water level sensor reading
 * Sends CAN request - use getCleanTankLowWaterSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestCleanTankLowWaterSensor();

/**
 * Request clean tank mid water level sensor reading
 * Sends CAN request - use getCleanTankMidWaterSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestCleanTankMidWaterSensor();

/**
 * Request clean tank high water level sensor reading
 * Sends CAN request - use getCleanTankHighWaterSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestCleanTankHighWaterSensor();

/**
 * Get last received clean tank low water level sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getCleanTankLowWaterSensor();

/**
 * Get last received clean tank mid water level sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getCleanTankMidWaterSensor();

/**
 * Get last received clean tank high water level sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getCleanTankHighWaterSensor();

#endif // LAVLI_SPECIFIC_CAN_COMMS_H
