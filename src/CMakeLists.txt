# programs
SET(SRCS
	dgalerkin.cpp
	configParser.cpp
	Mesh.cpp
	utils.cpp
	solver.cpp
	eqEdit.cpp
	fft.cpp
	../include/configParser.h
	../include/Mesh.h
	../include/utils.h
	../include/solver.h
	../include/eqEdit.h
	../include/fft.h
)

ADD_EXECUTABLE(dgalerkin ${SRCS})
TARGET_LINK_LIBRARIES(dgalerkin ${GMSH_LIBRARIES} ${LAPACKBLAS_LIBRARIES} ${VTK_LIBRARIES} -fopenmp -ltbb -lpthread -lwave)

vtk_module_autoinit(
	TARGETS dgalerkin
	MODULES ${VTK_LIBRARIES}
)
