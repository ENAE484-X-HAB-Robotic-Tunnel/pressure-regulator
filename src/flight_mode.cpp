#include "flight_mode.h"

#include <Arduino.h>

#include "config.h"
#include "protocol.h"

// Flight mode is deferred until test-mode validation is completed
// For now the firmware just keeps the solenoids safe and emits a periodic
// warning so the operator knows the board booted in the wrong mode

namespace {
unsigned long g_last_warn_ms = 0;
}

void flight_mode_setup() {
    pinMode(PIN_INFLATE, OUTPUT);
    pinMode(PIN_DEFLATE, OUTPUT);
    analogWrite(PIN_INFLATE, 0);
    digitalWrite(PIN_DEFLATE, HIGH); // safe default: bleed off
    protocol_log("WARN", "flight mode not implemented");
}

void flight_mode_loop() {
    unsigned long now = millis();
    if (now - g_last_warn_ms >= 5000UL) {
        g_last_warn_ms = now;
        protocol_log("WARN", "flight mode not implemented");
    }
}
