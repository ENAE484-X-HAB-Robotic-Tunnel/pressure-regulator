#include <Arduino.h>

#include "config.h"
#include "flight_mode.h"
#include "test_mode.h"

// Runtime selector is PIN_MODE_SELECT pin polled during first sec after reset

namespace {
bool g_in_test_mode = false;
}

void setup() {
    Serial.begin(115200);

    pinMode(PIN_MODE_SELECT, INPUT_PULLUP);
    pinMode(PIN_INFLATE, OUTPUT);
    pinMode(PIN_DEFLATE, OUTPUT);
    analogWrite(PIN_INFLATE, 0);
    digitalWrite(PIN_DEFLATE, LOW);

    // Internal pull-up holds the pin HIGH; shorting PIN_MODE_SELECT to GND
    // (e.g. with a screwdriver between PB3/D11 and an adjacent GND pin) selects
    // test mode at boot
    unsigned long start = millis();
    while (millis() - start < MODE_SELECT_WINDOW_MS) {
        if (digitalRead(PIN_MODE_SELECT) == LOW) {
            g_in_test_mode = true;
            break;
        }
    }

    if (g_in_test_mode) {
        test_mode_setup();
    } else {
        flight_mode_setup();
    }
}

void loop() {
    if (g_in_test_mode) {
        test_mode_loop();
    } else {
        flight_mode_loop();
    }
}
