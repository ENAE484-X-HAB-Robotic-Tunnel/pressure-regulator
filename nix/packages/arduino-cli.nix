{ pkgs, inputs, ... }:
pkgs.wrapArduinoCLI {
    libraries = with pkgs.arduinoLibraries; [
        (inputs.arduino-nix.latestVersion Arduino_Sensorkit)
        (inputs.arduino-nix.latestVersion PID_Timed)
        # (inputs.arduino-nix.latestVersion pkgs.arduinoLibraries."Adafruit PWM Servo Driver Library")
    ];

    packages = with pkgs.arduinoPackages; [ platforms.arduino.avr."1.6.23" ];
}
