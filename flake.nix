{
  description = "Explot - exploratory plotting program similar to gnuplot";
  
  inputs.nixgl.url = "github:guibou/nixGL";
  outputs = { nixpkgs, nixgl, ... }@inputs:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs { inherit system; overlays = [ nixgl.overlay ]; };
    in
      {
        devShell.${system}= pkgs.mkShell
          {
            nativeBuildInputs = with pkgs; [
              clang-tools_19
              clang_19
              llvmPackages_19.libcxx
              lldb_15
              cmake
              fmt
              ctre
              glfw
              glew
              glew.dev
              freetype
              libGL
              libglvnd
              linenoise-ng
              fontconfig
              glm
              howard-hinnant-date
              # pkgs. IS necessary here. Otherwise nixgl as passed to outputs is used.
              pkgs.nixgl.auto.nixGLDefault
            ];

            shellHook = ''
CXX=clang++
CC=clang
'';

          };
        
      };
}
