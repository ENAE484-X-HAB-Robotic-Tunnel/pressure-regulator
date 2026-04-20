{ pkgs, ... }:
# NOTE: to set the default package to any file in this directory
# perSystem.self.filename
# FIXME: fill in project build details
pkgs.writeShellApplication {
    name = "pressure-regulator";
    meta.description = "ENAE484 X-HAB Robotic Tunnel: pressure-regulator";

    runtimeInputs = [ ];

    text =
        # bash
        ''
            echo "hello world!"
        '';
}
