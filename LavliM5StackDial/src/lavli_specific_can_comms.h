#ifndef LAVLI_SPECIFIC_CAN_COMMS_H
#define LAVLI_SPECIFIC_CAN_COMMS_H

#include "can_comm.h"

// CAN Node Addresses (matching master node)
// #define MASTER_NODE_ADDRESS     0x300
#define MOTOR_CONTROL_ADDRESS   0x311
#define CONTROLLER_120V_1_ADDRESS 0x540
#define CONTROLLER_120V_2_ADDRESS 0x543
#define CONTROLLER_12V_1_ADDRESS 0x220
#define CONTROLLER_12V_2_ADDRESS 0x223
#define CONTROLLER_12V_3_ADDRESS 0x226
#define SENSOR_NODE_1_ADDRESS     0x124  // Clean Water Tank Level Sensors
#define SENSOR_NODE_2_ADDRESS     0x128

// Sensor Node 1 0x124
#define CLEAN_TANK_LOW_PIN   1 // GPIO 1  // Digital pin 0 - Low water level
#define CLEAN_TANK_MID_PIN   2 // GPIO 2  // Digital pin 1 - Mid water level
#define CLEAN_TANK_HIGH_PIN  3 // GPIO 3  // Digital pin 2 - High water level
#define TEMPERATURE_RESISTOR_BACK 8 // GPIO 8 (ANALOG)
#define TEMPERATURE_RESISTOR_FRONT 7 // GPIO 7 (ANALOG)
#define FLOW_SENSOR_PIN 9 // GPIO 9 (PULSE IN) 

// 12v Board 1 0x220
#define PERISTALTIC_PUMP_PORT 1
#define OZONE_GENERATOR_PORT 2
#define DRAIN_PUMP_PORT 3

// 120v Board 1 0x540
#define DOOR_LOCK_PORT 1
#define VANE_PUMP_PORT 2

// 12v Board 2 0x223
#define CONDENSOR_FANS_PORT 1
#define CLEAN_WATER_PUMP_PORT 2
#define SOLENOID_1_PORT 3 // TODO: Solenoid purpose TBD

// 120v Board 2 0x543
#define HEATER_PORT 1
#define HEATER_FANS_PORT 2

// 12v Board 3 0x226
#define SOLENOID_2_PORT 1 // TODO: Solenoid purpose TBD
#define SOLENOID_4_PORT 2 // TODO: Solenoid purpose TBD
#define SOLENOID_3_PORT 3 // TODO: Solenoid purpose TBD

// Sensor Node 2 0x128
#define DIRTY_TANK_LOW_PIN 1 // GPIO 1
#define DIRTY_TANK_MID_PIN 2 // GPIO 2
#define DIRTY_TANK_HIGH_PIN 3 // GPIO 3
#define LEAK_DETECTION_FRONT_PIN 6 // GPIO6
#define LEAK_DETECTION_BACK_PIN 8 // GPIO 8
#define PURGE_TANK_LOW_PIN 9 // GPIO 9
#define PURGE_TANK_HIGH_PIN 10 // GPIO 10
#define PRESSURE_SENSOR_PIN 11 // GPIO 11 (Analog)




// ============================================================================
// High-Level Lavli-Specific CAN Functions
// ============================================================================

/**
 * Toggle the heater on or off
 * @param on true to activate heater, false to deactivate
 * @return true if command sent successfully
 */
bool toggleHeater(bool on);

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

// ============================================================================
// 12V Controller Toggle Functions
// ============================================================================

/**
 * Toggle the peristaltic pump on or off (12V Board 1, Port 1)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool togglePeristalticPump(bool on);

/**
 * Toggle the ozone generator on or off (12V Board 1, Port 2)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool toggleOzoneGenerator(bool on);

/**
 * Toggle the drain pump on or off (12V Board 1, Port 3)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool toggleDrainPump(bool on);

/**
 * Toggle the condenser fans on or off (12V Board 2, Port 1)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool toggleCondenserFans(bool on);

/**
 * Toggle the clean water pump on or off (12V Board 2, Port 2)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool toggleCleanWaterPump(bool on);

// ============================================================================
// 120V Controller Toggle Functions
// ============================================================================

/**
 * Toggle the vane pump on or off (120V Board 1, Port 2)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool toggleVanePump(bool on);

/**
 * Toggle the heater fans on or off (120V Board 2, Port 2)
 * @param on true to activate, false to deactivate
 * @return true if command sent successfully
 */
bool toggleHeaterFans(bool on);

// ============================================================================
// Temperature Sensor Functions (Analog)
// ============================================================================

/**
 * Request temperature resistor back sensor reading (Sensor Node 1, Analog Pin 8)
 * Sends CAN request - use getTemperatureResistorBack() to retrieve value
 * @return true if request sent successfully
 */
bool requestTemperatureResistorBack();

/**
 * Get last received temperature resistor back sensor reading
 * @return SensorReading struct (check .valid field before using .analog_value)
 */
SensorReading getTemperatureResistorBack();

/**
 * Request temperature resistor front sensor reading (Sensor Node 1, Analog Pin 7)
 * Sends CAN request - use getTemperatureResistorFront() to retrieve value
 * @return true if request sent successfully
 */
bool requestTemperatureResistorFront();

/**
 * Get last received temperature resistor front sensor reading
 * @return SensorReading struct (check .valid field before using .analog_value)
 */
SensorReading getTemperatureResistorFront();

// ============================================================================
// Dirty Tank Water Level Sensor Functions (Digital)
// ============================================================================

/**
 * Request dirty tank low water level sensor reading (Sensor Node 2, Pin 1)
 * Sends CAN request - use getDirtyTankLowWaterSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestDirtyTankLowWaterSensor();

/**
 * Get last received dirty tank low water level sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getDirtyTankLowWaterSensor();

/**
 * Request dirty tank mid water level sensor reading (Sensor Node 2, Pin 2)
 * Sends CAN request - use getDirtyTankMidWaterSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestDirtyTankMidWaterSensor();

/**
 * Get last received dirty tank mid water level sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getDirtyTankMidWaterSensor();

/**
 * Request dirty tank high water level sensor reading (Sensor Node 2, Pin 3)
 * Sends CAN request - use getDirtyTankHighWaterSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestDirtyTankHighWaterSensor();

/**
 * Get last received dirty tank high water level sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getDirtyTankHighWaterSensor();

// ============================================================================
// Leak Detection Sensor Functions (Digital)
// ============================================================================

/**
 * Request leak detection front sensor reading (Sensor Node 2, Pin 6)
 * Sends CAN request - use getLeakDetectionFront() to retrieve value
 * @return true if request sent successfully
 */
bool requestLeakDetectionFront();

/**
 * Get last received leak detection front sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getLeakDetectionFront();

/**
 * Request leak detection back sensor reading (Sensor Node 2, Pin 8)
 * Sends CAN request - use getLeakDetectionBack() to retrieve value
 * @return true if request sent successfully
 */
bool requestLeakDetectionBack();

/**
 * Get last received leak detection back sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getLeakDetectionBack();

// ============================================================================
// Purge Tank Sensor Functions (Digital)
// ============================================================================

/**
 * Request purge tank low sensor reading (Sensor Node 2, Pin 9)
 * Sends CAN request - use getPurgeTankLowSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestPurgeTankLowSensor();

/**
 * Get last received purge tank low sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getPurgeTankLowSensor();

/**
 * Request purge tank high sensor reading (Sensor Node 2, Pin 10)
 * Sends CAN request - use getPurgeTankHighSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestPurgeTankHighSensor();

/**
 * Get last received purge tank high sensor reading
 * @return SensorReading struct (check .valid field before using .digital_value)
 */
SensorReading getPurgeTankHighSensor();

// ============================================================================
// Pressure Sensor Functions (Analog)
// ============================================================================

/**
 * Request pressure sensor reading (Sensor Node 2, Analog Pin 11)
 * Sends CAN request - use getPressureSensor() to retrieve value
 * @return true if request sent successfully
 */
bool requestPressureSensor();

/**
 * Get last received pressure sensor reading
 * @return SensorReading struct (check .valid field before using .analog_value)
 */
SensorReading getPressureSensor();

#endif // LAVLI_SPECIFIC_CAN_COMMS_H
