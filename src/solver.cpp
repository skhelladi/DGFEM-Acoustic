#include <chrono>
#include <gmsh.h>
#include <iostream>
#include <omp.h>
#include <sstream>
#include <utils.h>
#include <vector>

#include "Mesh.h"
#include "configParser.h"

namespace solver
{

    /**
     * Common variables to all solver
     */
    int elNumNodes;
    int numNodes;
    std::vector<std::string> g_names;
    std::vector<int> elTags;
    std::vector<double> elFlux;
    std::vector<double> elStiffvector;
    std::vector<std::vector<std::vector<double>>> Flux;

    std::vector<std::vector<float>> data4wave;

    /**
     * Perform a numerical step: u[t+1] = dt*M^-1*(S[u[t]]-F[u[t]]) + beta*u[t]
     * for all elements in mesh object.
     *
     * @param mesh Mesh object
     * @param config Configuration file
     * @param u Nodal solution vector
     * @param Flux Nodal physical Flux
     * @param beta double coefficient
     */
    void numStep(Mesh &mesh, Config config, std::vector<std::vector<double>> &u,
                 std::vector<std::vector<std::vector<double>>> &Flux, double beta)
    {

        for (int eq = 0; eq < 4; ++eq)
        {
            mesh.precomputeFlux(u[eq], Flux[eq], eq);

#pragma omp parallel for schedule(static) firstprivate(elFlux, elStiffvector) num_threads(config.numThreads)
            for (int el = 0; el < mesh.getElNum(); ++el)
            {

                mesh.getElFlux(el, elFlux.data());
                mesh.getElStiffVector(el, Flux[eq], u[eq], elStiffvector.data());
                eigen::minus(elStiffvector.data(), elFlux.data(), elNumNodes);
                eigen::linEq(&mesh.elMassMatrix(el), &elStiffvector[0], &u[eq][el * elNumNodes],
                             config.timeStep, beta, elNumNodes);
            }
        }
    }

    /**
     * Solve using forward explicit scheme. O(h)
     *
     * @param u initial nodal solution vector
     * @param mesh
     * @param config
     */
    void forwardEuler(std::vector<std::vector<double>> &u, Mesh &mesh, Config config)
    {

        /** Memory allocation */
        elNumNodes = mesh.getElNumNodes();
        numNodes = mesh.getNumNodes();
        elTags = std::vector<int>(&mesh.elTag(0), &mesh.elTag(0) + mesh.getElNum());
        elFlux.resize(elNumNodes);
        elStiffvector.resize(elNumNodes);
        Flux = std::vector<std::vector<std::vector<double>>>(4,
                                                             std::vector<std::vector<double>>(mesh.getNumNodes(),
                                                                                              std::vector<double>(3)));

        /** Gmsh save init */
        gmsh::model::list(g_names);
        int gp_viewTag = gmsh::view::add("Pressure");
        int gv_viewTag = gmsh::view::add("Velocity");
        int grho_viewTag = gmsh::view::add("Density");
        std::vector<std::vector<double>> g_p(mesh.getElNum(), std::vector<double>(elNumNodes));
        std::vector<std::vector<double>> g_rho(mesh.getElNum(), std::vector<double>(elNumNodes));
        std::vector<std::vector<double>> g_v(mesh.getElNum(), std::vector<double>(3 * elNumNodes));

        /** Precomputation */
        mesh.precomputeMassMatrix();

        /** Source */
        std::vector<std::vector<int>> srcIndices;
        for (int i = 0; i < config.sources.size(); ++i)
        {
            std::vector<int> indice;
            for (int n = 0; n < mesh.getNumNodes(); n++)
            {
                std::vector<double> coord, paramCoord;
                int _dim, _tag;
                gmsh::model::mesh::getNode(mesh.getElNodeTags()[n], coord, paramCoord, _dim, _tag);
                if (pow(coord[0] - config.sources[i].source[1], 2) +
                        pow(coord[1] - config.sources[i].source[2], 2) +
                        pow(coord[2] - config.sources[i].source[3], 2) <
                    pow(config.sources[i].source[4], 2))
                {
                    indice.push_back(n);
                }
            }
            srcIndices.push_back(indice);
        }

        /** Observer */
        std::vector<std::vector<int>> obsIndices;
        std::vector<std::vector<double>> obsPtDistance;
        for (int i = 0; i < config.observers.size(); ++i)
        {
            std::vector<int> indice;
            std::vector<double> dist;
            for (int n = 0; n < mesh.getNumNodes(); n++)
            {
                std::vector<double> coord, paramCoord;
                int _dim, _tag;
                gmsh::model::mesh::getNode(mesh.getElNodeTags()[n], coord, paramCoord, _dim, _tag);
                double distance = sqrt(pow(coord[0] - config.observers[i][0], 2) +
                                       pow(coord[1] - config.observers[i][1], 2) +
                                       pow(coord[2] - config.observers[i][2], 2));
                if (distance < config.observers[i][3])
                {
                    indice.push_back(n);
                    dist.push_back(distance);
                }
            }
            obsIndices.push_back(indice);
            obsPtDistance.push_back(dist);
        }

        /**
         * Main Loop : Time iteration
         */
        std::ofstream outfile("residuals.csv");
        outfile << "time;res_p;res_rho;res_vx;res_vy;res_vz;elapsed_time" << std::endl;

        std::vector<std::ofstream> obs_outfile(config.observers.size());
        data4wave.clear();
        data4wave.resize(config.observers.size());
        for (int obs = 0; obs < config.observers.size(); ++obs)
        {
            std::string filename = "results/observers" + std::to_string(obs + 1) + ".txt";
            obs_outfile[obs].open(filename.c_str());
            obs_outfile[obs] << "time;pressure;density;velocity_x;velocity_y;velocity_z" << std::endl;
        }

        auto start = std::chrono::system_clock::now();
        for (double t = config.timeStart, step = 0, tDisplay = 0; t <= config.timeEnd;
             t += config.timeStep, tDisplay += config.timeStep, ++step)
        {

            auto start_time = std::chrono::system_clock::now();
            std::vector<double> residual(5, 0.0);
            /**
             *  Savings and prints
             */

            if (tDisplay >= config.timeRate || step == 0)
            {
                tDisplay = 0;

/** [1] Copy solution to match GMSH format */
#pragma omp parallel for schedule(static) num_threads(config.numThreads)
                for (int el = 0; el < mesh.getElNum(); ++el)
                {
                    for (int n = 0; n < mesh.getElNumNodes(); ++n)
                    {
                        int elN = el * elNumNodes + n;
                        g_p[el][n] = u[0][elN];
                        g_rho[el][n] = u[0][elN] / (config.c0 * config.c0);
                        g_v[el][3 * n + 0] = u[1][elN];
                        g_v[el][3 * n + 1] = u[2][elN];
                        g_v[el][3 * n + 2] = u[3][elN];
                    }
                }

                /** [2] Print and compute iteration time */
                auto end = std::chrono::system_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start);
                gmsh::logger::write("[" + std::to_string(t) + "/" + std::to_string(config.timeEnd) + "s] Step number : " + std::to_string((int)step) + ", Elapsed time: " + std::to_string(elapsed.count()) + "s");
                screen_display::write_string("time\t\tres_p\t\tres_rho\t\tres_vx\t\tres_vy\t\tres_vz\t\telapsed time", BOLDBLUE);
                // mesh.writeVTK("result.vtk");
                std::string vtu_filename = "results/result" + std::to_string((int)step) + ".vtu";
                mesh.writeVTUb(vtu_filename, u);
            }

            /**
             * Update Source
             */

            for (int src = 0; src < config.sources.size(); ++src)
            {
                if (config.sources[src].formula == "" && config.sources[src].data.empty())
                {
                    double amp = config.sources[src].source[5];
                    double freq = config.sources[src].source[6];
                    double phase = config.sources[src].source[7];
                    double duration = config.sources[src].source[8];
                    if (t < duration)
                        for (int n = 0; n < srcIndices[src].size(); ++n)
                            u[0][srcIndices[src][n]] = amp * sin(2 * M_PI * freq * t + phase);
                }
                else
                {
                    if (config.sources[src].data.empty())
                    {
                        double duration = config.sources[src].source[5];
                        if (t < duration)
                            for (int n = 0; n < srcIndices[src].size(); ++n)
                                u[0][srcIndices[src][n]] = config.sources[src].value(t);
                    }
                    else
                    {
                        for (int n = 0; n < srcIndices[src].size(); ++n)
                            u[0][srcIndices[src][n]] = config.sources[src].interpolate_value(t);
                    }
                }
            }

            /**
             * First Order Euler
             */
            mesh.updateFlux(u, Flux, config.v0, config.c0, config.rho0);
            numStep(mesh, config, u, Flux, 1);

            /**
             * Compute residuals
             */
#pragma omp parallel for schedule(static) num_threads(config.numThreads)
            for (int el = 0; el < mesh.getElNum(); ++el)
            {
                for (int n = 0; n < mesh.getElNumNodes(); ++n)
                {
                    int elN = el * elNumNodes + n;
#pragma omp atomic update
                    residual[0] += pow(g_p[el][n] - u[0][elN], 2);
#pragma omp atomic update
                    residual[1] += pow(g_rho[el][n] - u[0][elN] / (config.c0 * config.c0), 2);
#pragma omp atomic update
                    residual[2] += pow(g_v[el][3 * n + 0] - u[1][elN], 2);
#pragma omp atomic update
                    residual[3] += pow(g_v[el][3 * n + 1] - u[2][elN], 2);
#pragma omp atomic update
                    residual[4] += pow(g_v[el][3 * n + 2] - u[3][elN], 2);
                }
            }
            outfile << t << ";";
            std::cout << std::scientific << t << "\t";
            auto end_time = std::chrono::system_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            for (int eq = 0; eq < residual.size(); ++eq)
            {
                residual[eq] /= (mesh.getElNum() * mesh.getElNumNodes());
                std::cout << std::scientific << residual[eq] << "\t";
                outfile << residual[eq] << ";";
            }
            std::cout << elapsed_time.count() * 1.0e-6 << " s" << std::endl;
            outfile << elapsed_time.count() * 1.0e-6 << std::endl;

            /**
             * get observers value
             * Franke-Little interpolation method
             */
            for (int obs = 0; obs < config.observers.size(); ++obs)
            {
                double p(0), rho(0), w_sum(0);
                std::vector<double> v = {0, 0, 0};
                for (int n = 0; n < obsIndices[obs].size(); ++n)
                {
                    double R = config.observers[obs][3];                        //! influence sphere
                    double w = 1.0 / (pow(obsPtDistance[obs][n], 2) + 1.0e-12); // fmax(1.0-obsPtDistance[obs][n]/R,0.0);//1.0 / pow(obsPtDistance[obs][n],2);
                    p += u[0][obsIndices[obs][n]] * w;
                    v[0] += u[1][obsIndices[obs][n]] * w;
                    v[1] += u[2][obsIndices[obs][n]] * w;
                    v[2] += u[3][obsIndices[obs][n]] * w;
                    w_sum += w;
                }
                p /= w_sum;
                rho = p / pow(config.c0, 2);
                v[0] /= w_sum;
                v[1] /= w_sum;
                v[2] /= w_sum;
                data4wave[obs].push_back(p);
                obs_outfile[obs] << t << ";" << rho << ";" << p << ";" << v[0] << ";" << v[1] << ";" << v[2] << std::endl;
            }
        }
        for (int obs = 0; obs < config.observers.size(); ++obs)
        {
            io::writeWave(data4wave[obs], "results/observer_" + std::to_string(obs + 1) + ".wav", 1.0 / config.timeStep, 16, 1, 1);
            io::writeFFT(data4wave[obs],config.timeStep,"results/observer_" + std::to_string(obs + 1));
        }

        outfile.close();
        for (int obs = 0; obs < config.observers.size(); ++obs)
            obs_outfile[obs].close();
    }

    /**
     * Solve using explicit Runge-Kutta integration method. O(h^4)
     *
     * @param u initial nodal solution vector
     * @param mesh
     * @param config
     */
    void rungeKutta(std::vector<std::vector<double>> &u, Mesh &mesh, Config config)
    {

        /** Memory allocation */
        elNumNodes = mesh.getElNumNodes();
        numNodes = mesh.getNumNodes();
        elTags = std::vector<int>(&mesh.elTag(0), &mesh.elTag(0) + mesh.getElNum());
        elFlux.resize(elNumNodes);
        elStiffvector.resize(elNumNodes);
        std::vector<std::vector<double>> k1, k2, k3, k4;
        Flux = std::vector<std::vector<std::vector<double>>>(4,
                                                             std::vector<std::vector<double>>(mesh.getNumNodes(),
                                                                                              std::vector<double>(3)));

        /** Gmsh save init */
        gmsh::model::list(g_names);
        int gp_viewTag = gmsh::view::add("Pressure");
        int gv_viewTag = gmsh::view::add("Velocity");
        int grho_viewTag = gmsh::view::add("Density");
        std::vector<std::vector<double>> g_p(mesh.getElNum(), std::vector<double>(elNumNodes));
        std::vector<std::vector<double>> g_rho(mesh.getElNum(), std::vector<double>(elNumNodes));
        std::vector<std::vector<double>> g_v(mesh.getElNum(), std::vector<double>(3 * elNumNodes));

        /** Precomputation (constants over time) */
        screen_display::write_string("\t>>> Precomputation", BLUE);
        mesh.precomputeMassMatrix();
        screen_display::write_string("\t>>> precomputeMassMatrix", BLUE);

        /** Source */
        std::vector<std::vector<int>> srcIndices;
        for (int i = 0; i < config.sources.size(); ++i)
        {
            std::vector<int> indice;
            for (int n = 0; n < mesh.getNumNodes(); n++)
            {
                std::vector<double> coord, paramCoord;
                int _dim, _tag;
                gmsh::model::mesh::getNode(mesh.getElNodeTags()[n], coord, paramCoord, _dim, _tag);
                if (pow(coord[0] - config.sources[i].source[1], 2) +
                        pow(coord[1] - config.sources[i].source[2], 2) +
                        pow(coord[2] - config.sources[i].source[3], 2) <
                    pow(config.sources[i].source[4], 2))
                {
                    indice.push_back(n);
                }
            }
            srcIndices.push_back(indice);
        }

        /** Observer */
        std::vector<std::vector<int>> obsIndices;
        std::vector<std::vector<double>> obsPtDistance;
        for (int i = 0; i < config.observers.size(); ++i)
        {
            std::vector<int> indice;
            std::vector<double> dist;
            for (int n = 0; n < mesh.getNumNodes(); n++)
            {
                std::vector<double> coord, paramCoord;
                int _dim, _tag;
                gmsh::model::mesh::getNode(mesh.getElNodeTags()[n], coord, paramCoord, _dim, _tag);
                double distance = sqrt(pow(coord[0] - config.observers[i][0], 2) +
                                       pow(coord[1] - config.observers[i][1], 2) +
                                       pow(coord[2] - config.observers[i][2], 2));
                if (distance < config.observers[i][3])
                {
                    indice.push_back(n);
                    dist.push_back(distance);
                }
            }
            obsIndices.push_back(indice);
            obsPtDistance.push_back(dist);
        }

        // for (int i = 0; i < obsIndices.size(); i++)
        // {
        //     for (int j = 0; j < obsIndices[i].size(); j++)
        //     {
        //         std::cout << obsIndices[i][j] << "\t";
        //     }
        //     std::cout << std::endl;
        // }
        // getchar();

        /**
         * Main Loop : Time iteration
         */

        std::ofstream outfile("residuals.csv");
        outfile << "time;res_p;res_rho;res_vx;res_vy;res_vz;elapsed_time" << std::endl;

        std::vector<std::ofstream> obs_outfile(config.observers.size());
        data4wave.clear();
        data4wave.resize(config.observers.size());
        for (int obs = 0; obs < config.observers.size(); ++obs)
        {
            std::string filename = "results/observers" + std::to_string(obs + 1) + ".txt";
            obs_outfile[obs].open(filename.c_str());
            obs_outfile[obs] << "time;density;pressure;velocity_x;velocity_y;velocity_z" << std::endl;
        }

        auto start = std::chrono::system_clock::now();
        for (double t = config.timeStart, step = 0, tDisplay = 0; t <= config.timeEnd;
             t += config.timeStep, tDisplay += config.timeStep, ++step)
        {
            auto start_time = std::chrono::system_clock::now();
            std::vector<double> residual(5, 0.0);
            /**
             *  Savings and prints
             */
            if (tDisplay >= config.timeRate || step == 0)
            {
                tDisplay = 0;

/** [1] Copy solution to match GMSH format */
// #pragma omp parallel for
#pragma omp parallel for schedule(static) num_threads(config.numThreads)
                for (int el = 0; el < mesh.getElNum(); ++el)
                {
                    for (int n = 0; n < mesh.getElNumNodes(); ++n)
                    {
                        int elN = el * elNumNodes + n;
                        g_p[el][n] = u[0][elN];
                        g_rho[el][n] = u[0][elN] / (config.c0 * config.c0);
                        g_v[el][3 * n + 0] = u[1][elN];
                        g_v[el][3 * n + 1] = u[2][elN];
                        g_v[el][3 * n + 2] = u[3][elN];
                    }
                }
                // gmsh::view::addModelData(gp_viewTag, step, g_names[0], "ElementNodeData", elTags, g_p, t, 1);
                // gmsh::view::addModelData(grho_viewTag, step, g_names[0], "ElementNodeData", elTags, g_rho, t, 1);
                // gmsh::view::addModelData(gv_viewTag, step, g_names[0], "ElementNodeData", elTags, g_v, t, 3);

                /** [2] Print and compute iteration time */
                auto end = std::chrono::system_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(end - start);
                gmsh::logger::write("[" + std::to_string(t) + "/" + std::to_string(config.timeEnd) + "s] Step number : " + std::to_string((int)step) + ", Elapsed time: " + std::to_string(elapsed.count()) + "s");
                screen_display::write_string("time\t\tres_p\t\tres_rho\t\tres_vx\t\tres_vy\t\tres_vz\t\telapsed time", BOLDBLUE);
                // mesh.writeVTK("result.vtk");
                std::string vtu_filename = "results/result" + std::to_string((int)step) + ".vtu";
                mesh.writeVTUb(vtu_filename, u);
                // mesh.writeVTK("result.vtk",u);
            }

            /** Source */
            for (int src = 0; src < config.sources.size(); ++src)
            {
                if (config.sources[src].formula == "" && config.sources[src].data.empty())
                {
                    double amp = config.sources[src].source[5];
                    double freq = config.sources[src].source[6];
                    double phase = config.sources[src].source[7];
                    double duration = config.sources[src].source[8];
                    if (t < duration)
                        for (int n = 0; n < srcIndices[src].size(); ++n)
                            u[0][srcIndices[src][n]] = amp * sin(2 * M_PI * freq * t + phase);
                }
                else
                {
                    if (config.sources[src].data.empty())
                    {
                        double duration = config.sources[src].source[5];
                        if (t < duration)
                            for (int n = 0; n < srcIndices[src].size(); ++n)
                                u[0][srcIndices[src][n]] = config.sources[src].value(t);
                    }
                    else
                    {
                        for (int n = 0; n < srcIndices[src].size(); ++n)
                            u[0][srcIndices[src][n]] = config.sources[src].interpolate_value(t);
                    }
                }
            }

            /**
             * Fourth order Runge-Kutta algorithm
             */
            k1 = k2 = k3 = k4 = u;
            /** [1] Step R-K */
            mesh.updateFlux(k1, Flux, config.v0, config.c0, config.rho0);
            numStep(mesh, config, k1, Flux, 0);
            for (int eq = 0; eq < u.size(); ++eq)
                eigen::plusTimes(k2[eq].data(), k1[eq].data(), 0.5, numNodes);
            /** [2] Step R-K */
            mesh.updateFlux(k2, Flux, config.v0, config.c0, config.rho0);
            numStep(mesh, config, k2, Flux, 0);
            for (int eq = 0; eq < u.size(); ++eq)
                eigen::plusTimes(k3[eq].data(), k2[eq].data(), 0.5, numNodes);
            /** [3] Step R-K */
            mesh.updateFlux(k3, Flux, config.v0, config.c0, config.rho0);
            numStep(mesh, config, k3, Flux, 0);
            for (int eq = 0; eq < u.size(); ++eq)
                eigen::plusTimes(k4[eq].data(), k3[eq].data(), 1, numNodes);
            /** [4] Step R-K */
            mesh.updateFlux(k4, Flux, config.v0, config.c0, config.rho0);
            numStep(mesh, config, k4, Flux, 0);
            /** Concat results of R-K iterations */
            // #pragma omp parallel for
            for (int eq = 0; eq < u.size(); ++eq)
            {
                for (int i = 0; i < numNodes; ++i)
                {
                    // #pragma omp atomic
                    u[eq][i] += (k1[eq][i] + 2 * k2[eq][i] + 2 * k3[eq][i] + k4[eq][i]) / 6.0;
                }
            }

#pragma omp parallel for schedule(static) num_threads(config.numThreads)
            for (int el = 0; el < mesh.getElNum(); ++el)
            {
                for (int n = 0; n < mesh.getElNumNodes(); ++n)
                {
                    int elN = el * elNumNodes + n;
#pragma omp atomic update
                    residual[0] += pow(g_p[el][n] - u[0][elN], 2);
#pragma omp atomic update
                    residual[1] += pow(g_rho[el][n] - u[0][elN] / (config.c0 * config.c0), 2);
#pragma omp atomic update
                    residual[2] += pow(g_v[el][3 * n + 0] - u[1][elN], 2);
#pragma omp atomic update
                    residual[3] += pow(g_v[el][3 * n + 1] - u[2][elN], 2);
#pragma omp atomic update
                    residual[4] += pow(g_v[el][3 * n + 2] - u[3][elN], 2);
                }
            }
            outfile << t << ";";
            std::cout << std::scientific << t << "\t";
            auto end_time = std::chrono::system_clock::now();
            auto elapsed_time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
            for (int eq = 0; eq < residual.size(); ++eq)
            {
                residual[eq] /= (mesh.getElNum() * mesh.getElNumNodes());
                std::cout << std::scientific << residual[eq] << "\t";
                outfile << residual[eq] << ";";
            }
            std::cout << elapsed_time.count() * 1.0e-6 << " s" << std::endl;
            outfile << elapsed_time.count() * 1.0e-6 << std::endl;
            /**
             * get observers value
             * Inverse distance weight interpolation method
             */
            for (int obs = 0; obs < config.observers.size(); ++obs)
            {
                double p(0), rho(0), w_sum(0);
                std::vector<double> v = {0, 0, 0};
                for (int n = 0; n < obsIndices[obs].size(); ++n)
                {
                    double R = config.observers[obs][3];                        //! influence sphere
                    double w = 1.0 / (pow(obsPtDistance[obs][n], 2) + 1.0e-12); // fmax(1.0-obsPtDistance[obs][n]/R,0.0);//1.0 / pow(obsPtDistance[obs][n],2);
                    p += u[0][obsIndices[obs][n]] * w;
                    v[0] += u[1][obsIndices[obs][n]] * w;
                    v[1] += u[2][obsIndices[obs][n]] * w;
                    v[2] += u[3][obsIndices[obs][n]] * w;
                    w_sum += w;
                }
                p /= w_sum;
                rho = p / pow(config.c0, 2);
                v[0] /= w_sum;
                v[1] /= w_sum;
                v[2] /= w_sum;
                data4wave[obs].push_back(p);
                obs_outfile[obs] << t << ";" << rho << ";" << p << ";" << v[0] << ";" << v[1] << ";" << v[2] << std::endl;
            }
        }
        for (int obs = 0; obs < config.observers.size(); ++obs)
        {
            io::writeWave(data4wave[obs], "results/observer_" + std::to_string(obs + 1) + ".wav", 1.0 / config.timeStep, 16, 1, 1);
            io::writeFFT(data4wave[obs],config.timeStep,"results/observer_" + std::to_string(obs + 1));
        }

        outfile.close();
        for (int obs = 0; obs < config.observers.size(); ++obs)
            obs_outfile[obs].close();
    }
}
