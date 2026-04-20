{ pkgs, inputs, ... }:
inputs.treefmt-nix.lib.mkWrapper pkgs {
    projectRootFile = "flake.nix";

    # nix
    programs.deadnix.enable = true;
    programs.nixfmt = {
        enable = true;
        indent = 4;
        strict = true;
    };

    # markdown
    programs.prettier = {
        enable = true;
        settings = {
            tabWidth = 4;
        };
    };

    # python
    programs = {
        ruff-check.enable = true;
        ruff-format.enable = true;
    };
}
