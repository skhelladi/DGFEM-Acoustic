CMAKE_MINIMUM_REQUIRED(VERSION 3.1)
PROJECT(DGalerkin VERSION 1.3.6)

# put all generated files in a single folder (required in windows for exe linked to dlls)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin CACHE PATH "")
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin CACHE PATH "")
MARK_AS_ADVANCED(LIBRARY_OUTPUT_PATH EXECUTABLE_OUTPUT_PATH)

# build type is "" by default in Linux
# IF(NOT CMAKE_BUILD_TYPE)
SET(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)

# ENDIF()

# enable C++11/C++17
SET(CMAKE_CXX_STANDARD 17) # newer way to set C++11/17 (requires cmake>3.1)
SET(CMAKE_CXX_STANDARD_REQUIRED ON)

IF(UNIX AND CMAKE_CXX_COMPILER_ID STREQUAL GNU)
    ADD_DEFINITIONS(-D_GLIBCXX_USE_CXX11_ABI=1)
ELSEIF(CMAKE_CXX_COMPILER_ID MATCHES MSVC)
    ADD_DEFINITIONS(-D_CRT_SECURE_NO_WARNINGS -D_SCL_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_WARNINGS)
    ADD_DEFINITIONS(-D_USE_MATH_DEFINES)
ENDIF()

# find gmsh-sdk
# gmsh.h
SET(GMSH_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/3rdParty/gmsh/include)
SET(GMSH_LIBRARIES ${PROJECT_SOURCE_DIR}/3rdParty/gmsh/lib/libgmsh.so)
SET(GMSH_EXECUTABLE ${PROJECT_SOURCE_DIR}/3rdParty/gmsh/bin/gmsh)

FIND_PATH(GMSH_INCLUDE_DIRS NAMES "gmsh.h")
MESSAGE(STATUS "GMSH_INCLUDE_DIRS=" ${GMSH_INCLUDE_DIRS})

if(NOT GMSH_INCLUDE_DIRS)
    MESSAGE(FATAL_ERROR "gmsh.h not found!")
ENDIF()

INCLUDE_DIRECTORIES(${GMSH_INCLUDE_DIRS})

# libgmsh.so
FIND_LIBRARY(GMSH_LIBRARIES gmsh)
MESSAGE(STATUS "GMSH_LIBRARIES=" ${GMSH_LIBRARIES})

IF(NOT GMSH_LIBRARIES)
    message(FATAL_ERROR "gmsh library not found!")
ENDIF()

# gmsh.exe
FIND_PROGRAM(GMSH_EXECUTABLE gmsh)
MESSAGE(STATUS "GMSH_EXECUTABLE=" ${GMSH_EXECUTABLE})

IF(NOT GMSH_EXECUTABLE)
    message(FATAL_ERROR "gmsh executable not found!")
ENDIF()

# Lapack
FIND_PACKAGE(BLAS)
FIND_PACKAGE(LAPACK)
MESSAGE(STATUS "LAPACK=" ${LAPACK_LIBRARIES})

if(LAPACK_FOUND AND BLAS_FOUND)
    set(LAPACKBLAS_LIBRARIES ${BLAS_LIBRARIES} ${LAPACK_LIBRARIES})
endif()

# Eigen
# SET(EIGEN_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/3rdParty/eigen)
# FIND_PATH(EIGEN_INCLUDE_DIRS NAMES "Eigen/Eigen")
# MESSAGE(STATUS "EIGEN_INCLUDE_DIRS=" ${EIGEN_INCLUDE_DIRS})

# if(NOT EIGEN_INCLUDE_DIRS)
#     MESSAGE(FATAL_ERROR "Eigen not found!")
# ENDIF()

# INCLUDE_DIRECTORIES(${EIGEN_INCLUDE_DIRS})
# FIND_PACKAGE(EIGEN REQUIRED)
SET(EIGEN_INCLUDE_DIRS "/usr/include/eigen3" CACHE PATH "Eigen directory override" FORCE)
FIND_PATH(EIGEN_INCLUDE_DIRS NAMES "Eigen/Eigen")
MESSAGE(STATUS "EIGEN_INCLUDE_DIRS=" ${EIGEN_INCLUDE_DIRS})
if(NOT EIGEN_INCLUDE_DIRS)
    MESSAGE(FATAL_ERROR "Eigen not found!")
ENDIF()
INCLUDE_DIRECTORIES(${EIGEN_INCLUDE_DIRS})

# SET(EIGEN_INCLUDE_DIRS "/usr/share/eigen3/cmake" CACHE PATH "Eigen directory override" FORCE)

# OpenMP
FIND_PACKAGE(OpenMP REQUIRED)

if(OPENMP_FOUND)
    message("OpenMP found")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${OpenMP_EXE_LINKER_FLAGS}")
endif()

# VTK
FIND_PACKAGE(VTK COMPONENTS IOXML REQUIRED)
SET(VTK_DIR "/usr/lib/x86_64-linux-gnu/cmake/vtk-9.1" CACHE PATH "VTK directory override" FORCE)

SET(WAVE_INCLUDE src/wave)
INCLUDE_DIRECTORIES(${WAVE_INCLUDE})

SET(WAVE_LD ${PROJECT_SOURCE_DIR}/build/bin)
LINK_DIRECTORIES(${WAVE_LD})

# Libraries
INCLUDE_DIRECTORIES(include)

# Sources
ADD_SUBDIRECTORY(src/wave)
ADD_SUBDIRECTORY(src)

