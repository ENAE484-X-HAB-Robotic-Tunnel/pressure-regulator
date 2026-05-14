#pragma once

#include <Arduino.h>

// Pin assignments
#define PIN_PRESSURE A0 // analog pressure sensor input
#define PIN_DEFLATE A1  // deflation solenoid (binary on/off)
#define PIN_INFLATE A2  // inflation solenoid (PWM)
#define PIN_MODE_SELECT                                                        \
    11 // PB3 / D11: LOW at boot -> test mode (uses internal pull-up)

// Pressure sensor: 0-5 PSI transducer, 0.5-4.5 V output, 5 V VRef
#define SENSOR_MIN_VOLTAGE 0.5f
#define SENSOR_MAX_VOLTAGE 4.5f
#define SENSOR_MAX_PSI 5.0f
#define SENSOR_AMBIENT_KPA 0.0294863881f
#define PSI_TO_KPA 6.89476f

// 10-bit ADC against default 5 V analog reference
#define ADC_VREF 5.0f
#define ADC_MAX 1023

// Control thresholds
#define PRESSURE_TOLERANCE_FRAC 0.005f // +/- 0.5 % of setpoint
#define OVERPRESSURE_FRAC 1.05f        // deflate when above 105 % of setpoint

// Loop timing
#define TELEMETRY_INTERVAL_MS 50UL // 20 Hz
#define PID_SAMPLE_TIME_S 0.05     // 50 ms

// Default PID gains (test GUI can override at runtime)
#define KP_DEFAULT 2.0
#define KI_DEFAULT 0.5
#define KD_DEFAULT 0.1

// Mode-select detection window after reset
#define MODE_SELECT_WINDOW_MS 1000UL

/*
 * Converts Sensor Readings from the Arduino's Analog-to-Digital Converter (ADC) into usable values in KPa
 *
 * @param[in] adc Sensor reading from Arduino's ADC
 * @return Sensor reading in KPa
 */
inline float adc_to_kpa(int adc) {
    float voltage = (float)adc * (ADC_VREF / (float)ADC_MAX);
    float frac = (voltage - SENSOR_MIN_VOLTAGE) /
                 (SENSOR_MAX_VOLTAGE - SENSOR_MIN_VOLTAGE);

    // Readings outside [0.0:1.0] are impossible and can be attributed to noise
    if (frac < 0.0f)
        frac = 0.0f;
    if (frac > 1.0f)
        frac = 1.0f;

    return frac * SENSOR_MAX_PSI * PSI_TO_KPA;

}
