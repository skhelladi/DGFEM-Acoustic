#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <chrono>
#include <map>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include <algorithm>
#include <execution>
#include <numeric>

//! /////////
#include <boost/foreach.hpp>
#include <boost/ref.hpp>
#include <boost/typeof/typeof.hpp>
#include <boost/unordered_set.hpp>
//! /////////

#include "configParser.h"
#include "utils.h"

//! VTK headers
#include <vtkCellArray.h>
#include <vtkDoubleArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkCellData.h>
#include <vtkTetra.h>
#include <vtkTriangle.h>
#include <vtkUnstructuredGrid.h>
#include <vtkXMLUnstructuredGridWriter.h>

#ifndef DGALERKIN_MESH_H
#define DGALERKIN_MESH_H

class Mesh
{

public:
    Mesh(Config config);

    /**
     * List of getters used for vector access and to improve readability.
     * The inline optimization (equivalent of c macro) is used to
     * remove the associated function overhead.
     *
     * The following inline fcts follow the convention:
     * el = element indices in memory storage (!= element tag)
     * f = face indices in memory storage
     * n,i = element or face node (0,..., elNumNode)
     * g = integration point indices
     * x = physical coordinates (0=x, 1=y, 2=z)
     * u = parametric coordinates (0=u, 1=v, 2=w)
     *
     * NB: since c+11, inline is implicit inside class core
     */
    inline size_t &elTag(size_t el)
    {
        return m_elTags[el];
    };
    inline size_t &elNodeTag(size_t el, int n = 0)
    {
        return m_elNodeTags[el * m_elNumNodes + n];
    };
    inline double &elJacobian(size_t el, int g = 0, int x = 0, int u = 0)
    {
        return m_elJacobians[el * m_elNumIntPts * 9 + g * 9 + u * 3 + x];
    };
    inline double &elJacobianDet(size_t el, int g = 0)
    {
        return m_elJacobianDets[el * m_elNumIntPts + g];
    };
    inline double &elWeight(int g)
    {
        return m_elIntParamCoords[g * 4 + 3];
    };
    inline double &elBasisFct(int g, int i = 0)
    {
        return m_elBasisFcts[g * m_elNumNodes + i];
    };
    inline double &elUGradBasisFct(int g, int i = 0, int u = 0)
    {
        return m_elUGradBasisFcts[g * m_elNumNodes * 3 + i * 3 + u];
    };
    inline double &elGradBasisFct(size_t el, int g = 0, int i = 0, int x = 0)
    {
        return m_elGradBasisFcts[el * m_elNumIntPts * m_elNumNodes * 3 + g * m_elNumNodes * 3 + i * 3 + x];
    };
    inline size_t &elFNodeTag(size_t el, int f = 0, int i = 0)
    {
        return m_elFNodeTags[el * m_fNumPerEl * m_fNumNodes + f * m_fNumNodes + i];
    };
    inline size_t &fNodeTag(int f, int i = 0)
    {
        return m_fNodeTags[f * m_fNumNodes + i];
    };
    inline size_t &elFNodeTagOrdered(size_t el, int f = 0, int i = 0)
    {
        return m_elFNodeTagsOrdered[el * m_fNumPerEl * m_fNumNodes + f * m_fNumNodes + i];
    };
    inline size_t &fNodeTagOrdered(int f, int i = 0)
    {
        return m_fNodeTagsOrdered[f * m_fNumNodes + i];
    };
    inline double &fJacobian(int f, int g = 0, int x = 0, int u = 0)
    {
        return m_fJacobians[f * m_fNumIntPts * 9 + g * 9 + u * 3 + x];
    };
    inline double &fJacobianDet(int f, int g = 0)
    {
        return m_fJacobianDets[f * m_fNumIntPts + g];
    };
    inline double &fUGradBasisFct(int g, int i = 0, int u = 0)
    {
        return m_fUGradBasisFcts[g * m_fNumNodes * 3 + i * 3 + u];
    };
    inline double &fGradBasisFct(int f, int g = 0, int i = 0, int x = 0)
    {
        return m_fGradBasisFcts[f * m_fNumIntPts * m_fNumNodes * 3 + g * m_fNumNodes * 3 + i * 3 + x];
    };
    inline double &fIntPtCoord(int f, int g = 0, int x = 0)
    {
        return m_fIntPtCoords[f * m_fNumIntPts * 3 + g * 3 + x];
    };
    inline double &fIntParamCoord(int f, int g = 0, int x = 0)
    {
        return m_fIntParamCoords[f * m_fNumIntPts * 3 + g * 3 + x];
    };
    inline double &fWeight(int g)
    {
        return m_fIntParamCoords[g * 4 + 3];
    };
    inline double &fBasisFct(int g, int i = 0)
    {
        return m_fBasisFcts[g * m_fNumNodes + i];
    };
    inline double &fNormal(int f, int g = 0, int x = 0)
    {
        return m_fNormals[f * 3 * m_fNumIntPts + g * 3 + x];
    };
    inline double &fTangent(int f, int g = 0, int x = 0)
    {
        return m_fTangents[f * 3 * m_fNumIntPts + g * 3 + x];
    };
    inline double &fBiTangent(int f, int g = 0, int x = 0)
    {
        return m_fBiTangents[f * 3 * m_fNumIntPts + g * 3 + x];
    };
    inline size_t &elFId(size_t el, int f = 0)
    {
        return m_elFIds[el * m_fNumPerEl + f];
    }
    inline size_t &fNbrElId(int f, size_t el = 0)
    {
        return m_fNbrElIds[f][el];
    }
    inline size_t &fNToElNId(int f, int nf = 0, size_t el = 0)
    {
        return m_fNToElNIds[f][nf * m_fNbrElIds[f].size() + el];
    }
    inline int &elFOrientation(size_t el, int f)
    {
        return m_elFOrientation[el * m_fNumPerEl + f];
    }
    inline double &elMassMatrix(size_t el, int i = 0, int j = 0)
    {
        return m_elMassMatrices[el * m_elNumNodes * m_elNumNodes + i * m_elNumNodes + j];
    }
    inline double &fFlux(int f, int n = 0)
    {
        return m_fFlux[f * m_fNumNodes + n];
    }

    /**
     * Extra getters
     */
    int getNumNodes()
    {
        return m_elNodeTags.size();
    }
    int getElNumNodes()
    {
        return m_elNumNodes;
    }
    int getElNum()
    {
        return m_elNum;
    }
    std::vector<size_t> const &getElNodeTags()
    {
        return m_elNodeTags;
    }

    /**
     * Matrices and vectors assembly
     */
    void getElMassMatrix(size_t el, bool inverse, double *elMassMatrix);
    void precomputeMassMatrix();
    void precomputeFlux(std::vector<double> &u, std::vector<std::vector<double>> &Flux, int eq);
    void getElFlux(size_t el, double *F);
    void getUniqueFaceNodeTags();
    void getConnectivityFaceToElement();
    // void getUniqueFaceNodeTags_test();
    void getElStiffVector(size_t el, std::vector<std::vector<double>> &Flux,
                          std::vector<double> &u, double *elStiffVector);
    void updateFlux(std::vector<std::vector<double>> &u, std::vector<std::vector<std::vector<double>>> &Flux,
                    std::vector<double> &v0, double c0, double rho0);

    /**
     * @brief Write VTK
     * Added by Sofiane KHELLADI in 11/03/2022
     */
    // void writeVTU(std::string filename, std::vector<std::vector<double>> &u);
    void writeVTUb(std::string filename, std::vector<std::vector<double>> &u);
    void writePVD(std::string filename);

private:
    Config config;    // Configuration object

    int fc = 1;                               // Numerical flux coefficient
    int m_Dim = 3;                            // Physical space dimension
    int m_elDim;                              // Dimension of the element (and the domain)
    std::vector<int> m_elType;                // Element Types (integer)
    std::string m_elName;                     // Element Type name
    int m_elOrder;                            // Element Order
    int m_elNumNodes;                         // Number of nodes per element
    int m_elNumIntPts;                        // Number of integration points
    int m_elNum;                              // Number of elements in dim
    std::string m_elIntType;                  // Integration type name
    std::vector<double> m_elParamCoord;       // Parametric coordinates of the element
    std::vector<size_t> m_elTags;             // Tags of the elements
    std::vector<size_t> m_elNodeTags;         // Tags of the nodes associated to each element
                                              // [e1n1, e1n2, ..., e2n1, e2n2, ...]
    std::vector<double> m_elJacobians;        // Jacobian evaluated at each integration points : (dx/du)
                                              // [e1g1Jxx, e1g1Jxy, e1g1Jxz, ..., e1gGJzz, e2g1Jxx, ...]
    std::vector<double> m_elJacobianDets;     // Determinants of the jacobian evaluated at each integration points
                                              // [e1g1DetJ, e1g2DetJ, ... e2g1DetJ, e2g2DetJ, ...]
    std::vector<double> m_elIntPtCoords;      // x, y, z coordinates of the integration points element by element.
                                              // [e1g1x, e1g1y, e1g1z, ... , e1gGz, e2g1x, ...]
    std::vector<double> m_elIntParamCoords;   // u, v, w coordinates and the weight q for each integration point
                                              // [g1u, g1v, g1w, g1q, g2u, ...]
    std::vector<double> m_elBasisFcts;        // Evaluation of the basis functions at the integration points
                                              // [g1f1, g1f2, ..., g2f1, g2f2, ...]
    std::vector<double> m_elUGradBasisFcts;   // Evaluation of the derivatives of the basis functions at the integration points
                                              // [g1df1/du, g1df1/dv, ..., g2df1/du, g2df1/dv, ..., g1df2/du, g1df2/dv, ...]
    std::vector<double> m_elGradBasisFcts;    // Evaluation of the derivatives of the basis functions at the integration points
                                              // [e1g1df1/dx, e1g1df1/dy, ..., e1g2df1/dx, e1g2df1/dy, ..., e1g1df2/dx, e1g1df2/dy, ...]
    std::vector<size_t> m_elFIds;             // Faces ids for each element
                                              // [e1f1, e1f2, ..., e2f1, e2f2, ...]
    std::vector<size_t> m_elFNodeTags;        // Node tags for each face and each element
    std::vector<size_t> m_elFNodeTagsOrdered; // Ordered used for comparison, UnOrdered preserve GMSH ordering and locality
                                              // [e1f1n1, e1f1n2, ..., e1f2n1, e1f2n2, ..., e2f1n1, e2f1n2, ...]
    std::vector<int> m_elFOrientation;        // Contains 1 or -1, if the outward element face is in the same direction
                                              // as the face normal or -1 if not. [e1f1, e1f2, ..., e2f1, e2f2]
    std::vector<double> m_elWeight;

    int m_fDim;                             // Face dimension
    std::string m_fName;                    // Face type name
    int m_fType;                            // Face type
    int m_fNumNodes;                        // Number of nodes per face
    int m_fNumPerEl;                        // Number of faces per element
    int m_fEntity;                          // Entity containing all the faces
    int m_fNum;                             // Number of unique faces
    int m_fNumIntPts;                       // Number of integration points on each face
    std::string m_fIntType;                 // Face integration type (same as element)
    std::vector<size_t> m_fNodeTags;        // Node tags for each unique face
    std::vector<size_t> m_fNodeTagsOrdered; // [f1n1, f1n2, ..., f2n1, f2n2, ...]
    std::vector<size_t> m_fTags;            // Tag for each unique face
                                            // [f1, f2, f3, ...]
    std::vector<double> m_fJacobians;       // Jacobian evaluated at each integration points : (dx/du)
                                            // [f1g1Jxx, f1g1Jxy, f1g1Jxz, ... f1g1Jzz, f1g2Jxx, ..., f1gGJzz, f2g1Jxx, ...]
    std::vector<double> m_fJacobianDets;    // Determinants of the jacobian evaluated at each integration points
                                            // [f1g1DetJ, f1g2DetJ, ... f2g1DetJ, f2g2DetJ, ...]
    std::vector<double> m_fIntPtCoords;     // x, y, z coordinates of the integration points for each faces
                                            // [f1g1x, f1g1y, f1g1z, ... , f1gGz, f2g1x, ...]
    std::vector<double> m_fIntParamCoords;  // u, v, w coordinates and the weight q for each integration point
                                            // [g1u, g1v, g1w, g1q, g2u, ...]
    std::vector<double> m_fBasisFcts;       // Evaluation of the basis functions at the integration points
                                            // [g1f1, g1f2, ..., g2f1, g2f2, ...]
    std::vector<double> m_fUGradBasisFcts;  // Evaluation of the derivatives of the basis functions at the integration points
                                            // [g1df1/du, g1df1/dv, ..., g2df1/du, g2df1/dv, ..., g1df2/du, g1df2/dv, ...]
    std::vector<double> m_fGradBasisFcts;   // Evaluation of the derivatives of the basis functions at the integration points
                                            // [e1g1df1/dx, e1g1df1/dy, ..., e1g2df1/dx, e1g2df1/dy, ..., e1g1df2/dx, e1g1df2/dy, ...]
    std::vector<double> m_fNormals;         // Normal for each face at each int point
                                            // [f1g1Nx, f1g1Ny, f1g1Nz, f1g2Nx, ..., f2g1Nx, f2g1Ny, f2g1Nz, ...]
    std::vector<double> m_fTangents;         // Tangent for each face at each int point
                                            // [f1g1Tx, f1g1Ty, f1g1Tz, f1g2Tx, ..., f2g1Tx, f2g1Ty, f2g1Tz, ...]
    std::vector<double> m_fBiTangents;         // BiTangent for each face at each int point
                                            // [f1g1Sx, f1g1Sy, f1g1Sz, f1g2Sx, ..., f2g1Sx, f2g1Sy, f2g1Sz, ...]                                        

    std::vector<std::vector<size_t>> m_fNbrElIds;  // Id of element of each side of the face
    std::vector<std::vector<size_t>> m_fNToElNIds; // Map face node Ids to element node Ids

    std::vector<double> m_elMassMatrices; // Element mass matrix stored contiguously (row major)
                                          // [e1m11, e1m12, ..., e1m21, e1m22, ..., e2m11, ...]
    std::vector<double> m_fFlux;          // Flux through all faces
                                          // [f1n1, f1n2, ..., f2n1, f2n2, ...]

    std::vector<bool> m_fIsBoundary; // Is Face a boundary
    std::vector<size_t> m_fBC;       // Boundary type

    std::vector<double> m_fWeight;

    std::vector<std::vector<double>> RKR; // R*K*R^-1 matrix product, absorbing boundary

    std::vector<std::vector<double>> uGhost;                 // Ghost element nodal solution
    std::vector<std::vector<std::vector<double>>> FluxGhost; // Ghost flux
};

//! Tables operation functions

template <typename T>
bool isNCoincidentValues3d(std::vector<T> &vec1, std::vector<T> &vec2)
{
    return ((vec1[0] == vec2[0]) && (vec1[1] == vec2[1]) && (vec1[2] == vec2[2])) ||
           ((vec1[0] == vec2[1]) && (vec1[1] == vec2[0]) && (vec1[2] == vec2[2])) ||
           ((vec1[0] == vec2[2]) && (vec1[1] == vec2[1]) && (vec1[2] == vec2[0])) ||
           ((vec1[0] == vec2[0]) && (vec1[1] == vec2[2]) && (vec1[2] == vec2[1])) ||
           ((vec1[0] == vec2[2]) && (vec1[1] == vec2[0]) && (vec1[2] == vec2[1])) ||
           ((vec1[0] == vec2[1]) && (vec1[1] == vec2[2]) && (vec1[2] == vec2[0]));
}

template <typename T>
bool isNCoincidentValues2d(std::vector<T> &vec1, std::vector<T> &vec2)
{
    return ((vec1[0] == vec2[0]) && (vec1[1] == vec2[1])) ||
           ((vec1[0] == vec2[1]) && (vec1[1] == vec2[0]));
}

template <typename T>
bool isNCoincidentValues(std::vector<T> &vec1, std::vector<T> &vec2, size_t Num)
{
    // bool value(true);
    // #pragma omp parallel for reduction(*:value)
    //     for (size_t i=0; i<Num;i++)
    //         value *= (vec1[i] == vec2[i]);

    // value = std::equal(std::execution::seq, vec1.begin(), vec1.end(), vec2.begin());

    return std::equal(std::execution::seq, vec1.begin(), vec1.end(), vec2.begin());
}

template <typename T>
std::vector<std::vector<T>> vector_to_matrix(std::vector<T> &vec, size_t offset = 2)
{
    std::vector<std::vector<T>> mat;

    for (size_t i = 0; i < vec.size(); i += offset)
    {
        std::vector<size_t> row;
        for (size_t j = 0; j < offset; j++)
        {
            row.push_back(vec[i + j]);
        }
        mat.push_back(row);
    }
    return mat;
}

// template <typename T>
std::vector<size_t> vector_of_tags(size_t vec_size, size_t offset);
// {
//     std::vector<size_t> value;
//     for (size_t i = 0,id=0; i < vec_size; i++, id+=(i%offset))
//     {
//         std::cout<<id<<std::endl;
//         value.push_back(id);
//     }
//     return value;
// }

template <typename T>
std::vector<T> matrix_to_vector(std::vector<std::vector<T>> &mat, size_t &offset)
{
    std::vector<T> vec;
    offset = mat[0].size();
    for (size_t i = 0; i < mat.size(); i++)
    {
        for (size_t j = 0; j < mat[i].size(); j++)
        {
            vec.push_back(mat[i][j]);
        }
    }
    return vec;
}

template <typename T>
void erase_row_from_matrix(std::vector<std::vector<T>> &mat, size_t &row_id)
{
    mat.erase(std::next(std::begin(mat), row_id));
}

template <typename T>
void erase_row_from_vector(std::vector<T> &vec, size_t &id)
{
    vec.erase(std::next(std::begin(vec), id));
}

#endif // DGALERKIN_MESH_H
