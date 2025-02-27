#include <algorithm>
#include <assert.h>
#include <chrono>
#include <gmsh.h>
#include <iostream>
#include <omp.h>
#include <string>

#include "Mesh.h"
#include "configParser.h"
#include "utils.h"

/**
 * Mesh constructor: load the mesh data and parameters thanks to
 * Gmsh api. Create the elements mapping and set the boundary conditions.
 *
 * @name string File name
 * @config config Configuration object (content of the config parsed and load in memory)
 */
Mesh::Mesh(Config config) : config(config)
{

    /******************************
     *          Elements          *
     ******************************/

    // meshFileName = config.meshFileName;

    screen_display::write_string("Load data", GREEN);

    auto pp = [](const std::string &label, const std::vector<double> &v, int mult)
    {
        std::cout << " * " << v.size() / mult << " " << label << ": ";
        for (auto c : v)
            std::cout << c << " ";
        std::cout << "\n";
    };

    auto start = std::chrono::system_clock::now();
    m_elDim = gmsh::model::getDimension();
    gmsh::model::mesh::getElementTypes(m_elType, m_elDim);
    int _numPrimaryNodes = 0;

    gmsh::model::mesh::getElementProperties(m_elType[0], m_elName, m_elDim,
                                            m_elOrder, m_elNumNodes, m_elParamCoord, _numPrimaryNodes);

    gmsh::model::mesh::getElementsByType(m_elType[0], m_elTags, m_elNodeTags);
    m_elNum = (int)m_elTags.size();
    m_elIntType = "Gauss" + std::to_string(2 * m_elOrder);

    // std::vector<double> m_elWeight;
    gmsh::model::mesh::getIntegrationPoints(m_elType[0], m_elIntType, m_elParamCoord, m_elWeight);
    // pp("integration points to integrate order " + std::to_string(m_elOrder*2) + " polynomials", m_elParamCoord, 3);

    screen_display::write_string("Elements - Compute Jacobian", GREEN);

    // std::vector<double> _localCoord;

    // screen_display::write_string("flag 0", RED);

    int _numOrientations;
    // const std::vector<int>& wantedOrientations = std::vector<int>()
    int _numComponents;
    gmsh::model::mesh::getBasisFunctions(m_elType[0], m_elParamCoord, config.elementType,
                                         _numComponents, m_elBasisFcts, _numOrientations);
    gmsh::model::mesh::getBasisFunctions(m_elType[0], m_elParamCoord, "Grad" + config.elementType,
                                         _numComponents, m_elUGradBasisFcts, _numOrientations);

    // screen_display::write_string(m_elIntType, RED);
    // screen_display::write_string("Grad" + config.elementType, RED);
    // gmsh::model::mesh::getBasisFunctions(m_elType[0], m_elIntType, "Grad" + config.elementType,
    //                                      m_elIntParamCoords, *new int, m_elUGradBasisFcts);

    gmsh::model::mesh::getJacobians(m_elType[0], m_elParamCoord, m_elJacobians,
                                    m_elJacobianDets, m_elIntPtCoords);

    // std::ofstream _outfile_("m_elJacobians.txt");
    // _outfile_ << "size=" << m_elJacobians.size() << std::endl;
    // for (size_t i = 0; i < m_elJacobians.size(); i++)
    //     _outfile_ << m_elJacobians[i] << std::endl;
    // _outfile_.close();

    // pp("Jacobian determinants at integration points", m_elJacobianDets, 1);

    m_elNumIntPts = (int)m_elJacobianDets.size() / m_elNum;

    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);
    /**
     * Gmsh provides the derivative of the shape functions along
     * the parametric directions. We therefore compute their derivative
     * along the physical directions thanks to composed derivative.
     * The system can be expressed as J^T * df/dx = df/du
     *
     * |dx/du dx/dv dx/dw|^T  |df/dx|   |df/du|
     * |dy/du dy/dv dy/dw|  * |df/dy| = |df/dv|
     * |dz/du dz/dv dz/dw|    |df/dz|   |df/dw|
     *
     * (x,y,z) are the physical coordinates
     * (u,v,w) are the parametric coordinates
     *
     * NB: Instead of transposing, we take advantages of the fact
     * Lapack/Blas use column major while Gmsh provides row major.
     */

    start = std::chrono::system_clock::now();
    std::vector<double> jacobian(m_elDim * m_elDim);
    m_elGradBasisFcts.resize(m_elNum * m_elNumNodes * m_elNumIntPts * 3);

    // #pragma omp parallel for
    for (size_t el = 0; el < m_elNum; ++el)
    {
        for (int g = 0; g < m_elNumIntPts; ++g)
        {
            for (int f = 0; f < m_elNumNodes; ++f)
            {
                // The copy operations are not required. They're simply enforced
                // to ensure that the inputs (jacobian, grad) remains unchanged.
                for (int i = 0; i < m_elDim; ++i)
                {
                    for (int j = 0; j < m_elDim; ++j)
                    {
                        // screen_display::write_value("elJacobian(el, g, i, j)",elJacobian(el, g, i, j),"");
                        jacobian[i * m_elDim + j] = elJacobian(el, g, i, j);
                    }
                }

                // std::cout<<"el = "<<el<<" - g = "<<g<<" - f = "<<f<<std::endl;
                // screen_display::write_value("elUGradBasisFct(g, f)",elUGradBasisFct(g, f),"",BLUE);
                // screen_display::write_value("elGradBasisFct(el, g, f)",elGradBasisFct(el, g, f),"",BLUE);
                std::copy(&elUGradBasisFct(g, f), &elUGradBasisFct(g, f) + m_elDim, &elGradBasisFct(el, g, f));
                eigen::solve(jacobian.data(), &elGradBasisFct(el, g, f), m_elDim);
                // screen_display::write_string("flag 1", RED);
            }
        }
    }

    // pp("Element jacobian", jacobian, 1);

    assert(m_elType.size() == 1);
    assert(m_elNodeTags.size() == m_elNum * m_elNumNodes);
    assert(m_elJacobianDets.size() == m_elNum * m_elNumIntPts);
    assert(m_elBasisFcts.size() == m_elNumNodes * m_elNumIntPts);
    assert(m_elGradBasisFcts.size() == m_elNum * m_elNumIntPts * m_elNumNodes * 3);

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    gmsh::logger::write("==================================================");
    gmsh::logger::write("Number of Elements : " + std::to_string(m_elNum));
    gmsh::logger::write("Element dimension : " + std::to_string(m_elDim));
    gmsh::logger::write("Element Type : " + m_elName);
    gmsh::logger::write("Element Order : " + std::to_string(m_elOrder));
    gmsh::logger::write("Element Nbr Nodes : " + std::to_string(m_elNumNodes));
    gmsh::logger::write("Integration type : " + m_elIntType);
    gmsh::logger::write("Integration Nbr points : " + std::to_string(m_elNumIntPts));

    /******************************
     *            Faces           *
     ******************************/
    screen_display::write_string("Faces treatment", GREEN);
    start = std::chrono::system_clock::now();
    m_fDim = m_elDim - 1;
    m_fName = m_fDim == 0 ? "point" : m_fDim == 1 ? "line"
                                  : m_fDim == 2   ? "triangle"
                                                  : "None"; // Quads not yet supported.
    m_fNumNodes = m_fDim == 0 ? 1 : m_fDim == 1 ? 1 + m_elOrder
                                : m_fDim == 2   ? (m_elOrder + 1) * (m_elOrder + 2) / 2
                                                : 0; // Triangular elements only.

    m_fType = gmsh::model::mesh::getElementType(m_fName, m_elOrder);

    /**
     * [1] Get Faces for all elements
     */
    if (m_fDim < 2)
        gmsh::model::mesh::getElementEdgeNodes(m_elType[0], m_elFNodeTags, -1);
    else
        gmsh::model::mesh::getElementFaceNodes(m_elType[0], 3, m_elFNodeTags, -1);

    m_fNumPerEl = m_elFNodeTags.size() / (m_elNum * m_fNumNodes);
    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);
    /**
     * [2] Remove the faces counted two times
     *     i.e. common face between two elements.
     */
    screen_display::write_string("Remove the faces counted two times", GREEN);
    start = std::chrono::system_clock::now();

    //! ////////////////////////////////
    getUniqueFaceNodeTags();
    //! ////////////////////////////////

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    /**
     * [3] Finally, we create a single entity containing all the
     *     unique faces. We call Gmsh with empty face tags and
     *     retrieve directly after the auto-generated tags.
     */
    screen_display::write_string("Create a single entity");
    start = std::chrono::system_clock::now();
    m_fEntity = gmsh::model::addDiscreteEntity(m_fDim);

    gmsh::model::mesh::addElementsByType(m_fEntity, m_fType, {}, m_fNodeTags);

    m_fIntType = m_elIntType;

    gmsh::model::mesh::getIntegrationPoints(m_fType, m_fIntType, m_fIntParamCoords, m_fWeight);

    m_fNum = m_fNodeTags.size() / m_fNumNodes;
    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    /**
     * A priori the same integration type and order is applied
     * to the surface and to the volume integrals.
     */
    screen_display::write_string("Faces - Compute Jacobian");
    start = std::chrono::system_clock::now();
    m_fIntType = m_elIntType;

    gmsh::model::mesh::getBasisFunctions(m_fType, m_fIntParamCoords, config.elementType, *new int, m_fBasisFcts, _numOrientations);

    gmsh::model::mesh::getBasisFunctions(m_fType, m_fIntParamCoords, "Grad" + config.elementType, *new int, m_fUGradBasisFcts, _numOrientations);

    gmsh::model::mesh::getJacobians(m_fType, m_fIntParamCoords, m_fJacobians, m_fJacobianDets, m_fIntPtCoords, m_fEntity);

    m_fNumIntPts = (int)m_fJacobianDets.size() / m_fNum;

    /**
     * See element part for explanation. (line 40)
     */
    m_fGradBasisFcts.resize(m_fNum * m_fNumNodes * m_fNumIntPts * 3);
    // #pragma omp parallel for
    for (int f = 0; f < m_fNum; ++f)
    {
        for (int g = 0; g < m_fNumIntPts; ++g)
        {
            for (int n = 0; n < m_fNumNodes; ++n)
            {
                for (int i = 0; i < m_elDim; ++i)
                {
                    for (int j = 0; j < m_elDim; ++j)
                    {
                        jacobian[i * m_elDim + j] = fJacobian(f, g, i, j);
                    }
                }
                std::copy(/*std::execution::par,*/ &fUGradBasisFct(g, n), &fUGradBasisFct(g, n) + m_elDim, &fGradBasisFct(f, g, n));
                eigen::solve(jacobian.data(), &fGradBasisFct(f, g, n), m_elDim);
            }
        }
    }

    // pp("m_fJacobians", m_fJacobians, 3);

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);
    /**
     * Define a normal associated to each surface.
     */

    screen_display::write_string("Define a normal/tangent/bitangent (1D and 2D only, 3D in progress) associated to each surface.", GREEN);
    start = std::chrono::system_clock::now();
    std::vector<double> normal(m_Dim);
    std::vector<double> tangent(m_Dim);
    std::vector<double> bitangent(m_Dim);
    // #pragma omp parallel for
    for (int f = 0; f < m_fNum; ++f)
    {
        // compute here tangent and bitangent for 3D cases

        std::vector<double> T = {0, 0, 0};
        std::vector<double> B = {0, 0, 0};

        if (m_fDim == 2)
        {
            //! only primary nodes are used
            std::vector<double> p1 = {fIntPtCoord(f, 0, 0), fIntPtCoord(f, 0, 1), fIntPtCoord(f, 0, 2)};
            std::vector<double> p2 = {fIntPtCoord(f, 1, 0), fIntPtCoord(f, 1, 1), fIntPtCoord(f, 1, 2)};
            std::vector<double> p3 = {fIntPtCoord(f, 2, 0), fIntPtCoord(f, 2, 1), fIntPtCoord(f, 2, 2)};

            double h = sqrt(pow(p2[0] - p1[0], 2) + pow(p2[1] - p1[1], 2) + pow(p2[2] - p1[2], 2));
            double i = ((p2[0] - p1[0]) * (p3[0] - p1[0]) + (p2[1] - p1[1]) * (p3[1] - p1[1]) + (p2[2] - p1[2]) * (p3[2] - p1[2])) / h;
            double j = sqrt(pow(p3[0] - p1[0], 2) + pow(p3[1] - p1[1], 2) + pow(p3[2] - p1[2], 2) - pow(i, 2));

            double u1(0), v1(0);
            double u2(h), v2(0);
            double u3(i), v3(j);

            double delta = fabs(u2 - u1) * fabs(v3 - v1) - fabs(v2 - v1) * fabs(u3 - u1);

            T[0] = (fabs(v3 - v1) * (p2[0] - p1[0]) - fabs(v2 - v1) * (p3[0] - p1[0])) / delta;
            T[1] = (fabs(v3 - v1) * (p2[1] - p1[1]) - fabs(v2 - v1) * (p3[1] - p1[1])) / delta;
            T[2] = (fabs(v3 - v1) * (p2[2] - p1[2]) - fabs(v2 - v1) * (p3[2] - p1[2])) / delta;

            B[0] = -(fabs(u3 - u1) * (p2[0] - p1[0]) - fabs(u2 - u1) * (p3[0] - p1[0])) / delta;
            B[1] = -(fabs(u3 - u1) * (p2[1] - p1[1]) - fabs(u2 - u1) * (p3[1] - p1[1])) / delta;
            B[2] = -(fabs(u3 - u1) * (p2[2] - p1[2]) - fabs(u2 - u1) * (p3[2] - p1[2])) / delta;
        }

        // screen_display::write_value("delta", delta);
        // screen_display::write_value("|T|", sqrt(pow(T[0], 2) + pow(T[1], 2) + pow(T[2], 2)));
        // screen_display::write_value("|B|", sqrt(pow(B[0], 2) + pow(B[1], 2) + pow(B[2], 2)));
        // getchar();

        //! ////////////////////////////////////////////////////
        for (int g = 0; g < m_fNumIntPts; ++g)
        {

            switch (m_fDim)
            {
            case 0:
            {
                normal = {1, 0, 0};
                tangent = {0, 0, 0};
                bitangent = {0, 0, 0};
                break;
            }
            case 1:
            {
                std::vector<double> normalPlane = {0, 0, -1};
                eigen::cross(&fGradBasisFct(f, g, 0), normalPlane.data(), normal.data());
                if (eigen::dot(&fGradBasisFct(f, g), &fGradBasisFct(f, 0), m_Dim) < 0)
                {
                    for (int x = 0; x < m_Dim; ++x)
                        // #pragma omp atomic
                        normal[x] *= -1.0;
                }
                tangent = {-normal[1], normal[0], 0};
                bitangent = {0, 0, 0};
                break;
            }
            case 2:
            {
                eigen::cross(&fGradBasisFct(f, g, 0), &fGradBasisFct(f, g, 1), normal.data());
                if (g != 0 && eigen::dot(&fNormal(f, 0), normal.data(), m_Dim) < 0)
                {
                    for (int x = 0; x < m_Dim; ++x)
                        // #pragma omp atomic
                        normal[x] *= -1.0;
                    // normal[x] = -normal[x];
                }
                tangent = {T[0], T[1], T[2]};
                bitangent = {B[0], B[1], B[2]};
                break;
            }
            }
            eigen::normalize(normal.data(), m_Dim);
            eigen::normalize(tangent.data(), m_Dim);
            eigen::normalize(bitangent.data(), m_Dim);
            m_fNormals.insert(m_fNormals.end(), normal.begin(), normal.end());
            m_fTangents.insert(m_fTangents.end(), tangent.begin(), tangent.end());
            m_fBiTangents.insert(m_fBiTangents.end(), bitangent.begin(), bitangent.end());
        }
    }

    if (m_elDim == 3 && m_elOrder != 1)
        fc = -1;

    screen_display::write_if_false(m_elFNodeTags.size() == m_elNum * m_fNumPerEl * m_fNumNodes, "m_elFNodeTags size error");
    screen_display::write_if_false(m_fJacobianDets.size() == m_fNum * m_fNumIntPts, "m_fJacobianDets size error");
    screen_display::write_if_false(m_fBasisFcts.size() == m_fNumNodes * m_fNumIntPts, "m_fBasisFcts size error");
    screen_display::write_if_false(m_fNormals.size() == m_Dim * m_fNum * m_fNumIntPts, "m_fNormals size error");
    screen_display::write_if_false(m_fTangents.size() == m_Dim * m_fNum * m_fNumIntPts, "m_fTangents size error");
    screen_display::write_if_false(m_fBiTangents.size() == m_Dim * m_fNum * m_fNumIntPts, "m_fBiTangents size error");

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    gmsh::logger::write("==================================================");
    gmsh::logger::write("Number of Faces : " + std::to_string(m_fNum));
    gmsh::logger::write("Faces per Element : " + std::to_string(m_fNumPerEl));
    gmsh::logger::write("Face dimension : " + std::to_string(m_fDim));
    gmsh::logger::write("Face Type : " + m_fName);
    gmsh::logger::write("Face Nbr Nodes : " + std::to_string(m_fNumNodes));
    gmsh::logger::write("Integration type : " + m_fIntType);
    gmsh::logger::write("Integration Nbr points : " + std::to_string(m_fNumIntPts));

    /******************************
     *       Connectivity         *
     ******************************/

    /**
     * Assign corresponding faces to each element, we use the
     * fact that the node tags per face has already been ordered
     */
    screen_display::write_string("Connectivity: Assign corresponding faces to each element", GREEN);
    start = std::chrono::system_clock::now();

    //! ////////////////////////////////
    getConnectivityFaceToElement();
    //! ////////////////////////////////

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    /**
     * For efficiency purposes we also directly store the mapping
     * between face node id and element node id. For example, the
     * 3rd node of the face correspond to the 7th of the element.
     */
    screen_display::write_string("Store the mapping between face node id and element node id", GREEN);
    start = std::chrono::system_clock::now();

    m_fNToElNIds.resize(m_fNum);
    // #pragma omp parallel for
    for (int f = 0; f < m_fNum; ++f)
    {
        for (int nf = 0; nf < m_fNumNodes; ++nf)
        {
            for (size_t el : m_fNbrElIds[f])
            {
                for (int nel = 0; nel < m_elNumNodes; ++nel)
                {
                    if (fNodeTag(f, nf) == elNodeTag(el, nel))
                        m_fNToElNIds[f].push_back(nel);
                }
            }
        }
    }
    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);
    /**
     * Up to now, the normals are associated to the faces.
     * We still need to know how the normal is oriented
     * with respect to its neighbouring elements.
     *
     * For instance, element1 normal has the same orientation, we therefore assign
     * the orientation +1 and reciprocally we set the orientation to -1 for e2.
     *
     *  ____     f1       ____
     * |    |     |      |    |
     * | e1 |->   |->  <-| e2 |
     * |____|     |      |____|
     *
     */
    screen_display::write_string("Define normals orientation", GREEN);
    start = std::chrono::system_clock::now();

    double dotProduct;
    std::vector<double> m_elBarycenters, fNodeCoord(3), elOuterDir(3), paramCoords;
    gmsh::model::mesh::getBarycenters(m_elType[0], -1, false, true, m_elBarycenters);

    m_elFOrientation.clear();

    for (size_t el = 0; el < m_elNum; ++el)
    {
        for (int f = 0; f < m_fNumPerEl; ++f)
        {
            dotProduct = 0.0;

            int _dim, _tag;

            gmsh::model::mesh::getNode(elFNodeTag(el, f), fNodeCoord, paramCoords, _dim, _tag);

            for (int x = 0; x < m_Dim; x++)
            {
                elOuterDir[x] = fNodeCoord[x] - m_elBarycenters[el * 3 + x];
                dotProduct += elOuterDir[x] * fNormal(elFId(el, f), 0, x);
            }

            size_t value = (dotProduct >= 0) ? 1 : -1;
            m_elFOrientation.push_back(value);
        }
    }

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    /**
     * Once the orientation known, we reclassify the neighbouring
     * elements by imposing the first one to be oriented in the
     * same direction than the corresponding face.
     */
    screen_display::write_string("Reclassification of the neighbouring elements", GREEN);
    start = std::chrono::system_clock::now();

    size_t elf;
    // #pragma omp parallel for
    for (int f = 0; f < m_fNum; ++f)
    {
        for (int lf = 0; lf < m_fNumPerEl; ++lf)
        {
            if (elFId(fNbrElId(f, 0), lf) == f)
                elf = lf;
        }
        if (m_fNbrElIds.size() == 2)
        {
            if (elFOrientation(fNbrElId(f, 0), elf) <= 0)
            {
                std::swap(m_fNbrElIds[f][0], m_fNbrElIds[f][1]);
                for (int nf = 0; nf < m_fNumNodes; ++nf)
                    std::swap(fNToElNId(f, nf, 0), fNToElNId(f, nf, 1));
            }
        }
    }

    assert(m_elFIds.size() == m_elNum * m_fNumPerEl);
    assert(m_elFOrientation.size() == m_elNum * m_fNumPerEl);

    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    gmsh::logger::write("==================================================");
    gmsh::logger::write("Element-Face connectivity retrieved.");
    start = std::chrono::system_clock::now();
    //---------------------------------------------------------------------
    // Boundary conditions
    //---------------------------------------------------------------------

    /******************************
     *     Boundary conditions    *
     ******************************/

    /**
     * Check if a face is a boundary or not and orientate
     * the normal at boundaries in the outward direction.
     * This convention is particularly useful for BCs.
     */
    screen_display::write_string("Boundary conditions", GREEN);
    // #pragma omp parallel for
    for (int f = 0; f < m_fNum; ++f)
    {
        if (m_fNbrElIds[f].size() < 2)
        {
            m_fIsBoundary.push_back(true);
            for (int lf = 0; lf < m_fNumPerEl; ++lf)
            {
                if (elFId(fNbrElId(f, 0), lf) == f)
                {
                    for (int g = 0; g < m_fNumIntPts; ++g)
                    {
                        // #pragma omp atomic
                        fNormal(f, g, 0) *= elFOrientation(fNbrElId(f, 0), lf);
                        // fTangent(f, g, 0) *= elFOrientation(fNbrElId(f, 0), lf);
                        // fBiTangent(f, g, 0) *= elFOrientation(fNbrElId(f, 0), lf);
                        // #pragma omp atomic
                        fNormal(f, g, 1) *= elFOrientation(fNbrElId(f, 0), lf);
                        // fTangent(f, g, 1) *= elFOrientation(fNbrElId(f, 0), lf);
                        // fBiTangent(f, g, 1) *= elFOrientation(fNbrElId(f, 0), lf);
                        // #pragma omp atomic
                        fNormal(f, g, 2) *= elFOrientation(fNbrElId(f, 0), lf);
                        // fTangent(f, g, 2) *= elFOrientation(fNbrElId(f, 0), lf);
                        // fBiTangent(f, g, 2) *= elFOrientation(fNbrElId(f, 0), lf);
                    }
                    elFOrientation(fNbrElId(f, 0), lf) = 1;
                }
            }
        }
        else
        {
            m_fIsBoundary.push_back(false);
        }
    }

    /**
     * Iterate over the physical boundaries and over each nodes
     * belonging to that boundary. Retrieve the associated face and assign
     * it an unique integer representing the BC type.
     *
     * 1        : Reflecting
     * 2        : Absorbing
     * Default  : Absorbing (!= 1 or 2)
     */
    m_fBC.resize(m_fNum);
    std::vector<size_t> nodeTags;
    std::vector<double> coord;
    for (auto const &physBC : config.physBCs)
    {
        auto physTag = physBC.first;
        auto BCtype = physBC.second.first;
        auto BCvalue = physBC.second.second;

        gmsh::model::mesh::getNodesForPhysicalGroup(m_fDim, physTag, nodeTags, coord);
        if (BCtype == "Reflecting")
        {
            for (int f = 0; f < m_fNum; ++f)
            {
                if (m_fIsBoundary[f] && std::find(nodeTags.begin(), nodeTags.end(), fNodeTag(f)) != nodeTags.end())
                    m_fBC[f] = 1;
            }
        }
        else
        {
            for (int f = 0; f < m_fNum; ++f)
            {
                if (m_fIsBoundary[f] && std::find(nodeTags.begin(), nodeTags.end(), fNodeTag(f)) != nodeTags.end())
                    m_fBC[f] = 0;
            }
        }
    }
    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);

    /**
     * Compute the R*K*R matrix product. This matrix product is related to the absorbing
     * boundary conditions in the specific context of acoustic waves. It is mainly used
     * to suppress the outgoing solution along characteristics lines.
     */

    screen_display::write_string("Compute the R*K*R matrix product", GREEN);
    start = std::chrono::system_clock::now();

    RKR.resize(m_fNum * m_fNumIntPts);

    // #pragma omp parallel for
    for (int f = 0; f < m_fNum; ++f)
    {
        if (m_fBC[f] == 0)
        {
            for (int g = 0; g < m_fNumIntPts; ++g)
            {
                int i = f * m_fNumIntPts + g;
                double nx(fNormal(f, g, 0)), ny(fNormal(f, g, 1)), nz(fNormal(f, g, 2));
                double tx(fTangent(f, g, 0)), ty(fTangent(f, g, 1)), tz(fTangent(f, g, 2));
                double sx(fBiTangent(f, g, 0)), sy(fBiTangent(f, g, 1)), sz(fBiTangent(f, g, 2));
                double c0(config.c0), rho0(config.rho0);
                double vx0(config.v0[0]), vy0(config.v0[1]), vz0(config.v0[2]);
                double vn0 = vx0 * nx + vy0 * ny + vz0 * nz;
                double lambda = (vn0 < 0) ? 0 : -1; // FIXME: Warning: lambda=-1 and not 1, this work but there is probably a bug in the code
                                                    // (pre-processing)...

                // double L1(fabs(vn0-c0)), L2(fabs(vn0+c0)), L3(fabs(vn0-c0));

                // screen_display::write_value("lambda",lambda);
                // getchar();

                RKR[i].resize(16);

                RKR[i][0] = 0.25 * (c0 + vn0);
                RKR[i][1] = 0.25 * (c0 * rho0 * (c0 + vn0) * nx);
                RKR[i][2] = 0.25 * (c0 * rho0 * (c0 + vn0) * ny);
                RKR[i][3] = 0.25 * (c0 * rho0 * (c0 + vn0) * nz);

                RKR[i][4] = 0.25 * (nx * (c0 + vn0) / (rho0 * c0));
                RKR[i][5] = 0.25 * ((c0 + vn0) * nx * nx - vn0 * lambda * (tx * tx + sx * sx));
                RKR[i][6] = 0.25 * ((c0 + vn0) * nx * ny - vn0 * lambda * (ty * tx + sy * sx));
                RKR[i][7] = 0.25 * ((c0 + vn0) * nx * nz - vn0 * lambda * (tz * tx + sz * sx));

                RKR[i][8] = 0.25 * (ny * (c0 + vn0) / (rho0 * c0));
                RKR[i][9] = 0.25 * ((c0 + vn0) * ny * nx - vn0 * lambda * (tx * ty + sx * sy));
                RKR[i][10] = 0.25 * ((c0 + vn0) * ny * ny - vn0 * lambda * (ty * ty + sy * sy));
                RKR[i][11] = 0.25 * ((c0 + vn0) * ny * nz - vn0 * lambda * (tz * ty + sz * sy));

                RKR[i][12] = 0.25 * (nz * (c0 + vn0) / (rho0 * c0));
                RKR[i][13] = 0.25 * ((c0 + vn0) * nz * nx - vn0 * lambda * (tx * tz + sx * sz));
                RKR[i][14] = 0.25 * ((c0 + vn0) * nz * ny - vn0 * lambda * (ty * tz + sy * sz));
                RKR[i][15] = 0.25 * ((c0 + vn0) * nz * nz - vn0 * lambda * (tz * tz + sz * sz));
            }
        }
    }

    assert(m_fIsBoundary.size() == m_fNum);

    /**
     * Extra Memory allocation:
     * Instantiate Ghost Elements and numerical flux storage.
     */
    m_fFlux.resize(m_fNum * m_fNumNodes);
    uGhost = std::vector<std::vector<double>>(4,
                                              std::vector<double>(m_fNum * m_fNumIntPts));
    FluxGhost = std::vector<std::vector<std::vector<double>>>(4,
                                                              std::vector<std::vector<double>>(m_fNum * m_fNumIntPts,
                                                                                               std::vector<double>(3)));

    gmsh::logger::write("Boundary conditions successfuly loaded.");
    gmsh::logger::write("==================================================");
    end = std::chrono::system_clock::now();
    elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time:", elapsed.count() * 1.0e-6, "s", BLUE);
}

/**
 * Precompute and store the mass matris for all elements in m_elMassMatrix
 */
void Mesh::precomputeMassMatrix()
{
    m_elMassMatrices.resize(m_elNum * m_elNumNodes * m_elNumNodes);
    // #pragma omp parallel for
    for (size_t el = 0; el < m_elNum; ++el)
    {
        getElMassMatrix(el, true, &elMassMatrix(el));
    }
}

/**
 * Compute the element mass matrix.
 *
 * @param el integer : element id (!= gmsh tag, it is the location in memory storage)
 * @param inverse boolean : Whether or not the mass matrix must be inverted before returned
 * @param elMassMatrix double array : Output storage of the element mass matrix
 */
void Mesh::getElMassMatrix(const size_t el, const bool inverse, double *elMassMatrix)
{
    for (int i = 0; i < m_elNumNodes; ++i)
    {
        for (int j = 0; j < m_elNumNodes; ++j)
        {
            elMassMatrix[i * m_elNumNodes + j] = 0.0;
            for (int g = 0; g < m_elNumIntPts; g++)
            {
                elMassMatrix[i * m_elNumNodes + j] += elBasisFct(g, i) * elBasisFct(g, j) *
                                                      m_elWeight[g] * elJacobianDet(el, g);
            }
        }
    }
    if (inverse)
        eigen::inverse(elMassMatrix, m_elNumNodes);
}

/**
 * Compute the element stiffness/convection matrix.
 *
 * @param el integer : element id
 * @param Flux double array : physical flux
 * @param u double array : solution at element node
 * @param elStiffVector double array : Output storage of the element stiffness vector
 */
void Mesh::getElStiffVector(const size_t el, std::vector<std::vector<double>> &Flux,
                            std::vector<double> &u, double *elStiffVector)
{
    int jId;
    for (int i = 0; i < m_elNumNodes; ++i)
    {
        elStiffVector[i] = 0.0;
        for (int j = 0; j < m_elNumNodes; ++j)
        {
            jId = el * m_elNumNodes + j;
            for (int g = 0; g < m_elNumIntPts; g++)
            {
                elStiffVector[i] += eigen::dot(Flux[jId].data(), &elGradBasisFct(el, g, i), m_Dim) *
                                    elBasisFct(g, j) * m_elWeight[g] * elJacobianDet(el, g);
            }
        }
    }
}

/**
 * Precompute the numerical flux through all the faces. The flux implemented is
 * the Rusanov Flux. Also note that the following code is paralelized using openMP.
 *
 * @param Flux double array : physical flux
 * @param u double array : solution at the node
 * @param eq : equation id (0 = pressure, 1 = velocity x, 2= vy, 3= vz)
 */
void Mesh::precomputeFlux(std::vector<double> &u, std::vector<std::vector<double>> &Flux, int eq)
{

#pragma omp parallel num_threads(config.numThreads)
    {
        // Memory allocation (Cross-plateform compatibility)
        size_t elUp, elDn;
        std::vector<double> FIntPts(m_fNumIntPts, 0);
        std::vector<double> Fnum(m_Dim, 0);

#pragma omp parallel for schedule(static)
        for (int f = 0; f < m_fNum; ++f)
        {

            std::fill(FIntPts.begin(), FIntPts.end(), 0);

            // Numerical Flux at Integration points
            if (m_fIsBoundary[f])
            {
                for (int g = 0; g < m_fNumIntPts; ++g)
                    FIntPts[g] = FluxGhost[eq][f * m_fNumIntPts + g][0];
            }
            else
            {
                for (int i = 0; i < m_fNumNodes; ++i)
                {
                    elUp = fNbrElId(f, 0) * m_elNumNodes + fNToElNId(f, i, 0);
                    elDn = fNbrElId(f, 1) * m_elNumNodes + fNToElNId(f, i, 1);
                    for (int g = 0; g < m_fNumIntPts; ++g)
                    {
                        for (int x = 0; x < m_Dim; ++x)
                            Fnum[x] = 0.5 * ((Flux[elUp][x] + Flux[elDn][x]) + fc * config.c0 * fNormal(f, g, x) * (u[elUp] - u[elDn]));
/////////////////////////
#pragma omp atomic update
                        FIntPts[g] += eigen::dot(&fNormal(f, g), Fnum.data(), m_Dim) * fBasisFct(g, i);
                    }
                }
            }

            // Surface integral
            for (int n = 0; n < m_fNumNodes; ++n)
            {
                fFlux(f, n) = 0;
                for (int g = 0; g < m_fNumIntPts; ++g)
                {
////////////////////////
#pragma omp atomic update
                    fFlux(f, n) += m_fWeight[g] * fBasisFct(g, n) * FIntPts[g] * fJacobianDet(f, g);
                }
            }
        }
    }
}

/**
 * Compute flux through a given element from
 * the value of the flux at the face.
 *
 * @param el integer : element id
 * @param F double array : Output element flux
 */
void Mesh::getElFlux(const size_t el, double *F)
{
    int i;
    std::fill(F, F + m_elNumNodes, 0);
    // #pragma omp parallel num_threads(config.numThreads)
    {
        // #pragma omp parallel for schedule(static)
        for (int f = 0; f < m_fNumPerEl; ++f)
        {
            el == fNbrElId(elFId(el, f), 0) ? i = 0 : i = 1;
            for (int nf = 0; nf < m_fNumNodes; ++nf)
            {
                // #pragma omp atomic
                F[fNToElNId(elFId(el, f), nf, i)] += elFOrientation(el, f) * fFlux(elFId(el, f), nf);
            }
        }
    }
}

/**
 * Compute physical flux from the nodal solution. Also update
 * the ghost element and numerical flux.
 *
 * @param u : nodal solution vector
 * @param Flux : Physical flux
 * @param v0 : mean flow speed (v0x,v0y,v0z)
 * @param c0 : speed of sound
 * @param rho0: mean flow density
 */
void Mesh::updateFlux(std::vector<std::vector<double>> &u, std::vector<std::vector<std::vector<double>>> &Flux,
                      std::vector<double> &v0, double c0, double rho0)
{

// #pragma omp parallel for
#pragma omp parallel for schedule(static) num_threads(config.numThreads)
    for (size_t el = 0; el < m_elNum; ++el)
    {
        for (int n = 0; n < m_elNumNodes; ++n)
        {
            int i = el * m_elNumNodes + n;

            // Pressure flux
            Flux[0][i] = {v0[0] * u[0][i] + rho0 * c0 * c0 * u[1][i],
                          v0[1] * u[0][i] + rho0 * c0 * c0 * u[2][i],
                          v0[2] * u[0][i] + rho0 * c0 * c0 * u[3][i]};
            // Vx
            Flux[1][i] = {v0[0] * u[1][i] + u[0][i] / rho0,
                          v0[1] * u[1][i],
                          v0[2] * u[1][i]};
            // Vy
            Flux[2][i] = {v0[0] * u[2][i],
                          v0[1] * u[2][i] + u[0][i] / rho0,
                          v0[2] * u[2][i]};
            // Vz
            Flux[3][i] = {v0[0] * u[3][i],
                          v0[1] * u[3][i],
                          v0[2] * u[3][i] + u[0][i] / rho0};
        }

        // Ghost elements
        for (int f = 0; f < m_fNumPerEl; ++f)
        {
            int fId = elFId(el, f);
            if (m_fIsBoundary[fId])
            {

                for (int g = 0; g < m_fNumIntPts; ++g)
                {
                    int gId = fId * m_fNumIntPts + g;

                    // Interpolate solution at integration points
                    uGhost[0][gId] = 0;
                    uGhost[1][gId] = 0;
                    uGhost[2][gId] = 0;
                    uGhost[3][gId] = 0;
                    for (int n = 0; n < m_fNumNodes; ++n)
                    {
                        int nId = el * m_elNumNodes + fNToElNId(fId, n, 0);
////////////////////////
#pragma omp atomic
                        uGhost[0][gId] += u[0][nId] * fBasisFct(g, n);
#pragma omp atomic
                        uGhost[1][gId] += u[1][nId] * fBasisFct(g, n);
#pragma omp atomic
                        uGhost[2][gId] += u[2][nId] * fBasisFct(g, n);
#pragma omp atomic
                        uGhost[3][gId] += u[3][nId] * fBasisFct(g, n);
                    }

                    if (m_fBC[fId] == 1)
                    {
                        double nx(fNormal(fId, g, 0)), ny(fNormal(fId, g, 1)), nz(fNormal(fId, g, 2));
                        double dot = nx * uGhost[1][gId] +
                                     ny * uGhost[2][gId] +
                                     nz * uGhost[3][gId];
// #pragma omp critical
                        // std::cout << nx << " " << ny << " " << nz << std::endl;

// Remove normal component (Rigid Wall BC)
#pragma omp atomic
                        uGhost[1][gId] -= dot * nx;
#pragma omp atomic
                        uGhost[2][gId] -= dot * ny;
#pragma omp atomic
                        uGhost[3][gId] -= dot * nz;

                        // Flux at integration points
                        // 1) Pressure flux
                        FluxGhost[0][gId] = {v0[0] * uGhost[0][gId] + rho0 * c0 * c0 * uGhost[1][gId],
                                             v0[1] * uGhost[0][gId] + rho0 * c0 * c0 * uGhost[2][gId],
                                             v0[2] * uGhost[0][gId] + rho0 * c0 * c0 * uGhost[3][gId]};
                        // 2) Vx
                        FluxGhost[1][gId] = {v0[0] * uGhost[1][gId] + uGhost[0][gId] / rho0,
                                             v0[1] * uGhost[1][gId],
                                             v0[2] * uGhost[1][gId]};
                        // 3) Vy
                        FluxGhost[2][gId] = {v0[0] * uGhost[2][gId],
                                             v0[1] * uGhost[2][gId] + uGhost[0][gId] / rho0,
                                             v0[2] * uGhost[2][gId]};
                        // 4) Vz
                        FluxGhost[3][gId] = {v0[0] * uGhost[3][gId],
                                             v0[1] * uGhost[3][gId],
                                             v0[2] * uGhost[3][gId] + uGhost[0][gId] / rho0};

                        // Project Flux on the normal
                        for (int eq = 0; eq < 4; ++eq)
                            FluxGhost[eq][gId][0] = eigen::dot(&fNormal(fId, g), &FluxGhost[eq][gId][0], m_Dim);
                    }
                    else
                    {
                        // Absorbing boundary conditions
                        // /!\ Flux already projected on normal,
                        FluxGhost[0][gId][0] = RKR[gId][0] * uGhost[0][gId] +
                                               RKR[gId][1] * uGhost[1][gId] +
                                               RKR[gId][2] * uGhost[2][gId] +
                                               RKR[gId][3] * uGhost[3][gId];
                        FluxGhost[1][gId][0] = RKR[gId][4] * uGhost[0][gId] +
                                               RKR[gId][5] * uGhost[1][gId] +
                                               RKR[gId][6] * uGhost[2][gId] +
                                               RKR[gId][7] * uGhost[3][gId];
                        FluxGhost[2][gId][0] = RKR[gId][8] * uGhost[0][gId] +
                                               RKR[gId][9] * uGhost[1][gId] +
                                               RKR[gId][10] * uGhost[2][gId] +
                                               RKR[gId][11] * uGhost[3][gId];
                        FluxGhost[3][gId][0] = RKR[gId][12] * uGhost[0][gId] +
                                               RKR[gId][13] * uGhost[1][gId] +
                                               RKR[gId][14] * uGhost[2][gId] +
                                               RKR[gId][15] * uGhost[3][gId];
                    }
                }
            }
        }
    }
}

/**
 * List of nodes for each unique face given a list of node per face and per elements
 */

void Mesh::getUniqueFaceNodeTags()
{
    // Ordering per face for efficient comparison
    m_elFNodeTagsOrdered = m_elFNodeTags;
    // #pragma omp parallel for
    for (int i = 0; i < m_elFNodeTagsOrdered.size(); i += m_fNumNodes)
        std::sort(/*std::execution::par,*/ m_elFNodeTagsOrdered.begin() + i, m_elFNodeTagsOrdered.begin() + (i + m_fNumNodes));

    screen_display::write_string("get Unique Face Node Tags", RED);

    m_fNodeTags = m_elFNodeTags;
    std::vector<size_t> m_fNodeTags_t;
    std::vector<size_t> m_fNodeTags_tmp;
    std::vector<std::vector<size_t>> m_fNodeTags_tab;
    size_t fNumNativeNodes;

    //! TIMER start ////////////////////////////////
    auto start = std::chrono::system_clock::now();
    //! ////////////////////////////////////////////
    if (m_fDim < 2)
    {
        screen_display::write_string("Create and get all egdes", BLUE);
        std::vector<size_t> edge_tags;
        gmsh::model::mesh::createEdges();
        gmsh::model::mesh::getAllEdges(edge_tags, m_fNodeTags_t);
        fNumNativeNodes = 2;

        std::vector<std::vector<size_t>> m_fNodeTags_tab_full = vector_to_matrix(m_fNodeTags, m_fNumNodes);
        std::vector<std::vector<size_t>> m_fNodeTags_t_tab = vector_to_matrix(m_fNodeTags_t, fNumNativeNodes);

        for (size_t i = 0; i < m_fNodeTags_tab_full.size(); i++)
        {
            for (size_t j = 0; j < m_fNodeTags_t_tab.size(); j++)
            {
                if (isNCoincidentValues2d(m_fNodeTags_tab_full[i], m_fNodeTags_t_tab[j]))
                {
                    m_fNodeTags_tab.push_back(m_fNodeTags_tab_full[i]);
                    erase_row_from_matrix(m_fNodeTags_t_tab, j);
                    break;
                }
            }
        }
    }
    else
    {
        screen_display::write_string("Create and get all faces", BLUE);
        std::vector<size_t> face_tags;
        gmsh::model::mesh::createFaces();
        gmsh::model::mesh::getAllFaces(3, face_tags, m_fNodeTags_t);
        fNumNativeNodes = 3;

        std::vector<std::vector<size_t>> m_fNodeTags_tab_full = vector_to_matrix(m_fNodeTags, m_fNumNodes);
        std::vector<std::vector<size_t>> m_fNodeTags_t_tab = vector_to_matrix(m_fNodeTags_t, fNumNativeNodes);

        for (size_t i = 0; i < m_fNodeTags_tab_full.size(); i++)
        {
            for (size_t j = 0; j < m_fNodeTags_t_tab.size(); j++)
            {
                if (isNCoincidentValues3d(m_fNodeTags_tab_full[i], m_fNodeTags_t_tab[j]))
                {
                    m_fNodeTags_tab.push_back(m_fNodeTags_tab_full[i]);
                    erase_row_from_matrix(m_fNodeTags_t_tab, j);
                    break;
                }
            }
        }

        // auto inner_loop = [&](std::vector<size_t> &V)
        // {
        //     for (size_t j = 0; j < m_fNodeTags_t_tab.size(); j++)
        //     {
        //         if (isNCoincidentValues3d(V, m_fNodeTags_t_tab[j])) // fNumNativeNodes
        //         {
        //             m_fNodeTags_tab.push_back(V);
        //             erase_row_from_matrix(m_fNodeTags_t_tab, j);
        //             break;
        //         }
        //     }
        // };
        // for_each(m_fNodeTags_tab_full.begin(), m_fNodeTags_tab_full.end(), inner_loop);
    }

    size_t tmp;
    // m_fNodeTags.clear();
    m_fNodeTags = matrix_to_vector(m_fNodeTags_tab, tmp);
    // m_fNodeTagsOrdered.clear();
    m_fNodeTagsOrdered = m_fNodeTags;
    for (int i = 0; i < m_fNodeTagsOrdered.size(); i += m_fNumNodes)
        std::sort(/*std::execution::par,*/ m_fNodeTagsOrdered.begin() + i, m_fNodeTagsOrdered.begin() + (i + m_fNumNodes));

    screen_display::write_if_false(tmp == m_fNumNodes, "Bad dimension error...");

    //! TIMER END ///////////////////////////////////////////////////////////////////
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time (getUniqueFaceNodeTags):", elapsed.count() * 1.0e-6, "s", BLUE);
    //! //////////////////////////////////////////////////////////////////////////////
}

std::vector<size_t> vector_of_tags(size_t vec_size, size_t offset)
{
    std::vector<size_t> value;
    // std::cout<<offset<<std::endl;
    for (size_t i = 0, id = 0; i < vec_size; i++)
    {
        if (i % offset == 0)
            id++;
        value.push_back(id - 1);
        // value.push_back((i % offset == 0)?(id++ - 1):id);
    }
    return value;
}
void Mesh::getConnectivityFaceToElement()
{
    //! TIMER start ////////////////////////////////
    auto start = std::chrono::system_clock::now();
    //! ////////////////////////////////////////////
    m_fNbrElIds.resize(m_fNum);

    std::vector<std::vector<size_t>> m_elFNodeTagsOrdered_tab = vector_to_matrix(m_elFNodeTagsOrdered, m_fNumNodes);
    std::vector<std::vector<size_t>> m_fNodeTagsOrdered_tab = vector_to_matrix(m_fNodeTagsOrdered, m_fNumNodes);

    std::vector<size_t> elFtags = vector_of_tags(m_elFNodeTagsOrdered_tab.size(), m_fNumPerEl); //! vector of elements face tags
    std::vector<size_t> ftags = vector_of_tags(m_fNodeTagsOrdered_tab.size(), 1);               //! vector of face tags

    for (size_t i = 0; i < m_elFNodeTagsOrdered_tab.size(); i++)
    {
        for (size_t j = 0; j < m_fNodeTagsOrdered_tab.size(); j++)
        {
            if (isNCoincidentValues(m_elFNodeTagsOrdered_tab[i], m_fNodeTagsOrdered_tab[j], m_fNumNodes))
            {
                m_elFIds.push_back(ftags[j]);
                m_fNbrElIds[ftags[j]].push_back(elFtags[i]);
                if (m_fNbrElIds[ftags[j]].size() == 2)
                {
                    erase_row_from_matrix(m_fNodeTagsOrdered_tab, j);
                    erase_row_from_vector(ftags, j);
                    break;
                }
                // if(flag) break;
            }
        }
    }

    //! TIMER END ///////////////////////////////////////////////////////////////////
    auto end = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    screen_display::write_value("Elapsed time (getConnectivityFaceToElement):", elapsed.count() * 1.0e-6, "s", BLUE);
    //! //////////////////////////////////////////////////////////////////////////////
}

/**
 * @brief Write VTK & PVD
 */

void Mesh::writeVTUb(std::string filename, std::vector<std::vector<double>> &u)
{
    // std::string filename = filename;
    screen_display::write_string("Write VTU: " + filename, BOLDRED);

    size_t eltype;
    std::vector<size_t> node_tag;
    std::vector<double> coord_tmp;
    std::vector<double> param_coord_tmp;
    gmsh::model::mesh::getNodes(node_tag, coord_tmp, param_coord_tmp);
    coord_tmp.clear();
    param_coord_tmp.clear();

    size_t elNumNodes = (m_elDim == 2) ? 3 : 4; //! 3 points: triangle , 4 points : tetrahedral

    vtkNew<vtkPoints> points;

    vtkNew<vtkCellArray> cellArray;
    vtkNew<vtkDoubleArray> pressure, density, velocity;
    vtkNew<vtkUnstructuredGrid> unstructuredGrid;
    vtkNew<vtkXMLUnstructuredGridWriter> writer;

    for (auto n : node_tag)
    {
        std::vector<double> coord, paramCoord;
        int _dim, _tag;
        gmsh::model::mesh::getNode(n, coord, paramCoord, _dim, _tag);
        points->InsertNextPoint(coord[0], coord[1], coord[2]);
    }

    for (size_t i = 0; i < getElNum(); i++)
    {
        vtkNew<vtkTetra> tetra;
        vtkNew<vtkTriangle> tri;
        for (size_t j = 0; j < elNumNodes; j++) /*getElNumNodes()*/
        {
            if (m_elDim == 3)
                tetra->GetPointIds()->SetId(j, elNodeTag(i, j) - 1);
            else
                tri->GetPointIds()->SetId(j, elNodeTag(i, j) - 1);
        }

        if (m_elDim == 3)
            cellArray->InsertNextCell(tetra);
        else
            cellArray->InsertNextCell(tri);
    }

    pressure->SetName("Pressure [Pa]");
    density->SetName("Density [kg/m³]");
    velocity->SetName("Velocity [m/s]");
    velocity->SetNumberOfComponents(3);

    std::vector<size_t> nb_occurence(node_tag.size(), 1);
    std::vector<double> pressure_vec(node_tag.size(), 0.0);
    std::vector<double> density_vec(node_tag.size(), 0.0);
    std::vector<double> vel_x_vec(node_tag.size(), 0.0);
    std::vector<double> vel_y_vec(node_tag.size(), 0.0);
    std::vector<double> vel_z_vec(node_tag.size(), 0.0);

    for (size_t el = 0; el < getElNum(); ++el)
    {
        double p(0.0), rho(0.0), vx(0.0), vy(0.0), vz(0.0);
        for (size_t n = 0; n < getElNumNodes(); ++n)
        {
            size_t elN = el * getElNumNodes() + n;
            p += u[0][elN];
            rho += u[0][elN] / (config.c0 * config.c0);
            vx += u[1][elN];
            vy += u[2][elN];
            vz += u[3][elN];
        }
        double V[] = {vx / getElNumNodes(), vy / getElNumNodes(), vz / getElNumNodes()};
        pressure->InsertNextValue(p / getElNumNodes());
        density->InsertNextValue(rho / getElNumNodes());
        velocity->InsertNextTuple(V);
    }

    unstructuredGrid->SetPoints(points);

    if (m_elDim == 3)
        unstructuredGrid->SetCells(VTK_TETRA, cellArray);
    else
        unstructuredGrid->SetCells(VTK_TRIANGLE, cellArray);

    unstructuredGrid->GetCellData()->AddArray(pressure);
    unstructuredGrid->GetCellData()->AddArray(density);
    unstructuredGrid->GetCellData()->AddArray(velocity);

    // Write file

    writer->SetFileName(filename.c_str());
    writer->SetInputData(unstructuredGrid);
    writer->Write();
}

void Mesh::writePVD(std::string filename)
{
    screen_display::write_string("Write PVD at " + filename, BOLDRED);
    std::ofstream file(filename.c_str(), std::ios_base::ate);

    file << "<VTKFile type=\"Collection\" version=\"1.0\" byte_order=\"LittleEndian\" header_type=\"UInt64\">" << std::endl;
    file << "  <Collection>" << std::endl;
    for (double t = config.timeStart, step = 0, tDisplay = 0; t <= config.timeEnd;
         t += config.timeStep, tDisplay += config.timeStep, ++step)
    {
        if (tDisplay >= config.timeRate || step == 0)
        {
            tDisplay = 0;
            std::string vtu_filename = "results/result" + std::to_string((int)step) + ".vtu";
            file << "    <DataSet timestep=\"" << t << "\" part=\"0\" file=\"" << vtu_filename << "\"/>" << std::endl;
        }
    }
    file << "  </Collection>" << std::endl;
    file << "</VTKFile>" << std::endl;

    file.close();
}
