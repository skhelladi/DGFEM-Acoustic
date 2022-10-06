#include <fstream>
#include <map>
#include <string>
#include "eqEdit.h"

#ifndef DGALERKIN_CONFIG_H
#define DGALERKIN_CONFIG_H

//! ////////////////////////////////////////////////////////////////
class Sources
{
public:
    Sources(std::string f, std::vector<double> s)
    {
        formula = f;
        source = s;
    }
    Sources() {}
    std::string formula = "";
    std::vector<double> source;
    EQ_EDIT expression;
    double value(double t)
    {
        return expression.value(true,formula,{{"t",t}});
    }
};

struct Config
{
    // Initial, final time and time step(t>0)
    double timeStart = 0;
    double timeEnd = 1;
    double timeStep = 0.1;
    double timeRate = 0.1;

    // Element Type:
    std::string elementType = "Lagrange";

    // Time integration method
    std::string timeIntMethod = "Euler1";

    // Boundary condition
    // key : physical group Tag
    // value : tuple<BCType, BCValue>
    std::map<int, std::pair<std::string, double>> physBCs;

    // Mean flow parameters
    std::vector<double> v0 = {0, 0, 0};
    double rho0 = 1;
    double c0 = 1;

    // Number of threads
    int numThreads = 1;

    // Sources
    // struct sources
    // {
    //     std::vector<std::vector<double>> source;
    //     std::string source_expression = "";
    // };
    std::vector<Sources> sources;
    // Initial conditions
    std::vector<std::vector<double>> initConditions;

    // Save file
    // std::string saveFile = "results.msh";
};

namespace config
{
    Config parseConfig(std::string name);
}

#endif
