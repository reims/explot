# Explot - exploratory data visualisation tool similar to gnuplopt 

Explot uses a command syntax similar to gnuplot. The key difference is rendering on the GPU (OpenGL right now) to allow interactive visualisation of larger datasets.

**Note**: This is just a hobby project of mine and definitely not ready for serious use.

## Usage

Start `explot` in a terminal and start plotting.

## Build

`explot` uses cmake as the build system and nix for dependency management. Use `nix develop --impure` to enter a development shell. Within this shell you can use cmake to build `explot` as usual. `--impure` is needed because OpenGL depends on the hardware. Unfortunately, that also means that `explot` must be started via `nixGL explot` from the development shell, unless you are using NixOS (I use Arch btw).

If you want to build `explot` without nix, you must make sure that cmake can find all the dependencies. See `flake.nix` for a list of dependencies.


## Screenshots

### Plot example
![`plot sin(x) with lines, cos(x) with lines`](./img/plot_example.png)

### Splot example
![`splot sin(x/3)*cos(x/3) with lines`](./img/splot_example.png)

## Features

### Gnuplot featues

- [x] Basic commands `plot`, `splot`, `set` and `show`
- [x] Plotting datafiles, as columns and as a matrix
- [x] Plotting functions and parametric curves/surfaces
- [x] Datetime handling
- Plotting styles:
  - [x] points
  - [x] lines
  - [x] impulses (2d only)
  - [ ] pm3d
  - [ ] hiddden3d
  - [ ] vectors
  - [ ] error bars
  
### Additional features

- [x] Pressing `q` closes plot window
- [ ] Parameters for expressions that can be changed interactively. They will probably use a syntax like `$p1`, `$p2` etc similar to columns.

### Gnuplot features that are out-of-scope

- Creating images for publication. `gnuplot` is a great tool for that. `explot` is for interactive visualisation. In particluar, `explot` will never support `set output` unless the scope changes.
  
