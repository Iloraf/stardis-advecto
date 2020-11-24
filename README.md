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
parallelize its computations.


First ensure that CMake and a C89 compiler that implements the OpenMP 2.0
specification are installed on your system. Then install the RCMake package as
well as all the aforementioned prerequisites. Finally generate the project from
the `cmake/CMakeLists.txt` file by appending to the `CMAKE_PREFIX_PATH`
variable the install directories of its dependencies.

## Release Notes

### v0.5

- Ensure C89 compliance.
- New output format for infra-red rendering.
- Use new stardis-solver 0.11.
- Replace fixed dates by time-ranges as time arguments
  for computations.
- Allow unsteady Green's function computations.
- Model files now include scale parameter.

### v0.4

- Improve C99 compliance.
- Build on Windows systems.
- Use new stardis-solver 0.5.
- Transition to cmake to manage builds.

### v0.3.2

- Add the solve_probe_boundary feature. The solve_probe_boundary VS
  solve_probe selection is automated according the probe-geometry distance.
  solve_probe_boundary is called for probe points closer than 2.1 delta
  from geometry.
- Add flux boundary conditions.

### v0.3.1

Add radiative transfer computations. To achieve this, media gain 2 new parameters: 

- emissivity;
- specular_fraction.

### v0.3

- Upgrade stardis-solver to v0.3.
- Add volumic power sources on solids;
- Allow to use the fp_to_meter parameter of the stardis-solver solve function;
- Add a dump geometry feature. It outputs the geometry as it is sent to
  stardis-solver in VTK format, together with the front and back media and
  boundary conditions information.


### v0.1

- Allow probe computations on conductive-only thermal systems.
- Allow Dirichlet and h.dT boundary conditions.