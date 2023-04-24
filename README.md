# stardis

## Overview

Stardis is a software dedicated to the resolution of coupled
convective-conductive-radiative thermal problems in 3D environments.
It is based on [stardis-solver](https://gitlab.com/meso-star/stardis-solver)
and exposes some of the main features of the solver in an easy to use way.
Using stardis is a practical way of carrying out thermal studies on CAD
models which can be exported from Salomé or other similar software.

## How to build

Stardis relies on the [CMake](http://www.cmake.org) and the
[RCMake](https://gitlab.com/vaplv/rcmake/) package to build.
It also depends on the
[RSys](https://gitlab.com/vaplv/rsys/),
[star-3d](https://gitlab.com/meso-star/star-3d/),
[star-enclosures-3d](https://gitlab.com/meso-star/star-enclosures-3d/),
[star-geometry-3d](https://gitlab.com/meso-star/star-geometry-3d/),
[star-stl](https://gitlab.com/meso-star/star-stl) and
[stardis-solver](https://gitlab.com/meso-star/stardis-solver) libraries
as well as on the [OpenMP](http://www.openmp.org) 2.0 specification to
parallelize its computations.  It may depend on
[OpenMPI](https://www.open-mpi.org/) 2.0 if distributed memory parallelism
is enabled via the `ENABLE_MPI` variable of the CMake file.

First ensure that CMake and a C compiler are installed on your system.
Then install the RCMake package as well as all the aforementioned
prerequisites. Finally generate the project from
the `cmake/CMakeLists.txt` file by appending to the `CMAKE_PREFIX_PATH`
variable the install directories of its dependencies.

## Release notes

## Version 0.8

- Add a new option to support non-linear radiative transfer computations.
- Changes in input file's format to support non-linear radiative transfer by
  adding reference temperatures on interfaces.
- Add optional support for MPI (must be enabled at compile time, default is OFF).
- Change random number generator type to use Threefry.
- Change the format of binary Green files. A new public header file is now
  installed that describes all types involved in binary Green files.
- Fix a crash on an exit-on-error execution path.
- Fix parsing of command-line options.

### Version 0.7.2

Fix the binary file format of the green function: the fileformat has been
updated without incrementing the version of the serialised data.

### Version 0.7.1

Fix debug build.

### Version 0.7

- Remove the boundary condition `T_BOUNDARY_FOR_FLUID`: it was exactly the same
  than `H_BOUNDARY_FOR_FLUID` that should now be used instead.
- Sets the required version of Star-SampPling to 0.12. This version fixes
  compilation errors with gcc 11 but introduces API breaks.

### Version 0.6

- Add thermal contact resistances.
- Add serialization for random generator's state.
- Bugfixes in arguments parsing.
- Fix Green output file padding.

### Version 0.5.1

- Fix a memleak
- Add a file format version in binary Green files.
- Man improvement.

### Version 0.5

- Ensure C89 compliance.
- New output format for infra-red rendering.
- Use new stardis-solver 0.11.
- Replace fixed dates by time-ranges as time arguments
  for computations.
- Allow unsteady Green's function computations.
- Model files now include scale parameter.

### Version 0.4

- Improve C99 compliance.
- Build on Windows systems.
- Use new stardis-solver 0.5.
- Transition to cmake to manage builds.

### Version 0.3.2

- Add the `solve_probe_boundary` feature. The `solve_probe_boundary` VS
  `solve_probe` selection is automated according the probe-geometry distance.
  `solve_probe_boundary` is called for probe points closer than 2.1 delta
  from geometry.
- Add flux boundary conditions.

### Version 0.3.1

Add radiative transfer computations. To achieve this, media gain 2 new parameters: 

- emissivity;
- `specular_fraction`.

### Version 0.3

- Upgrade stardis-solver to v0.3.
- Add volumic power sources on solids;
- Allow to use the `fp_to_meter` parameter of the stardis-solver solve function;
- Add a dump geometry feature. It outputs the geometry as it is sent to
  stardis-solver in VTK format, together with the front and back media and
  boundary conditions information.


### Version 0.1

- Allow probe computations on conductive-only thermal systems.
- Allow Dirichlet and h.dT boundary conditions.

## License

Copyright (C) 2018-2023 |Méso|Star> (<contact@meso-star.com>). Stardis is free
software released under the GPL v3+ license: GNU GPL version 3 or later. You
are welcome to redistribute it under certain conditions; refer to the COPYING
file for details.

