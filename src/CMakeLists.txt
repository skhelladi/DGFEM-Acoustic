# programs

SET(SRCS dgalerkin.cpp configParser.cpp ../include/configParser.h Mesh.cpp ../include/Mesh.h utils.cpp ../include/utils.h solver.cpp ../include/solver.h ../include/eqEdit.h eqEdit.cpp)
ADD_EXECUTABLE(dgalerkin ${SRCS})
TARGET_LINK_LIBRARIES(dgalerkin ${GMSH_LIBRARIES} ${LAPACKBLAS_LIBRARIES} ${VTK_LIBRARIES} -fopenmp -ltbb)

vtk_module_autoinit(
	TARGETS dgalerkin
	MODULES ${VTK_LIBRARIES}
)
