{
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {
    self,
    nixpkgs,
    ...
  }: let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
    hyprlandPackage = pkgs.hyprland;
  in {
    packages.${system} = rec {
      hyprfx = pkgs.gcc14Stdenv.mkDerivation {
        pname = "hyprfx";
        version = "0.0.1";
        src = ./.;

        nativeBuildInputs = with pkgs; [pkg-config];
        buildInputs = [hyprlandPackage.dev] ++ hyprlandPackage.buildInputs;

        installPhase = ''
          mkdir -p $out/lib
          install ./hyprfx.so $out/lib/libhyprfx.so
        '';
      };

      default = hyprfx;
    };

    devShells.${system}.default = pkgs.mkShell {
      name = "hyprfx";
      inputsFrom = [self.packages.${system}.hyprfx];
      packages = with pkgs; [
        clang-tools

        (writeShellScriptBin "load" ''
          hyprctl plugin load "$(pwd)/hyprfx.so"
        '')

        (writeShellScriptBin "unload" ''
          hyprctl plugin unload "$(pwd)/hyprfx.so"
        '')

        (writeShellScriptBin "reload" ''
          make && hyprctl plugin unload "$(pwd)/hyprfx.so"; hyprctl plugin load "$(pwd)/hyprfx.so"
        '')

        (writeShellScriptBin "fmt" ''
          clang-format -i src/*.cpp src/*.hpp
        '')
      ];
    };
  };
}
