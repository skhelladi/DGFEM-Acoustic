# DGFEM for Acoustic Wave Propagation 

[![version](https://img.shields.io/badge/version-1.3.4-red)](https://github.com/skhelladi/DGFEM-CAA/releases/tag/v1.3.4) 
[![compilers](https://img.shields.io/badge/c++-17%20|%2020-27ae60.svg)](https://github.com/skhelladi/DGFEM-CAA/releases/tag/v1.3.4) 

This repository implements a discontinuous Galerkin finite element method (DGFEM) applied to the linearized Euler equations and the acoustic 
perturbation equations. 
The solver is based on [GMSH](http://gmsh.info/) library and supports a wide range of features:

- 1D, 2D, 3D problems
- 4-th order Runge-Kutta
- High order elements
- Absorbing and reflecting boundaries
- Multiple sources support: monopoles, dipoles, quadrupoles and user defined analytical formulation sources 
- Complex geometry and unstructured grid (only triangles (2D) and tetrahedrons (3D) elements are supported)
- VTK Post-processing (use [Paraview](https://www.paraview.org/)) 

<!-- | Auditorium     | Isosurfaces     | Bulk|
| ------------- |:-------------:| :-------------:| 
| <img src="https://gitlab.ensam.eu/khelladi/DGFEM-Acoustic/-/raw/b1026a1c6b9d312d02f6f70e776ed98e054ef00a/assets/auditorium_source2_2.png" width="400" height="200" />    | <img src="https://gitlab.ensam.eu/khelladi/DGFEM-Acoustic/-/raw/b1026a1c6b9d312d02f6f70e776ed98e054ef00a/assets/auditorium_source_iso1.png" width="400" height="200" />  | <img src="https://gitlab.ensam.eu/khelladi/DGFEM-Acoustic/-/raw/b1026a1c6b9d312d02f6f70e776ed98e054ef00a/assets/auditorium_source_bulk1.png" width="400" height="200" /> | -->


## Getting Started
 	
### Prerequisites

First, make sure the following libraries are installed. If you are running a linux distribution (ubuntu, debian, ...), an installation [script](https://github.com/skhelladi/DGFEM-CAA/blob/main/build.sh) is provided. 

```
Gmsh (v4.10.5)
Eigen (v3.4.0)
Lapack
Blas
OpenMP
Libtbb
VTK (v9.x)
```

### Installing

```
git clone https://github.com/skhelladi/DGFEM-CAA.git
cd DGFEM-CAA
mkdir build && cd build
cmake ../ -DCMAKE_BUILD_TYPE=Release  -G "Unix Makefiles" -DGMSH_INCLUDE_DIRS="../gmsh-4.10.5-Linux64-sdk/include" -DGMSH_LIBRARIES="../gmsh-4.10.5-Linux64-sdk/lib/libgmsh.so" -DGMSH_EXECUTABLE="../gmsh-4.10.5-Linux64-sdk/bin/gmsh" -DEIGEN_INCLUDE_DIRS="/usr/include/eigen3"
make -j4
```

or simply use build.sh 
```
git clone https://github.com/skhelladi/DGFEM-CAA.git
cd DGFEM-CAA
sh build.sh
```


## Running the tests
Once the sources sucessfully build, you can start using with the solver. It required two arguments: a mesh file created with Gmsh and a config file containing the solver options. Examples of mesh files and config files are given [here](https://github.com/skhelladi/DGFEM-CAA/tree/development/doc).

```
cd bin
./dgalerkin mymesh.msh myconfig.conf
```

### Minimal working example

2D propagation of an Gaussian initial condition over a square.

```
cd build/bin
./dgalerkin ../../doc/2d/square.msh ../../doc/config/config.conf 
```

or configure run_caa batch file with the right mesh and configurations files.

```
sh run_caa 
```
## Configuration file example
<!-- python style text highlight -->
```python 
# Initial, final time and time step(t>0)
timeStart=0
timeEnd=0.05
timeStep=0.00001

# Saving rate:
timeRate=0.001

# Element Type:
# ["Lagrange", "IsoParametric", ...]
elementType=Lagrange

# Time integration method:
# ["Euler1", "Euler2", "Runge-Kutta"...]
timeIntMethod=Runge-Kutta

# Boundary condition:
# /!\ The physical group name must match the Gmsh name (case sensitive)
# MyPhysicalName = Absorbing or Reflecting
Reflecting = Reflecting
Absorbing = Absorbing

# Number of thread
numThreads=12

# Mean Flow parameters
v0_x = -30
v0_y = 30
v0_z = 0
rho0 = 1.225
c0 = 100

# Source:
# name = fct,x,y,z,size,intensity,frequency,phase,duration
# - fct supported = [monopole, dipole, quadrupole, formula, file (csv, wav)]
# - if fct = formula => name = fct,"formula expr",x,y,z,size,duration (ex: formulat = 0.1 * sin(2 * pi * 50 * t))
# - if fct = file => name = fct,"filename",x,y,z,size
#	suported file formats are : csv, wav
# - (x,y,z) = source position
# - intensity = source intensity
# - frequency = source frequency
# NB: Extended source or Multiple sources are supported.
#     (source1 = ..., source2 = ...) indice must change.
source1 = formula, "0.1 * sin(2 * pi * 50 * t)", 0.0,0.0,0.0, 0.1, 0.1
source2 = monopole, 0.0,0.0,0.0, 0.1, 0.1,50,0,0.1
source3 = file,"data/data.csv", 0.0,0.0,0.0, 0.1
source4 = file,"data/data.wav", 0.0,0.0,0.0, 0.1
# source4 = udf, "-0.1 * sin(2 * pi * 50 * t)", -0.5,0.0,0.0, 0.1, 0.1

# Initial condition:
# name = gaussian,x,y,z, size, amplitude
# - fct supported = [gaussian]
# - (x,y,z) = position
# - amplitude = initial amplitude
# NB: Multiple CI are supported and recursively added.
#     (initial condition1 = ..., initial condition = ...)
# initialCondtition1 = gaussian, 0,0,0,1,1

# Observers position:
# name = x,y,z, size
# - (x,y,z) = position
# NB: Multiple observers are supported and recursively added.
#     (observer1 = ...; observer2 = ...)
observer1 = 2.11792,0.00340081,0.0,0.1
observer2 = -2.11792,0.00340081,0.0,0.1


```

## Author
* Sofiane Khelladi

#### Forked from code developed by
* Pierre-Olivier Vanberg
* Martin Lacroix
* Tom Servais

Link : https://github.com/pvanberg/DGFEM-Acoustic

