{ pkgs, perSystem, ... }:
perSystem.devshell.mkShell (
    { extraModulesPath, ... }:
    {
        name = "ENAE484 X-HAB Robotic Tunnel: pressure-regulator";
        motd = ''
            {141}ENAE484 X-HAB Robotic Tunnel: pressure-regulator{reset} devshell
            $(type -p menu &>/dev/null && menu)
        '';

        imports = map (p: "${extraModulesPath}/${p}") (
            pkgs.lib.flatten [
                "locale.nix"
                "git/hooks.nix"
            ]
        );

        extra.locale.lang = "en_US.UTF-8";

        git.hooks = {
            enable = true;
            pre-commit.text = "nix fmt";
        };

        devshell.startup = {
            ros2shellcompletion = {
                deps = [ ];
                text =
                    # bash
                    ''
                        # Setup ROS 2 shell completion. Doing it in direnv is useless.
                        if [[ ! $DIRENV_IN_ENVRC ]]; then
                            eval "$(${pkgs.python3Packages.argcomplete}/bin/register-python-argcomplete ros2)"
                            eval "$(${pkgs.python3Packages.argcomplete}/bin/register-python-argcomplete colcon)"
                        fi
                    '';
            };
        };

        commands = [ ];
        packages = with pkgs; [
            # ros2 jazzy
            # (
            #     with pkgs.rosPackages.jazzy;
            #     buildEnv {
            #         wrapPrograms = false;
            #         paths = [
            #             pkgs.colcon
            #             ros-core
            #
            #             # Work around https://github.com/lopsided98/nix-ros-overlay/pull/624
            #             ament-cmake-core
            #             python-cmake-module
            #
            #             # Dependencies from package.xml files
            #             ament-cmake
            #             ament-copyright
            #             ament-flake8
            #             ament-lint-auto
            #             ament-lint-common
            #             ament-pep257
            #             geometry-msgs
            #             python3Packages.matplotlib
            #             python3Packages.numpy
            #             python3Packages.pytest
            #             rclpy
            #             rosidl-default-generators
            #             rosidl-default-runtime
            #             tf-transformations
            #         ];
            #     }
            # )

            # python
            (python3.withPackages (
                ps: with ps; [
                    # python packages here
                    pandas
                    matplotlib
                    numpy
                    pyserial
                    scipy
                    scikit-image
                    tkinter
                ]
            ))

            # c/cpp
            clang-tools
            cmake

            # arduino
            perSystem.self.arduino-cli
            perSystem.nixpkgs-unstable.avrdude
            platformio
        ];

        env = [ ];
    }
)
