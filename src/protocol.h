#pragma once

#include <Arduino.h>

struct ProtocolCallbacks {
    void (*on_start)();
    void (*on_end)();
    void (*on_set_target)(double kpa);
    void (*on_gains)(double kp, double ki, double kd);
};

void protocol_init(const ProtocolCallbacks &cb);

// Drain serial input & dispatch any complete lines to the registered callbacks
// Non-blocking: returns immediately if no full line is available
void protocol_poll();

void protocol_emit_telemetry(double target_kpa, double current_kpa, int adc_raw,
                             int inflate_pwm, bool deflate_open,
                             bool at_target);

void protocol_log(const char *level, const char *msg);
void protocol_ack(const char *cmd);
void protocol_err(const char *reason);
