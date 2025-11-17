#include "lavli_specific_can_comms.h"
#include <Arduino.h>

// ============================================================================
// 120V Controller Functions
// ============================================================================

bool toggleHeater(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s dryer (port %d)\n",
                  on ? "Activating" : "Deactivating", HEATER_PORT);

    bool success = sendOutputCommand(CONTROLLER_120V_1_ADDRESS, command, HEATER_PORT);

    if (success) {
        Serial.println("[Lavli] Heater command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send heater command");
    }

    return success;
}

// ============================================================================
// Motor Control Functions
// ============================================================================

bool setMotorRPM(int rpm) {
    bool success = true;

    if (rpm == 0) {
        // Stop motor
        Serial.println("[Lavli] Stopping motor");
        success = sendCANMessage(MOTOR_CONTROL_ADDRESS, MOTOR_STOP_CMD, 0);

        if (success) {
            Serial.println("[Lavli] Motor stop command sent successfully");
        } else {
            Serial.println("[Lavli] Failed to send motor stop command");
        }

        return success;
    }

    // Determine direction based on sign
    bool clockwise = (rpm > 0);
    uint16_t absoluteRPM = abs(rpm);

    Serial.printf("[Lavli] Setting motor: %d RPM (%s)\n",
                  absoluteRPM, clockwise ? "clockwise" : "counter-clockwise");

    // First, set direction
    success = sendMotorDirection(MOTOR_CONTROL_ADDRESS, clockwise);

    if (!success) {
        Serial.println("[Lavli] Failed to send motor direction command");
        return false;
    }

    // Small delay to ensure direction is set before RPM
    delay(10);

    // Then, set RPM
    success = sendMotorRPM(MOTOR_CONTROL_ADDRESS, absoluteRPM);

    if (success) {
        Serial.printf("[Lavli] Motor RPM command sent successfully: %d RPM %s\n",
                      absoluteRPM, clockwise ? "CW" : "CCW");
    } else {
        Serial.println("[Lavli] Failed to send motor RPM command");
    }

    return success;
}

// ============================================================================
// Clean Tank Water Level Sensor Request Functions
// ============================================================================

bool requestCleanTankLowWaterSensor() {
    Serial.println("[Lavli] Requesting clean tank LOW water sensor");
    bool success = requestDigitalReading(SENSOR_NODE_1_ADDRESS, CLEAN_TANK_LOW_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request low water sensor");
    }

    return success;
}

bool requestCleanTankMidWaterSensor() {
    Serial.println("[Lavli] Requesting clean tank MID water sensor");
    bool success = requestDigitalReading(SENSOR_NODE_1_ADDRESS, CLEAN_TANK_MID_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request mid water sensor");
    }

    return success;
}

bool requestCleanTankHighWaterSensor() {
    Serial.println("[Lavli] Requesting clean tank HIGH water sensor");
    bool success = requestDigitalReading(SENSOR_NODE_1_ADDRESS, CLEAN_TANK_HIGH_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request high water sensor");
    }

    return success;
}

// ============================================================================
// Clean Tank Water Level Sensor Getter Functions
// ============================================================================

SensorReading getCleanTankLowWaterSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_1_ADDRESS, CLEAN_TANK_LOW_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Low water sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Low water sensor: NO VALID DATA");
    }

    return reading;
}

SensorReading getCleanTankMidWaterSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_1_ADDRESS, CLEAN_TANK_MID_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Mid water sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Mid water sensor: NO VALID DATA");
    }

    return reading;
}

SensorReading getCleanTankHighWaterSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_1_ADDRESS, CLEAN_TANK_HIGH_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] High water sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] High water sensor: NO VALID DATA");
    }

    return reading;
}

// ============================================================================
// 12V Controller Toggle Functions
// ============================================================================

bool togglePeristalticPump(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s peristaltic pump (12V Board 1, port %d)\n",
                  on ? "Activating" : "Deactivating", PERISTALTIC_PUMP_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_1_ADDRESS, command, PERISTALTIC_PUMP_PORT);

    if (success) {
        Serial.println("[Lavli] Peristaltic pump command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send peristaltic pump command");
    }

    return success;
}

bool toggleOzoneGenerator(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s ozone generator (12V Board 1, port %d)\n",
                  on ? "Activating" : "Deactivating", OZONE_GENERATOR_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_1_ADDRESS, command, OZONE_GENERATOR_PORT);

    if (success) {
        Serial.println("[Lavli] Ozone generator command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send ozone generator command");
    }

    return success;
}

bool toggleDrainPump(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s drain pump (12V Board 1, port %d)\n",
                  on ? "Activating" : "Deactivating", DRAIN_PUMP_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_1_ADDRESS, command, DRAIN_PUMP_PORT);

    if (success) {
        Serial.println("[Lavli] Drain pump command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send drain pump command");
    }

    return success;
}

bool toggleCondenserFans(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s condenser fans (12V Board 2, port %d)\n",
                  on ? "Activating" : "Deactivating", CONDENSOR_FANS_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_2_ADDRESS, command, CONDENSOR_FANS_PORT);

    if (success) {
        Serial.println("[Lavli] Condenser fans command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send condenser fans command");
    }

    return success;
}

bool toggleCleanWaterPump(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s clean water pump (12V Board 2, port %d)\n",
                  on ? "Activating" : "Deactivating", CLEAN_WATER_PUMP_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_2_ADDRESS, command, CLEAN_WATER_PUMP_PORT);

    if (success) {
        Serial.println("[Lavli] Clean water pump command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send clean water pump command");
    }

    return success;
}

bool toggleROBallValve(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s RO ball valve (12V Board 2, port %d)\n",
                  on ? "Activating" : "Deactivating", RO_BALL_VALVE_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_2_ADDRESS, command, RO_BALL_VALVE_PORT);

    if (success) {
        Serial.println("[Lavli] RO ball valve command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send RO ball valve command");
    }

    return success;
}

bool toggleCleanInletSolenoid(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s clean inlet solenoid (12V Board 3, port %d)\n",
                  on ? "Activating" : "Deactivating", CLEAN_INLET_SOLENOID_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_3_ADDRESS, command, CLEAN_INLET_SOLENOID_PORT);

    if (success) {
        Serial.println("[Lavli] Clean inlet solenoid command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send clean inlet solenoid command");
    }

    return success;
}

bool toggleROFlushToPurgeSolenoid(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s RO flush to purge solenoid (12V Board 3, port %d)\n",
                  on ? "Activating" : "Deactivating", RO_FLUSH_TO_PURGE_SOLENOID_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_3_ADDRESS, command, RO_FLUSH_TO_PURGE_SOLENOID_PORT);

    if (success) {
        Serial.println("[Lavli] RO flush to purge solenoid command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send RO flush to purge solenoid command");
    }

    return success;
}

bool toggleROFlushInletSolenoid(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s RO flush inlet solenoid (12V Board 3, port %d)\n",
                  on ? "Activating" : "Deactivating", RO_FLUSH_INLET_SOLENOID_PORT);

    bool success = sendOutputCommand(CONTROLLER_12V_3_ADDRESS, command, RO_FLUSH_INLET_SOLENOID_PORT);

    if (success) {
        Serial.println("[Lavli] RO flush inlet solenoid command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send RO flush inlet solenoid command");
    }

    return success;
}

// ============================================================================
// 120V Controller Toggle Functions
// ============================================================================

bool toggleVanePump(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s vane pump (120V Board 1, port %d)\n",
                  on ? "Activating" : "Deactivating", VANE_PUMP_PORT);

    bool success = sendOutputCommand(CONTROLLER_120V_1_ADDRESS, command, VANE_PUMP_PORT);

    if (success) {
        Serial.println("[Lavli] Vane pump command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send vane pump command");
    }

    return success;
}

bool toggleHeaterFans(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s heater fans (120V Board 2, port %d)\n",
                  on ? "Activating" : "Deactivating", HEATER_FANS_PORT);

    bool success = sendOutputCommand(CONTROLLER_120V_2_ADDRESS, command, HEATER_FANS_PORT);

    if (success) {
        Serial.println("[Lavli] Heater fans command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send heater fans command");
    }

    return success;
}

// ============================================================================
// Temperature Sensor Functions (Analog)
// ============================================================================

bool requestTemperatureResistorBack() {
    Serial.println("[Lavli] Requesting temperature resistor BACK");
    bool success = requestAnalogReading(SENSOR_NODE_1_ADDRESS, TEMPERATURE_RESISTOR_BACK);

    if (!success) {
        Serial.println("[Lavli] Failed to request temperature resistor back");
    }

    return success;
}

SensorReading getTemperatureResistorBack() {
    SensorReading reading = getAnalogReading(SENSOR_NODE_1_ADDRESS, TEMPERATURE_RESISTOR_BACK);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Temperature resistor back: %u (age: %lu ms)\n",
                      reading.analog_value, age);
    } else {
        Serial.println("[Lavli] Temperature resistor back: NO VALID DATA");
    }

    return reading;
}

bool requestTemperatureResistorFront() {
    Serial.println("[Lavli] Requesting temperature resistor FRONT");
    bool success = requestAnalogReading(SENSOR_NODE_1_ADDRESS, TEMPERATURE_RESISTOR_FRONT);

    if (!success) {
        Serial.println("[Lavli] Failed to request temperature resistor front");
    }

    return success;
}

SensorReading getTemperatureResistorFront() {
    SensorReading reading = getAnalogReading(SENSOR_NODE_1_ADDRESS, TEMPERATURE_RESISTOR_FRONT);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Temperature resistor front: %u (age: %lu ms)\n",
                      reading.analog_value, age);
    } else {
        Serial.println("[Lavli] Temperature resistor front: NO VALID DATA");
    }

    return reading;
}

// ============================================================================
// Dirty Tank Water Level Sensor Functions (Digital)
// ============================================================================

bool requestDirtyTankLowWaterSensor() {
    Serial.println("[Lavli] Requesting dirty tank LOW water sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, DIRTY_TANK_LOW_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request dirty tank low water sensor");
    }

    return success;
}

SensorReading getDirtyTankLowWaterSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, DIRTY_TANK_LOW_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Dirty tank low water sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Dirty tank low water sensor: NO VALID DATA");
    }

    return reading;
}

bool requestDirtyTankMidWaterSensor() {
    Serial.println("[Lavli] Requesting dirty tank MID water sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, DIRTY_TANK_MID_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request dirty tank mid water sensor");
    }

    return success;
}

SensorReading getDirtyTankMidWaterSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, DIRTY_TANK_MID_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Dirty tank mid water sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Dirty tank mid water sensor: NO VALID DATA");
    }

    return reading;
}

bool requestDirtyTankHighWaterSensor() {
    Serial.println("[Lavli] Requesting dirty tank HIGH water sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, DIRTY_TANK_HIGH_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request dirty tank high water sensor");
    }

    return success;
}

SensorReading getDirtyTankHighWaterSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, DIRTY_TANK_HIGH_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Dirty tank high water sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Dirty tank high water sensor: NO VALID DATA");
    }

    return reading;
}

// ============================================================================
// Leak Detection Sensor Functions (Digital)
// ============================================================================

bool requestLeakDetectionFront() {
    Serial.println("[Lavli] Requesting leak detection FRONT sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, LEAK_DETECTION_FRONT_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request leak detection front sensor");
    }

    return success;
}

SensorReading getLeakDetectionFront() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, LEAK_DETECTION_FRONT_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Leak detection front: %s (age: %lu ms)\n",
                      reading.digital_value ? "LEAK DETECTED" : "NO LEAK", age);
    } else {
        Serial.println("[Lavli] Leak detection front: NO VALID DATA");
    }

    return reading;
}

bool requestLeakDetectionBack() {
    Serial.println("[Lavli] Requesting leak detection BACK sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, LEAK_DETECTION_BACK_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request leak detection back sensor");
    }

    return success;
}

SensorReading getLeakDetectionBack() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, LEAK_DETECTION_BACK_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Leak detection back: %s (age: %lu ms)\n",
                      reading.digital_value ? "LEAK DETECTED" : "NO LEAK", age);
    } else {
        Serial.println("[Lavli] Leak detection back: NO VALID DATA");
    }

    return reading;
}

// ============================================================================
// Purge Tank Sensor Functions (Digital)
// ============================================================================

bool requestPurgeTankLowSensor() {
    Serial.println("[Lavli] Requesting purge tank LOW sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, PURGE_TANK_LOW_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request purge tank low sensor");
    }

    return success;
}

SensorReading getPurgeTankLowSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, PURGE_TANK_LOW_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Purge tank low sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Purge tank low sensor: NO VALID DATA");
    }

    return reading;
}

bool requestPurgeTankHighSensor() {
    Serial.println("[Lavli] Requesting purge tank HIGH sensor");
    bool success = requestDigitalReading(SENSOR_NODE_2_ADDRESS, PURGE_TANK_HIGH_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request purge tank high sensor");
    }

    return success;
}

SensorReading getPurgeTankHighSensor() {
    SensorReading reading = getDigitalReading(SENSOR_NODE_2_ADDRESS, PURGE_TANK_HIGH_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Purge tank high sensor: %s (age: %lu ms)\n",
                      reading.digital_value ? "DETECTED" : "NOT DETECTED", age);
    } else {
        Serial.println("[Lavli] Purge tank high sensor: NO VALID DATA");
    }

    return reading;
}

// ============================================================================
// Pressure Sensor Functions (Analog)
// ============================================================================

bool requestPressureSensor() {
    Serial.println("[Lavli] Requesting pressure sensor");
    bool success = requestAnalogReading(SENSOR_NODE_2_ADDRESS, PRESSURE_SENSOR_PIN);

    if (!success) {
        Serial.println("[Lavli] Failed to request pressure sensor");
    }

    return success;
}

SensorReading getPressureSensor() {
    SensorReading reading = getAnalogReading(SENSOR_NODE_2_ADDRESS, PRESSURE_SENSOR_PIN);

    if (reading.valid) {
        unsigned long age = millis() - reading.timestamp;
        Serial.printf("[Lavli] Pressure sensor: %u (age: %lu ms)\n",
                      reading.analog_value, age);
    } else {
        Serial.println("[Lavli] Pressure sensor: NO VALID DATA");
    }

    return reading;
}
