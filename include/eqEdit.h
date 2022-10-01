#ifndef EQEDIT_H
#define EQEDIT_H

#include <math.h>
#include <string>
#include <stdlib.h>
#include <ctype.h>
#include <iostream>
#include <cstdarg>
#include <cstring>
#include <vector>
//////////////////////////
//////////////////////////

// using namespace std;

////////////////////////////////////////////////////////////////////////////////
//                              CONSTANTES                                    //
////////////////////////////////////////////////////////////////////////////////
#define  CONSMAX    2
#define  FUNMAX     19
#define  F_STACKMAX 100
#define  ERRORMAX   7
////////////////////////////////////////////////////////////////////////////////


////////////////////////////////////////////////////////////////////////////////
//                              CONSTANTES ARRYS                              //
////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    std::string   name;
    double val;
} constyp;

////////////////////////////////////////////////////////////////////////////////
//                                 VARIABLES                                  //
////////////////////////////////////////////////////////////////////////////////
typedef struct
{
    std::string name;
    double val;
} vartyp;

////////////////////////////////////////////////////////////////////////////////
//                                  ERRORS                                    //
////////////////////////////////////////////////////////////////////////////////
typedef enum
{
  NO_FUNCTION, NOT_ALPANUM, UNKNOWN_FUNC, NOT_DEFINED_VAR, BAD_OPERATOR, BAD_FUNC, UNSUPPORTED_VAR_NUM
} error_id;


////////////////////////////////////////////////////////////////////////////////
//                               FUNCTIONS                                    //
////////////////////////////////////////////////////////////////////////////////
typedef enum
{
    F_ABS,F_INT,F_FRAC,F_ROND,
    F_LOG,F_LN,F_EXP,
    F_SIN,F_COS,F_TAN,F_SH,F_CH,F_TH,
    F_ASIN,F_ACOS,F_ATN,F_ASH,F_ACH,F_ATH
} funcname;

double F_FUNC(int k,double x);

////////////////////////////////////////////////////////////////////////////////
//                                   WEIGHT                                   //
////////////////////////////////////////////////////////////////////////////////
char  weight(char op);
//---------------------------------------------------------------------------

class EQ_EDIT
{
public:
    EQ_EDIT();
    virtual ~EQ_EDIT();
    void    Set_NVar(int value)
    {
        NVar=value;
    }
    int     Get_NVar()
    {
        return NVar;
    }
    void    Set_Equation(std::string value)
    {
        Equation=value;
    }
    std::string  Get_Equation()
    {
        return Equation;
    }
    //double  Value(bool go,vector<double> value);   //y=f(x)
    double  value(bool go, std::string equation, std::vector<vartyp> var={{"x",0.0}});
private:
    double F_VALUE(std::string expr,bool cal);
    std::string Equation;
    int    NVar;
    std::vector<vartyp> vartb;

};

#endif // EQEDIT_H
