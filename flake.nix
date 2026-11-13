{
  description = "CSC5101 lab env";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }: let
    system = "aarch64-darwin";
  in {
    devShells."${system}".default = let
      pkgs = import nixpkgs { inherit system; };
    in pkgs.mkShell {

      packages = with pkgs; [
        # build
        gcc
      ];

      shellHook = ''
      '';

    };
  };
}

