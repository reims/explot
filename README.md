# Explot - exploratory data visualisation tool similar to gnuplopt 

Explot uses a command syntax similar to gnuplot. The key difference is rendering on the GPU (OpenGL right now) to allow interactive visualisation of larger datasets.

**Note**: This is just a hobby project of mine and definitely not ready for serious use.

## Usage

Start `explot` in a terminal and start plotting.

## Build

`explot` uses cmake as the build system and nix for dependency management. Use `nix develop --impure` to enter a development shell. Within this shell you can use cmake to build `explot` as usual. `--impure` is needed because OpenGL depends on the hardware. Unfortunately, that also means that `explot` must be started via `nixGL explot` from the development shell, unless you are using NixOS (I use Arch btw).

If you want to build `explot` without nix, you must make sure that cmake can find all the dependencies. See `flake.nix` for a list of dependencies.


