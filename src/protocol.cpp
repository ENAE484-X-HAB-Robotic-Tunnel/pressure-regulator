#include "protocol.h"

#include <stdlib.h>
#include <string.h>

namespace {

constexpr uint8_t LINE_BUF_SIZE = 64;

ProtocolCallbacks g_cb = {nullptr, nullptr, nullptr, nullptr};
char g_line[LINE_BUF_SIZE];
uint8_t g_line_len = 0;

void handle_line(char *line) {
    // Skip leading whitespace, trim trailing CR
    while (*line == ' ' || *line == '\t')
        line++;
    char *end = line + strlen(line);
    while (end > line &&
           (end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) {
        *--end = '\0';
    }
    if (*line == '\0')
        return;

    char *sp = strchr(line, ' ');
    if (sp)
        *sp = '\0';
    char *args = sp ? sp + 1 : nullptr;

    if (strcmp(line, "PING") == 0) {
        Serial.println(F("PONG"));
        return;
    }
    if (strcmp(line, "START") == 0) {
        if (g_cb.on_start)
            g_cb.on_start();
        return;
    }
    if (strcmp(line, "END") == 0) {
        if (g_cb.on_end)
            g_cb.on_end();
        return;
    }
    if (strcmp(line, "SET") == 0) {
        if (!args) {
            protocol_err("set_missing_arg");
            return;
        }
        double kpa = atof(args);
        if (g_cb.on_set_target)
            g_cb.on_set_target(kpa);
        return;
    }
    if (strcmp(line, "GAINS") == 0) {
        if (!args) {
            protocol_err("gains_missing_args");
            return;
        }
        char *a = args;
        char *b = strchr(a, ' ');
        if (!b) {
            protocol_err("gains_missing_args");
            return;
        }
        *b++ = '\0';
        char *c = strchr(b, ' ');
        if (!c) {
            protocol_err("gains_missing_args");
            return;
        }
        *c++ = '\0';
        double kp = atof(a), ki = atof(b), kd = atof(c);
        if (g_cb.on_gains)
            g_cb.on_gains(kp, ki, kd);
        return;
    }

    protocol_err("unknown_command");
}

void print_double(double v, uint8_t decimals) { Serial.print(v, decimals); }

} // namespace

void protocol_init(const ProtocolCallbacks &cb) {
    g_cb = cb;
    g_line_len = 0;
}

void protocol_poll() {
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0)
            break;
        if (c == '\n') {
            g_line[g_line_len] = '\0';
            handle_line(g_line);
            g_line_len = 0;
        } else if (g_line_len < LINE_BUF_SIZE - 1) {
            g_line[g_line_len++] = (char)c;
        } else {
            // Overflow: discard the line.
            g_line_len = 0;
            protocol_err("line_too_long");
        }
    }
}

void protocol_emit_telemetry(double target_kpa, double current_kpa, int adc_raw,
                             int inflate_pwm, bool deflate_open,
                             bool at_target) {
    Serial.print(F("T,"));
    Serial.print(millis());
    Serial.print(',');
    print_double(target_kpa, 3);
    Serial.print(',');
    print_double(current_kpa, 3);
    Serial.print(',');
    Serial.print(adc_raw);
    Serial.print(',');
    Serial.print(inflate_pwm);
    Serial.print(',');
    Serial.print(deflate_open ? 1 : 0);
    Serial.print(',');
    Serial.println(at_target ? 1 : 0);
}

void protocol_log(const char *level, const char *msg) {
    Serial.print(F("LOG,"));
    Serial.print(level);
    Serial.print(',');
    Serial.println(msg);
}

void protocol_ack(const char *cmd) {
    Serial.print(F("ACK,"));
    Serial.println(cmd);
}

void protocol_err(const char *reason) {
    Serial.print(F("ERR,"));
    Serial.println(reason);
}
