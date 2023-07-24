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
              clang-tools_15
              clang_15
              llvmPackages_15.libcxx
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
              # pkgs. IS necessary here. Otherwise nixgl as passed to outputs is used.
              pkgs.nixgl.auto.nixGLDefault
            ];

            shellHook = ''
CXX=clang++
'';

          };
        
      };
}
