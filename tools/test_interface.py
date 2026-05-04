#!/usr/bin/env python3
"""Tkinter test-mode interface for the pressure regulator firmware.

Connects over USB serial to an Arduino Nano running test mode and provides:
  - Port selection / connect / disconnect
  - Start / End test session
  - Target pressure entry (kPa), locked while regulating
  - Live telemetry display (target, current, ADC, PWM, deflate state, at-target)
  - PID gain editing
  - Scrolling log of LOG/ACK/ERR lines

Protocol matches src/protocol.cpp:
  Outbound: PING | START | END | SET <kpa> | GAINS <kp> <ki> <kd>
  Inbound:  T,<ms>,<target>,<current>,<adc>,<pwm>,<deflate>,<at_target>
            LOG,<level>,<msg> | ACK,<cmd> | ERR,<reason> | PONG
"""

from __future__ import annotations

import queue
import threading
import time
import tkinter as tk
from tkinter import ttk

import serial
import serial.tools.list_ports

BAUD = 115200
RECONNECT_GRACE_S = 2.0


class SerialReader(threading.Thread):
    def __init__(self, ser: serial.Serial, out_queue: queue.Queue) -> None:
        super().__init__(daemon=True)
        self._ser = ser
        self._queue = out_queue
        self._stop = threading.Event()

    def stop(self) -> None:
        self._stop.set()

    def run(self) -> None:
        buf = bytearray()
        while not self._stop.is_set():
            try:
                chunk = self._ser.read(64)
            except serial.SerialException as e:
                self._queue.put(("disconnect", str(e)))
                return
            except OSError as e:
                self._queue.put(("disconnect", str(e)))
                return
            if not chunk:
                continue
            buf.extend(chunk)
            while b"\n" in buf:
                line, _, rest = buf.partition(b"\n")
                buf = bytearray(rest)
                text = line.decode("utf-8", errors="replace").strip()
                if text:
                    self._queue.put(("line", text))


class App:
    def __init__(self, root: tk.Tk) -> None:
        self.root = root
        root.title("Pressure Regulator Test Interface")

        self.ser: serial.Serial | None = None
        self.reader: SerialReader | None = None
        self.queue: queue.Queue = queue.Queue()

        # Session state derived from telemetry
        self.session_active = False
        self.at_target = False

        self._build_ui()
        self._refresh_ports()
        self._set_connected(False)
        self.root.after(50, self._drain_queue)

    # ---------- UI construction ----------

    def _build_ui(self) -> None:
        pad = {"padx": 6, "pady": 4}

        conn = ttk.LabelFrame(self.root, text="Connection")
        conn.grid(row=0, column=0, sticky="ew", **pad)
        ttk.Label(conn, text="Port:").grid(row=0, column=0, **pad)
        self.port_var = tk.StringVar()
        self.port_combo = ttk.Combobox(
            conn, textvariable=self.port_var, width=24, state="readonly"
        )
        self.port_combo.grid(row=0, column=1, **pad)
        ttk.Button(conn, text="Refresh", command=self._refresh_ports).grid(
            row=0, column=2, **pad
        )
        self.connect_btn = ttk.Button(
            conn, text="Connect", command=self._toggle_connect
        )
        self.connect_btn.grid(row=0, column=3, **pad)
        self.status_var = tk.StringVar(value="Disconnected")
        ttk.Label(conn, textvariable=self.status_var, foreground="red").grid(
            row=0, column=4, **pad
        )

        sess = ttk.LabelFrame(self.root, text="Session")
        sess.grid(row=1, column=0, sticky="ew", **pad)
        self.start_btn = ttk.Button(sess, text="Start Test", command=self._start_test)
        self.start_btn.grid(row=0, column=0, **pad)
        self.end_btn = ttk.Button(sess, text="End Test", command=self._end_test)
        self.end_btn.grid(row=0, column=1, **pad)

        tgt = ttk.LabelFrame(self.root, text="Target Pressure (kPa)")
        tgt.grid(row=2, column=0, sticky="ew", **pad)
        self.target_var = tk.StringVar(value="0.0")
        self.target_entry = ttk.Entry(tgt, textvariable=self.target_var, width=12)
        self.target_entry.grid(row=0, column=0, **pad)
        self.set_btn = ttk.Button(tgt, text="Set", command=self._set_target)
        self.set_btn.grid(row=0, column=1, **pad)
        self.target_lock_var = tk.StringVar(value="")
        ttk.Label(tgt, textvariable=self.target_lock_var, foreground="gray").grid(
            row=0, column=2, **pad
        )

        tel = ttk.LabelFrame(self.root, text="Live Telemetry")
        tel.grid(row=3, column=0, sticky="ew", **pad)
        self.tel_target = tk.StringVar(value="--.- kPa")
        self.tel_current = tk.StringVar(value="--.- kPa")
        self.tel_adc = tk.StringVar(value="---")
        self.tel_pwm = tk.StringVar(value="---")
        self.tel_deflate = tk.StringVar(value="CLOSED")
        self.tel_at_target = tk.StringVar(value="NO")
        rows = [
            ("Target:", self.tel_target),
            ("Current:", self.tel_current),
            ("Sensor (ADC):", self.tel_adc),
            ("Inflate PWM:", self.tel_pwm),
            ("Deflate:", self.tel_deflate),
            ("At target:", self.tel_at_target),
        ]
        for i, (label, var) in enumerate(rows):
            ttk.Label(tel, text=label, width=14, anchor="w").grid(
                row=i, column=0, **pad
            )
            ttk.Label(
                tel, textvariable=var, width=18, anchor="w", font=("TkFixedFont",)
            ).grid(row=i, column=1, **pad)

        gains = ttk.LabelFrame(self.root, text="PID Gains")
        gains.grid(row=4, column=0, sticky="ew", **pad)
        self.kp_var = tk.StringVar(value="2.0")
        self.ki_var = tk.StringVar(value="0.5")
        self.kd_var = tk.StringVar(value="0.1")
        for i, (label, var) in enumerate(
            [("Kp", self.kp_var), ("Ki", self.ki_var), ("Kd", self.kd_var)]
        ):
            ttk.Label(gains, text=label).grid(row=0, column=2 * i, **pad)
            ttk.Entry(gains, textvariable=var, width=8).grid(
                row=0, column=2 * i + 1, **pad
            )
        self.apply_gains_btn = ttk.Button(
            gains, text="Apply", command=self._apply_gains
        )
        self.apply_gains_btn.grid(row=0, column=6, **pad)

        log = ttk.LabelFrame(self.root, text="Log")
        log.grid(row=5, column=0, sticky="nsew", **pad)
        self.log_text = tk.Text(
            log, height=10, width=70, state="disabled", font=("TkFixedFont",)
        )
        self.log_text.grid(row=0, column=0, sticky="nsew")
        scroll = ttk.Scrollbar(log, orient="vertical", command=self.log_text.yview)
        scroll.grid(row=0, column=1, sticky="ns")
        self.log_text.configure(yscrollcommand=scroll.set)

        self.root.columnconfigure(0, weight=1)
        self.root.rowconfigure(5, weight=1)
        log.columnconfigure(0, weight=1)
        log.rowconfigure(0, weight=1)

    # ---------- connection ----------

    def _refresh_ports(self) -> None:
        ports = sorted(
            p.device
            for p in serial.tools.list_ports.comports()
            if "USB" in p.device or "ACM" in p.device or "usbserial" in p.device.lower()
        )
        if not ports:
            ports = sorted(p.device for p in serial.tools.list_ports.comports())
        self.port_combo["values"] = ports
        if ports and not self.port_var.get():
            self.port_var.set(ports[0])

    def _toggle_connect(self) -> None:
        if self.ser is None:
            self._connect()
        else:
            self._disconnect()

    def _connect(self) -> None:
        port = self.port_var.get().strip()
        if not port:
            self._append_log("no port selected")
            return
        try:
            self.ser = serial.Serial(port, BAUD, timeout=0.1)
        except (serial.SerialException, OSError) as e:
            self._append_log(f"connect failed: {e}")
            self.ser = None
            return
        # Arduino Nano resets when DTR toggles on open; give it time before
        # the first PING is sent
        time.sleep(RECONNECT_GRACE_S)
        self.reader = SerialReader(self.ser, self.queue)
        self.reader.start()
        self._set_connected(True)
        self._append_log(f"connected to {port}")
        self._send_line("PING")

    def _disconnect(self, reason: str | None = None) -> None:
        if self.reader is not None:
            self.reader.stop()
            self.reader = None
        if self.ser is not None:
            try:
                self.ser.close()
            except Exception:
                pass
            self.ser = None
        self.session_active = False
        self.at_target = False
        self._set_connected(False)
        if reason:
            self._append_log(f"disconnected: {reason}")
        else:
            self._append_log("disconnected")

    def _set_connected(self, connected: bool) -> None:
        if connected:
            self.connect_btn.configure(text="Disconnect")
            self.status_var.set("Connected")
            self.start_btn.state(["!disabled"])
            self.end_btn.state(["!disabled"])
            self.set_btn.state(["!disabled"])
            self.apply_gains_btn.state(["!disabled"])
        else:
            self.connect_btn.configure(text="Connect")
            self.status_var.set("Disconnected")
            for btn in (
                self.start_btn,
                self.end_btn,
                self.set_btn,
                self.apply_gains_btn,
            ):
                btn.state(["disabled"])
        self._update_target_lock()

    # ---------- commands ----------

    def _send_line(self, text: str) -> None:
        if self.ser is None:
            self._append_log("not connected")
            return
        try:
            self.ser.write((text + "\n").encode("utf-8"))
        except (serial.SerialException, OSError) as e:
            self._disconnect(reason=str(e))

    def _start_test(self) -> None:
        self._send_line("START")

    def _end_test(self) -> None:
        self._send_line("END")

    def _set_target(self) -> None:
        try:
            kpa = float(self.target_var.get())
        except ValueError:
            self._append_log("target not a number")
            return
        if kpa < 0:
            self._append_log("target cannot be negative")
            return
        self._send_line(f"SET {kpa:.3f}")

    def _apply_gains(self) -> None:
        try:
            kp = float(self.kp_var.get())
            ki = float(self.ki_var.get())
            kd = float(self.kd_var.get())
        except ValueError:
            self._append_log("gain not a number")
            return
        self._send_line(f"GAINS {kp:.4f} {ki:.4f} {kd:.4f}")

    # ---------- queue draining ----------

    def _drain_queue(self) -> None:
        try:
            while True:
                kind, payload = self.queue.get_nowait()
                if kind == "line":
                    self._handle_line(payload)
                elif kind == "disconnect":
                    self._disconnect(reason=payload)
        except queue.Empty:
            pass
        self.root.after(50, self._drain_queue)

    def _handle_line(self, line: str) -> None:
        if line.startswith("T,"):
            self._handle_telemetry(line)
            return
        # Everything else lands in the log.
        self._append_log(line)
        if line.startswith("ACK,START"):
            self.session_active = True
            self._update_target_lock()
        elif line.startswith("ACK,END"):
            self.session_active = False
            self._update_target_lock()

    def _handle_telemetry(self, line: str) -> None:
        parts = line.split(",")
        if len(parts) != 8:
            return
        try:
            _, _ms, target, current, adc, pwm, deflate, at_target = parts
            target_f = float(target)
            current_f = float(current)
            adc_i = int(adc)
            pwm_i = int(pwm)
            deflate_i = int(deflate)
            at_target_i = int(at_target)
        except ValueError:
            return
        self.tel_target.set(f"{target_f:6.2f} kPa")
        self.tel_current.set(f"{current_f:6.2f} kPa")
        self.tel_adc.set(str(adc_i))
        self.tel_pwm.set(str(pwm_i))
        self.tel_deflate.set("OPEN" if deflate_i else "CLOSED")
        self.tel_at_target.set("YES" if at_target_i else "NO")
        self.at_target = bool(at_target_i)
        self._update_target_lock()

    def _update_target_lock(self) -> None:
        # Locked: a session is running AND we are not yet within tolerance
        # Unlocked: idle session OR pressure has stabilized
        if self.ser is None:
            self.target_entry.state(["disabled"])
            self.target_lock_var.set("")
            return
        if self.session_active and not self.at_target:
            self.target_entry.state(["disabled"])
            self.target_lock_var.set("(locked: regulating)")
        else:
            self.target_entry.state(["!disabled"])
            self.target_lock_var.set("(unlocked)")

    # ---------- log ----------

    def _append_log(self, msg: str) -> None:
        ts = time.strftime("%H:%M:%S")
        self.log_text.configure(state="normal")
        self.log_text.insert("end", f"[{ts}] {msg}\n")
        self.log_text.see("end")
        self.log_text.configure(state="disabled")


def main() -> None:
    root = tk.Tk()
    app = App(root)
    try:
        root.mainloop()
    finally:
        app._disconnect()


if __name__ == "__main__":
    main()
