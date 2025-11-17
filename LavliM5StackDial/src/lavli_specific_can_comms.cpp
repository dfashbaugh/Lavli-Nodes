#include "lavli_specific_can_comms.h"
#include <Arduino.h>

// ============================================================================
// 120V Controller Functions
// ============================================================================

bool toggleDryer(bool on) {
    uint8_t command = on ? ACTIVATE_CMD : DEACTIVATE_CMD;

    Serial.printf("[Lavli] %s dryer (port %d)\n",
                  on ? "Activating" : "Deactivating", DRYER_PORT);

    bool success = sendOutputCommand(CONTROLLER_120V_1_ADDRESS, command, DRYER_PORT);

    if (success) {
        Serial.println("[Lavli] Dryer command sent successfully");
    } else {
        Serial.println("[Lavli] Failed to send dryer command");
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
