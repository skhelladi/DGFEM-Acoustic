#include "eqEdit.h"

EQ_EDIT::EQ_EDIT()
{
    //ctor
}

EQ_EDIT::~EQ_EDIT()
{
    //dtor
}

constyp constb[CONSMAX]= {{"pi",M_PI},{"e",M_E}};
//vartyp vartb[VARMAX]= {{"x",0.0},{"y",0.0},{"z",0.0},{"a",0.0},{"b",0.0},{"c",0.0},{"d",0.0},{"f",0.0},{"g",0.0},{"h",0.0}};
////////////////////////////////////////////////////////////////////////////////
//                                  ERRORS                                    //
////////////////////////////////////////////////////////////////////////////////

std::string errtb[ERRORMAX]=
{
    "No function...",
    "Not an alpha-numeric character...",
    "Unknown function...",
    "Not defined variable: choose one among 't,x,y,z,a,b,c,d,f,g,h'...",
    "Probably you've used a bad operator...",
    "Bad function expression...",
    "Number of variable unsupported..."
};
////////////////////////////////////////////////////////////////////////////////
//                               FUNCTIONS                                    //
////////////////////////////////////////////////////////////////////////////////

std::string functb[FUNMAX]=
{
    "abs","int","frac","rond",
    "log","ln","exp",
    "sin","cos","tan","sh","ch","th",
    "asin","acos","atan","ash","ach","ath",
};

////////////////////////////////////////////////////////////////////////////////
//                               FUNCTIONS                                    //
////////////////////////////////////////////////////////////////////////////////

double F_FUNC(int k,double x)
{
    switch(k)
    {
    case F_ABS :
        return fabs(x);
    case F_INT :
        return floor(x);
    case F_FRAC:
        return x-floor(x);
    case F_ROND:
        return (x<0.5*(ceil(x)+floor(x)))?floor(x):ceil(x);
    case F_LOG :
        return log10(x);
    case F_LN  :
        return log(x);
    case F_EXP :
        return exp(x);
    case F_SIN :
        return sin(x);
    case F_COS :
        return cos(x);
    case F_TAN :
        return tan(x);
    case F_SH  :
        return sinh(x);
    case F_CH  :
        return cosh(x);
    case F_TH  :
        return tanh(x);
    case F_ASIN:
        return asin(x);
    case F_ACOS:
        return acos(x);
    case F_ATN :
        return atan(x);
    case F_ASH :
        return (x<0.0)?-log(sqrt(x*x+1.0)-x):log(x+sqrt(x*x+1.0));
    case F_ACH :
        return log(x+sqrt(x*x-1.0));
    case F_ATH :
        return 0.5*log((1+x)/(1-x));
    }
    return 0.0;
}


////////////////////////////////////////////////////////////////////////////////
//                                   WEIGHT                                   //
////////////////////////////////////////////////////////////////////////////////
char  weight(char op)
{
    return (
               (op==')')                   ? 1:
               (op=='+'||op=='-')          ? 2:
               (op=='*'||op=='/'||op=='m') ? 3:
               (op=='^')                   ? 4:
               (op=='('||op=='f')          ? 1:0
           );
}
//---------------------------------------------------------------------------

double EQ_EDIT::F_VALUE(std::string expr,bool cal)
{
    double         stack_x[F_STACKMAX];
    char           stack_o[F_STACKMAX];
    int            stack_f[F_STACKMAX];
    int            kx=-1,
                   ko=0,
                   kf=-1;
    int            state=0,
                   end_pos=0;
    char           *ci=NULL;
    char           cop;
    int            sop,
                   k;
    char           *cx=(char*)"";

    int            VARMAX = NVar;

    //strcpy(cx,expr.c_str());


    if(expr=="")
    {
        std::cerr<<errtb[NO_FUNCTION];
        return -1;
    }

    cx = (char*)expr.c_str();

    for(int i=0; i<F_STACKMAX; i++)
    {
        stack_x[i]=0;
    }

    stack_o[0]='(';
    while(end_pos<2)
    {
        switch(state)
        {
        case 0:

        case 1:
            if(*cx=='(')
            {
                stack_o[++ko]='(';
                cx++;
                state=0;
                break;
            }

            if(*cx=='-'&&state==0)
            {
                stack_o[++ko]='m';
                cx++;
                state=1;
                break;
            }

            if(!isalnum(*cx))
            {
                std::cerr<<errtb[NOT_ALPANUM];
                return -1;
            }


            if(isdigit(*cx))
            {
                stack_x[++kx]=strtod(cx,&ci);
                cx=ci;
                state=2;
                break;
            }

            k=0;

            while(isalnum(cx[k]))
                k++;
            ci=(char*)calloc(k+1,sizeof(char*));
            strncpy(ci,cx,k);
            cx+=k;
            if(*cx=='(')
            {
                for(k=0; k<FUNMAX; k++)
                    if(strcmp(ci,functb[k].c_str())==0)
                        break;
                free(ci);
                ci=NULL;
                if(FUNMAX<=k)
                {
                    std::cerr<<errtb[UNKNOWN_FUNC];
                    return -1;
                }

                stack_f[++kf]=k;
                stack_o[++ko]='f';
                cx++;
                state=0;
                break;
            }
            for(k=0; k<CONSMAX; k++)
                if(strcmp(ci,constb[k].name.c_str())==0)
                    break;
            if(CONSMAX<=k)
            {
                for(k=0; k<VARMAX; k++)
                    if(strcmp(ci,vartb[k].name.c_str())==0)
                        break;

                if(VARMAX<=k)
                {
                    //free(ci);
                    std::cerr<<errtb[NOT_DEFINED_VAR];
                    return -1;
                }
                stack_x[++kx]=vartb[k].val;
            }
            else
                stack_x[++kx]=constb[k].val;
            state=2;
            free(ci);
            break;
        case 2:
            cop=(*cx==0)?')':*cx++;
            sop=weight(cop);
            if(sop==0)
            {
                //free(ci);
                std::cerr<<errtb[BAD_OPERATOR];
                return -1;
            }

            state=1;
            while(state<2&&sop<=weight(stack_o[ko]))
            {
                switch(stack_o[ko])
                {
                case '+':
                    stack_x[kx-1]=cal?stack_x[kx-1]+stack_x[kx]:0.0;
                    kx--;
                    break;
                case '-':
                    stack_x[kx-1]=cal?stack_x[kx-1]-stack_x[kx]:0.0;
                    kx--;
                    break;
                case '*':
                    stack_x[kx-1]=cal?stack_x[kx-1]*stack_x[kx]:0.0;
                    kx--;
                    break;
                case '/':
                    stack_x[kx-1]=cal?stack_x[kx-1]/stack_x[kx]:0.0;
                    kx--;
                    break;
                case '^':
                    stack_x[kx-1]=cal?pow(stack_x[kx-1],stack_x[kx]):0.0;
                    kx--;
                    break;
                case 'm':
                    stack_x[kx]=cal?-stack_x[kx]:0.0;
                    break;
                case 'f':
                    stack_x[kx]=cal?F_FUNC(stack_f[kf],stack_x[kx]):0.0;
                    kf--;
                case '(':
                    state=2;
                }
                stack_o[ko--]=' ';
            }
            if(cop!=')')
                stack_o[++ko]=cop;
            else if(*cx==0)
                end_pos++;
        }


    }

    if(ko!=-1||kx!=0||kf!=-1)
    {
        std::cerr<<errtb[BAD_FUNC];
        return -1;
    }

    return stack_x[0];
}

double EQ_EDIT::value(bool go, std::string equation, std::vector<vartyp> var)
{
    if(go)
    {
        if(equation=="")
        {
            std::cerr<<errtb[NO_FUNCTION];
            return -1;
        }

        NVar = var.size();

        vartb = var;
        Equation  = equation;

        return F_VALUE(Equation.c_str(),true);
    }
    else
        return 0.0;
}
