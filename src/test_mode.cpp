#include "test_mode.h"

#include <Arduino.h>
#include <PID_Timed.h>
#include <math.h>

#include "config.h"
#include "protocol.h"

namespace {

enum SessionState { IDLE, RUNNING };

double g_setpoint_kpa = 0.0;
double g_current_kpa = 0.0;
double g_pid_output = 0.0; // 0-255 PWM
int g_last_adc = 0;
SessionState g_state = IDLE;
bool g_deflate_open = false;
bool g_at_target = false;
unsigned long g_last_telemetry_ms = 0;
unsigned long g_last_compute_ms = 0;

PID g_pid(&g_current_kpa, &g_pid_output, &g_setpoint_kpa, KP_DEFAULT,
          KI_DEFAULT, KD_DEFAULT, PID_SAMPLE_TIME_S, PID::DIRECT);

void apply_idle_outputs() {
    analogWrite(PIN_INFLATE, 0);
    digitalWrite(PIN_DEFLATE, LOW);
    g_deflate_open = false;
    g_pid_output = 0.0;
}

void on_start() {
    g_state = RUNNING;
    g_pid.clearErrorIntegral();
    g_pid.enable(true);
    g_last_compute_ms = millis();
    protocol_ack("START");
    protocol_log("INFO", "test session started");
}

void on_end() {
    g_state = IDLE;
    g_pid.enable(false);
    analogWrite(PIN_INFLATE, 0);
    digitalWrite(PIN_DEFLATE, HIGH); // bleed off pressure on stop
    g_deflate_open = true;
    g_pid_output = 0.0;
    protocol_ack("END");
    protocol_log("INFO", "test session ended");
}

void on_set_target(double kpa) {
    if (kpa < 0.0) {
        protocol_err("target_negative");
        return;
    }
    g_setpoint_kpa = kpa;
    protocol_ack("SET");
}

void on_gains(double kp, double ki, double kd) {
    if (kp < 0.0 || ki < 0.0 || kd < 0.0) {
        protocol_err("gain_negative");
        return;
    }
    g_pid.SetTunings(kp, ki, kd);
    protocol_ack("GAINS");
}

} // namespace

void test_mode_setup() {
    pinMode(PIN_PRESSURE, INPUT);
    pinMode(PIN_INFLATE, OUTPUT);
    pinMode(PIN_DEFLATE, OUTPUT);
    apply_idle_outputs();

    g_pid.SetOutputLimits(0, 255);
    g_pid.enable(false);

    ProtocolCallbacks cb = {on_start, on_end, on_set_target, on_gains};
    protocol_init(cb);

    protocol_log("INFO", "test mode ready");
}

void test_mode_loop() {
    protocol_poll();

    g_last_adc = analogRead(PIN_PRESSURE);
    g_current_kpa = adc_to_kpa(g_last_adc);

    // Tolerance is meaningless when setpoint is zero, treat zero setpoint as
    // "at target whenever current is essentially zero"
    if (g_setpoint_kpa > 0.0) {
        g_at_target = fabs(g_current_kpa - g_setpoint_kpa) <=
                      g_setpoint_kpa * PRESSURE_TOLERANCE_FRAC;
    } else {
        g_at_target = g_current_kpa < 0.05;
    }

    if (g_state == RUNNING) {
        bool overpressure = g_setpoint_kpa > 0.0 &&
                            g_current_kpa > g_setpoint_kpa * OVERPRESSURE_FRAC;

        if (overpressure) {
            digitalWrite(PIN_DEFLATE, HIGH);
            g_deflate_open = true;
            analogWrite(PIN_INFLATE, 0);
            g_pid_output = 0.0;
            // Keep PID running for state but discard its output this cycle
            g_pid.Compute(PID_SAMPLE_TIME_S);
        } else {
            digitalWrite(PIN_DEFLATE, LOW);
            g_deflate_open = false;
            g_pid.Compute(PID_SAMPLE_TIME_S);
            int pwm = g_at_target ? 0 : (int)g_pid_output;
            if (pwm < 0)
                pwm = 0;
            if (pwm > 255)
                pwm = 255;
            analogWrite(PIN_INFLATE, pwm);
        }
    } else {
        apply_idle_outputs();
    }

    unsigned long now = millis();
    if (now - g_last_telemetry_ms >= TELEMETRY_INTERVAL_MS) {
        g_last_telemetry_ms = now;
        int pwm_out = (int)g_pid_output;
        if (pwm_out < 0)
            pwm_out = 0;
        if (pwm_out > 255)
            pwm_out = 255;
        protocol_emit_telemetry(g_setpoint_kpa, g_current_kpa, g_last_adc,
                                pwm_out, g_deflate_open, g_at_target);
    }
}
