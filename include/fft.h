#ifndef FFT_H_INCLUDED
#define FFT_H_INCLUDED


#include <complex>
#include <iostream>
#include <valarray>
#include <vector>
#include <fstream>
#include <fftw3.h>
#include <Eigen/Eigen>
#include "utils.h"
#define REAL 0
#define IMAG 1

// using namespace std;
using namespace Eigen;


const double PI = 3.141592653589793238460;

typedef std::complex<double> Complex;
typedef std::valarray<Complex> CArray;

// Cooley–Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
void _fft(CArray& x);

// Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
// Better optimized but less intuitive
// !!! Warning : in some cases this code make result different from not optimased version above (need to fix bug)
// The bug is now fixed @2017/05/30
void fft(CArray& x);

// inverse fft (in-place)
void ifft(CArray& x);

size_t getNearestLowerPowerOf2(size_t number); //! used for FFT
size_t getNearestPowerOf2(size_t number);

//vector<vector<double> > FFT_spec(vector<vector<double> > usrData);
MatrixXd FFT_spec(MatrixXd usrData);
MatrixXd FFTW_spec(MatrixXd usrData);
MatrixXd PSD_spec(MatrixXd usrData);

//int TestFFT(vector<vector<double> > usrData);
int WriteFFT(MatrixXd usrData, std::string filename);
int WritePSD(MatrixXd usrData, std::string filename);


#endif // FFT_H_INCLUDED
