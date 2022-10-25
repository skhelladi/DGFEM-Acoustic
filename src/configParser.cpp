#include <algorithm>
#include <cstdio>
#include <fstream>
#include <gmsh.h>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <utils.h>

#include "configParser.h"

/**
 * Parse et load config file.
 * Update accordingly the global variables.
 */
namespace config
{
    std::vector<std::string> split(std::string str, char delimiter)
    {
        std::vector<std::string> internal;
        std::stringstream ss(str); // Turn the string into a stream.
        std::string tok;

        while (getline(ss, tok, delimiter))
        {
            internal.push_back(tok);
        }

        return internal;
    }

    Config parseConfig(std::string name)
    {
        Config config;

        std::ifstream cFile(name);
        if (cFile.is_open())
        {
            std::string line;
            std::map<std::string, std::string> configMap;
            while (getline(cFile, line))
            {
                line.erase(std::remove_if(line.begin(), line.end(), isspace),
                           line.end());
                if (line[0] == '#' || line.empty())
                    continue;
                auto delimiterPos = line.find("=");
                auto name = line.substr(0, delimiterPos);
                auto value = line.substr(delimiterPos + 1);
                configMap[name] = value;
            }
            config.meshFileName = configMap["meshFileName"];
            config.timeStart = std::stod(configMap["timeStart"]);
            config.timeEnd = std::stod(configMap["timeEnd"]);
            config.timeStep = std::stod(configMap["timeStep"]);
            config.timeRate = std::stod(configMap["timeRate"]);
            config.elementType = configMap["elementType"];
            config.timeIntMethod = configMap["timeIntMethod"];
            // config.saveFile = configMap["saveFile"];
            config.numThreads = std::stoi(configMap["numThreads"]);
            config.numThreads = config.numThreads == 1 ? 0 : config.numThreads;
            config.v0[0] = std::stod(configMap["v0_x"]);
            config.v0[1] = std::stod(configMap["v0_y"]);
            config.v0[2] = std::stod(configMap["v0_z"]);
            config.rho0 = std::stod(configMap["rho0"]);
            config.c0 = std::stod(configMap["c0"]);

            for (std::map<std::string, std::string>::iterator iter = configMap.begin(); iter != configMap.end(); ++iter)
            {
                std::string key = iter->first;
                if (key.find("source") == 0)
                {
                    std::vector<std::string> sep = split(iter->second, ',');

                    if (sep[0] != "formula" && sep[0] != "file")
                    {
                        double x = std::stod(sep[1]);
                        double y = std::stod(sep[2]);
                        double z = std::stod(sep[3]);
                        double size = std::stod(sep[4]);
                        double amp = std::stod(sep[5]);
                        double freq = std::stod(sep[6]);
                        double phase = std::stod(sep[7]);
                        double duration = std::stod(sep[8]);
                        double pole;
                        if (sep[0] == "dipole")
                        {
                            pole = 1;

                            Sources S1("", {pole, x - size, y, z, size / 2., amp, freq, phase, duration});
                            Sources S2("", {pole, x + size, y, z, size / 2., amp, freq, phase + M_PI, duration});
                            config.sources.push_back(S1);
                            config.sources.push_back(S2);
                        }
                        else if (sep[0] == "quadrupole")
                        {
                            pole = 2;

                            Sources S1("", {pole, x - size, y, z, size / 2., amp, freq, phase, duration});
                            Sources S2("", {pole, x + size, y, z, size / 2., amp, freq, phase, duration});
                            Sources S3("", {pole, x, y - size, z, size / 2., amp, freq, phase + M_PI, duration});
                            Sources S4("", {pole, x, y + size, z, size / 2., amp, freq, phase + M_PI, duration});
                            config.sources.push_back(S1);
                            config.sources.push_back(S2);
                            config.sources.push_back(S3);
                            config.sources.push_back(S4);
                        }
                        else //! monopole
                        {
                            pole = 0;

                            Sources S("", {pole, x, y, z, size, amp, freq, phase, duration});
                            config.sources.push_back(S);
                        }
                    }
                    else
                    {

                        if (sep[0] == "formula")
                        {
                            std::string expr = sep[1];

                            if (expr.front() == '"')
                                expr.erase(0, 1);
                            if (expr.back() == '"')
                                expr.pop_back();

                            double x = std::stod(sep[2]);
                            double y = std::stod(sep[3]);
                            double z = std::stod(sep[4]);
                            double size = std::stod(sep[5]);
                            double duration = std::stod(sep[6]);
                            // double pole = -1;
                            Sources S(expr, {-1, x, y, z, size, duration});
                            config.sources.push_back(S);
                        }
                        else if (sep[0] == "file")
                        {
                            std::string filename = sep[1];
                            if (filename.front() == '"')
                                filename.erase(0, 1);
                            if (filename.back() == '"')
                                filename.pop_back();
                            double x = std::stod(sep[2]);
                            double y = std::stod(sep[3]);
                            double z = std::stod(sep[4]);
                            double size = std::stod(sep[5]);
                            // double duration = std::stod(sep[6]);
                            // double pole = -1;
                            if (fileExtension(filename) == "csv")
                            {
                                Sources S("", {-1, x, y, z, size}, io::parseCSVFile(filename, ';'));
                                config.sources.push_back(S);
                            }
                            else if (fileExtension(filename) == "wav")
                            {
                                Sources S("", {-1, x, y, z, size}, io::parseWAVEFile(filename));
                                config.sources.push_back(S);
                            }
                            else
                            {
                                Fatal_Error("Not supported data")
                            }

                            // getchar();
                        }
                    }
                }
                else if (key.find("observer") == 0)
                {
                    std::vector<std::string> sep = split(iter->second, ',');
                    double x = std::stod(sep[0]);
                    double y = std::stod(sep[1]);
                    double z = std::stod(sep[2]);
                    double size = std::stod(sep[3]);
                    std::vector<double> obs = {x, y, z, size};
                    config.observers.push_back(obs);
                }
                else if (key.find("initialCondtition") == 0)
                {
                    std::vector<std::string> sep = split(iter->second, ',');
                    double x = std::stod(sep[1]);
                    double y = std::stod(sep[2]);
                    double z = std::stod(sep[3]);
                    double size = std::stod(sep[4]);
                    double amp = std::stod(sep[5]);
                    std::vector<double> init1 = {0, x, y, z, size, amp};
                    config.initConditions.push_back(init1);
                }
            }

            std::string physName;
            gmsh::open(config.meshFileName);
            gmsh::vectorpair m_physicalDimTags;
            int bcDim = gmsh::model::getDimension() - 1;
            gmsh::model::getPhysicalGroups(m_physicalDimTags, bcDim);
            for (int p = 0; p < m_physicalDimTags.size(); ++p)
            {
                gmsh::model::getPhysicalName(m_physicalDimTags[p].first, m_physicalDimTags[p].second, physName);
                if (configMap[physName].find("Absorbing") == 0)
                {
                    config.physBCs[m_physicalDimTags[p].second] = std::make_pair("Absorbing", 0);
                }
                else if (configMap[physName].find("Reflecting") == 0)
                {
                    config.physBCs[m_physicalDimTags[p].second] = std::make_pair("Reflecting", 0);
                }
                else
                {
                    gmsh::logger::write("Not specified or supported boundary conditions.");
                }
            }
        }
        else
        {
            throw;
        }
        gmsh::logger::write("==================================================");
        gmsh::logger::write("Simulation parameters : ");
        gmsh::logger::write("Time step : " + std::to_string(config.timeStep));
        gmsh::logger::write("Final time : " + std::to_string(config.timeEnd));
        gmsh::logger::write("Mean flow velocity : (" + std::to_string(config.v0[0]) + "," + std::to_string(config.v0[1]) + "," + std::to_string(config.v0[2]) + ")");
        gmsh::logger::write("Mean density : " + std::to_string(config.rho0));
        gmsh::logger::write("Speed of sound : " + std::to_string(config.c0));
        gmsh::logger::write("Solver : " + config.timeIntMethod);

        return config;
    }
    Config parseJSON(std::string name)
    {

        Config config;

        std::ifstream cFile(name);

        if (cFile.is_open())
        {
            // mesh
            cFile >> config.jsonData;
            config.meshFileName = config.jsonData["mesh"]["File"];
            int nbBC = config.jsonData["mesh"]["BC"]["number"];
            std::string physName;
            gmsh::open(config.meshFileName);
            gmsh::vectorpair m_physicalDimTags;
            int bcDim = gmsh::model::getDimension() - 1;
            gmsh::model::getPhysicalGroups(m_physicalDimTags, bcDim);
            for (int p = 0; p < m_physicalDimTags.size(); ++p)
            {
                gmsh::model::getPhysicalName(m_physicalDimTags[p].first, m_physicalDimTags[p].second, physName);
                if (physName == config.jsonData["mesh"]["BC"]["boundary" + std::to_string(p + 1)]["name"])
                {
                    if (config.jsonData["mesh"]["BC"]["boundary" + std::to_string(p + 1)]["type"] == "Absorbing")
                        config.physBCs[m_physicalDimTags[p].second] = std::make_pair("Absorbing", 0);
                    else if (config.jsonData["mesh"]["BC"]["boundary" + std::to_string(p + 1)]["type"] == "Reflecting")
                        config.physBCs[m_physicalDimTags[p].second] = std::make_pair("Reflecting", 0);
                    else
                    {
                        gmsh::logger::write("Not specified or supported boundary conditions.");
                    }
                }
                else
                {
                    gmsh::logger::write("Not specified or supported boundary conditions.");
                }
            }
            screen_display::write_string("Mesh loaded",GREEN);
            // solver

            config.timeStart = config.jsonData["solver"]["time"]["start"];
            config.timeEnd = config.jsonData["solver"]["time"]["end"];
            config.timeStep = config.jsonData["solver"]["time"]["step"];
            config.timeRate = config.jsonData["solver"]["time"]["rate"];
            config.elementType = config.jsonData["solver"]["elementType"];
            config.timeIntMethod = config.jsonData["solver"]["timeIntMethod"];
            config.numThreads = config.jsonData["solver"]["numThreads"];
            config.numThreads = (config.numThreads == 1) ? 0 : config.numThreads;
            screen_display::write_string("Solver parameters loaded",GREEN);
            // initial conditions
            config.v0[0] = config.jsonData["initialization"]["meanFlow"]["vx"];
            config.v0[1] = config.jsonData["initialization"]["meanFlow"]["vy"];
            config.v0[2] = config.jsonData["initialization"]["meanFlow"]["vz"];
            config.rho0 = config.jsonData["initialization"]["meanFlow"]["rho"];
            config.c0 = config.jsonData["initialization"]["meanFlow"]["c"];

            int nbInit = config.jsonData["initialization"]["number"];
            for (int i = 0; i < nbInit; i++)
            {
                std::string str = config.jsonData["initialization"]["initialCondition" + std::to_string(i + 1)]["type"];
                int index = 0;
                if (str == "gaussian")
                    index = 0;

                double x = config.jsonData["initialization"]["initialCondition" + std::to_string(i + 1)]["x"];
                double y = config.jsonData["initialization"]["initialCondition" + std::to_string(i + 1)]["y"];
                double z = config.jsonData["initialization"]["initialCondition" + std::to_string(i + 1)]["z"];
                double size = config.jsonData["initialization"]["initialCondition" + std::to_string(i + 1)]["size"];
                double amp = config.jsonData["initialization"]["initialCondition" + std::to_string(i + 1)]["amplitude"];
                std::vector<double> init1 = {double(index), x, y, z, size, amp};
                config.initConditions.push_back(init1);
            }
            screen_display::write_string("Initial conditions loaded",GREEN);
            // observers
            int nbObs = config.jsonData["observers"]["number"];
            for (int i = 0; i < nbObs; i++)
            {
                double x = config.jsonData["observers"]["observer" + std::to_string(i + 1)]["x"];
                double y = config.jsonData["observers"]["observer" + std::to_string(i + 1)]["y"];
                double z = config.jsonData["observers"]["observer" + std::to_string(i + 1)]["z"];
                double size = config.jsonData["observers"]["observer" + std::to_string(i + 1)]["size"];
                std::vector<double> obs = {x, y, z, size};
                config.observers.push_back(obs);
            }
            screen_display::write_string("Observers coordinates loaded",GREEN);
            
            // sources
            int nbSrc = config.jsonData["sources"]["number"];
            for (int i = 0; i < nbSrc; i++)
            {
                std::string type = config.jsonData["sources"]["source" + std::to_string(i + 1)]["type"];
                std::string fct  = config.jsonData["sources"]["source" + std::to_string(i + 1)]["fct"];
                double x = config.jsonData["sources"]["source" + std::to_string(i + 1)]["x"];
                double y = config.jsonData["sources"]["source" + std::to_string(i + 1)]["y"];
                double z = config.jsonData["sources"]["source" + std::to_string(i + 1)]["z"];
                double size = config.jsonData["sources"]["source" + std::to_string(i + 1)]["size"];
                double amp = config.jsonData["sources"]["source" + std::to_string(i + 1)]["amplitude"];
                double freq = config.jsonData["sources"]["source" + std::to_string(i + 1)]["frequency"];
                double phase = config.jsonData["sources"]["source" + std::to_string(i + 1)]["phase"];
                double duration = config.jsonData["sources"]["source" + std::to_string(i + 1)]["duration"];
                double pole;

                if (type != "formula" && type != "file")
                {
                    if (type == "dipole")
                    {
                        pole = 1;

                        Sources S1("", {pole, x - size, y, z, size / 2., amp, freq, phase, duration});
                        Sources S2("", {pole, x + size, y, z, size / 2., amp, freq, phase + M_PI, duration});
                        config.sources.push_back(S1);
                        config.sources.push_back(S2);
                    }
                    else if (type == "quadrupole")
                    {
                        pole = 2;

                        Sources S1("", {pole, x - size, y, z, size / 2., amp, freq, phase, duration});
                        Sources S2("", {pole, x + size, y, z, size / 2., amp, freq, phase, duration});
                        Sources S3("", {pole, x, y - size, z, size / 2., amp, freq, phase + M_PI, duration});
                        Sources S4("", {pole, x, y + size, z, size / 2., amp, freq, phase + M_PI, duration});
                        config.sources.push_back(S1);
                        config.sources.push_back(S2);
                        config.sources.push_back(S3);
                        config.sources.push_back(S4);
                    }
                    else //! monopole
                    {
                        pole = 0;

                        Sources S("", {pole, x, y, z, size, amp, freq, phase, duration});
                        config.sources.push_back(S);
                    }
                }
                else
                {

                    if (type == "formula")
                    {
                        std::string expr = fct;
                        expr.erase(std::remove_if(expr.begin(), expr.end(), isspace),expr.end());

                        if (expr.front() == '"')
                            expr.erase(0, 1);
                        if (expr.back() == '"')
                            expr.pop_back();
                            
                        Sources S(expr, {-1, x, y, z, size, duration});
                        config.sources.push_back(S);
                    }
                    else if (type == "file")
                    {
                        std::string filename = fct;
                        if (filename.front() == '"')
                            filename.erase(0, 1);
                        if (filename.back() == '"')
                            filename.pop_back();
                        // double duration = std::stod(sep[6]);
                        // double pole = -1;
                        if (fileExtension(filename) == "csv")
                        {
                            Sources S("", {-1, x, y, z, size}, io::parseCSVFile(filename, ';'));
                            config.sources.push_back(S);
                        }
                        else if (fileExtension(filename) == "wav")
                        {
                            Sources S("", {-1, x, y, z, size}, io::parseWAVEFile(filename));
                            config.sources.push_back(S);
                        }
                        else
                        {
                            Fatal_Error("Not supported data")
                        }

                        // getchar();
                    }
                }
            }
            screen_display::write_string("Sources loaded",GREEN);
            
        }
        else
        {
            throw;
        }
        gmsh::logger::write("==================================================");
        gmsh::logger::write("Simulation parameters : ");
        gmsh::logger::write("Time step : " + std::to_string(config.timeStep));
        gmsh::logger::write("Final time : " + std::to_string(config.timeEnd));
        gmsh::logger::write("Mean flow velocity : (" + std::to_string(config.v0[0]) + "," + std::to_string(config.v0[1]) + "," + std::to_string(config.v0[2]) + ")");
        gmsh::logger::write("Mean density : " + std::to_string(config.rho0));
        gmsh::logger::write("Speed of sound : " + std::to_string(config.c0));
        gmsh::logger::write("Solver : " + config.timeIntMethod);

        return config;
    }
}
