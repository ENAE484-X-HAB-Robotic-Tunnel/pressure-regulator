{
    description = "ENAE484 X-HAB Robotic Tunnel: pressure-regulator flake";

    inputs = {
        # nixpkgs
        # NOTE: nixpkgs follows ros-overlay, not the other way around!
        # nixpkgs.url = "github:nixos/nixpkgs/release-25.11";
        # nixpkgs-unstable.url = "github:nixos/nixpkgs/nixos-unstable";
        nixpkgs.follows = "nix-ros-overlay/nixpkgs";
        systems.url = "github:nix-systems/default-linux";

        # flake tools (thanks to numtide)
        blueprint = {
            url = "github:numtide/blueprint";
            inputs = {
                nixpkgs.follows = "nixpkgs";
                systems.follows = "systems";
            };
        };
        flake-utils = {
            url = "github:numtide/flake-utils";
            inputs.systems.follows = "systems";
        };
        devshell = {
            url = "github:numtide/devshell";
            inputs.nixpkgs.follows = "nixpkgs";
        };
        treefmt-nix = {
            url = "github:numtide/treefmt-nix";
            inputs.nixpkgs.follows = "nixpkgs";
        };

        # nix + OpenGL
        nixgl = {
            url = "github:nix-community/nixgl";
            inputs = {
                nixpkgs.follows = "nixpkgs";
                flake-utils.follows = "flake-utils";
            };
        };

        # nix + ros (thanks to lopsided98)
        nix-ros-overlay = {
            url = "github:lopsided98/nix-ros-overlay/develop";
            inputs = {
                nixpkgs.url = "github:lopsided98/nixpkgs";
                flake-utils.follows = "flake-utils";
            };
        };
    };

    outputs =
        inputs:
        let
            blueprint = inputs.blueprint {
                inherit inputs;
                nixpkgs.overlays = [
                    inputs.nixgl.overlay
                    inputs.nix-ros-overlay.overlays.default
                    (_final: prev: { vcstool = prev.vcs2l; })
                ];
                prefix = "./nix/";
            };
        in
        blueprint
        // {
            overlays.default = final: _prev: { "enae484-xhab-robotic-tunnel" = blueprint.mkPackagesFor final; };
        };
    nixConfig = {
        extra-substituters = [
            # "https://cache.nixos.org"
            # "https://ros.cachix.org"
            "https://attic.iid.ciirc.cvut.cz/ros"
        ];
        extra-trusted-public-keys = [
            # "cache.nixos.org-1:6NCHdD59X431o0gWypbMrAURkbJ16ZPMQFGspcDShjY="
            # "ros.cachix.org-1:dSyZxI8geDCJrwgvCOHDoAfOm5sV1wCPjBkKL+38Rvo="
            "ros:JR95vUYsShSqfA1VTYoFt1Nz6uXasm5QrcOsGry9f6Q="
        ];
    };
}
