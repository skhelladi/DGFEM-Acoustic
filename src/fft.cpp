#include "fft.h"

// Cooley–Tukey FFT (in-place, divide-and-conquer)
// Higher memory requirements and redundancy although more intuitive
void _fft(CArray &x)
{
    const size_t N = x.size();
    if (N <= 1)
        return;

    // divide
    CArray even = x[std::slice(0, N / 2, 2)];
    CArray odd = x[std::slice(1, N / 2, 2)];

    // conquer
    fft(even);
    fft(odd);

    // combine
    for (size_t k = 0; k < N / 2; ++k)
    {
        Complex t = std::polar(1.0, -2 * PI * k / N) * odd[k];
        x[k] = even[k] + t;
        x[k + N / 2] = even[k] - t;
    }
}

// Cooley-Tukey FFT (in-place, breadth-first, decimation-in-frequency)
// Better optimized but less intuitive
// !!! Warning : in some cases this code make result different from not optimased version above (need to fix bug)
// The bug is now fixed @2017/05/30
void fft(CArray &x)
{
    // DFT
    size_t N = x.size(), k = N, n;
    double thetaT = 3.14159265358979323846264338328L / N;
    Complex phiT = Complex(cos(thetaT), -sin(thetaT)), T;
    while (k > 1)
    {
        n = k;
        k >>= 1;
        phiT = phiT * phiT;
        T = 1.0L;
        for (size_t l = 0; l < k; l++)
        {
            for (size_t a = l; a < N; a += n)
            {
                size_t b = a + k;
                Complex t = x[a] - x[b];
                x[a] += x[b];
                x[b] = t * T;
            }
            T *= phiT;
        }
    }
    // Decimate
    size_t m = (size_t)log2(N);
    for (size_t a = 0; a < N; a++)
    {
        size_t b = a;
        // Reverse bits
        b = (((b & 0xaaaaaaaa) >> 1) | ((b & 0x55555555) << 1));
        b = (((b & 0xcccccccc) >> 2) | ((b & 0x33333333) << 2));
        b = (((b & 0xf0f0f0f0) >> 4) | ((b & 0x0f0f0f0f) << 4));
        b = (((b & 0xff00ff00) >> 8) | ((b & 0x00ff00ff) << 8));
        b = ((b >> 16) | (b << 16)) >> (32 - m);
        if (b > a)
        {
            Complex t = x[a];
            x[a] = x[b];
            x[b] = t;
        }
    }
    //// Normalize (This section make it not working correctly)
    // Complex f = 1.0 / sqrt(N);
    // for (size_t i = 0; i < N; i++)
    //	x[i] *= f;
}

// inverse fft (in-place)
void ifft(CArray &x)
{
    // conjugate the complex numbers
    x = x.apply(std::conj);

    // forward fft
    fft(x);

    // conjugate the complex numbers again
    x = x.apply(std::conj);

    // scale the numbers
    x /= x.size();
}

size_t getNearestLowerPowerOf2(size_t number)
{
    size_t n = floor(log(number) / log(2.0));
    return pow(2, n);
}

size_t getNearestPowerOf2(size_t number)
{
    size_t n1 = floor(log(number) / log(2.0));
    size_t n2 = ceil(log(number) / log(2.0));
    size_t n = (fabs(number - n1) <= fabs(number - n2)) ? n1 : n2;
    return pow(2, n);
}

MatrixXd FFT_spec(MatrixXd usrData)
{
    size_t nb_x = usrData.rows(); // getNearestLowerPowerOf2(usrData.rows());

    double periods = (usrData(nb_x - 1, 0) - usrData(0, 0));

    Complex test[nb_x];

    for (size_t i = 0; i < nb_x; i++)
    {
        test[i] = Complex(usrData(i, 1), 0);
    }

    CArray data(test, nb_x);

    // forward fft
    fft(data);

    MatrixXd usrData_freq(nb_x / 2, 5);

    for (size_t i = 0; i < nb_x / 2; ++i)
    {
        double freq = i / periods;
        usrData_freq(i, 0) = freq;
        usrData_freq(i, 1) = 2.0 * sqrt(pow(data[i].real(), 2) + pow(data[i].imag(), 2)) / nb_x;
        usrData_freq(i, 2) = data[i].real();
        usrData_freq(i, 3) = data[i].imag();
        double SPL = 20.0 * log10(usrData_freq(i, 1) / 2.0E-5);
        usrData_freq(i, 4) = (SPL < 0.0) ? 0.0 : SPL;
    }
    usrData_freq(0, 4) = usrData_freq(1, 4);

    return usrData_freq;
}

// using FFTW lib
MatrixXd FFTW_spec(MatrixXd usrData)
{
    size_t nb_x = usrData.rows(); // getNearestLowerPowerOf2(usrData.rows());

    double periods = (usrData(nb_x - 1, 0) - usrData(0, 0));

    fftw_complex x[nb_x];
    fftw_complex y[nb_x];

    for (size_t i = 0; i < nb_x; i++)
    {
        x[i][REAL] = usrData(i, 1);
        x[i][IMAG] = 0.0;
    }

    fftw_plan plan = fftw_plan_dft_1d(nb_x, x, y, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);

    fftw_destroy_plan(plan);
    fftw_cleanup();

    MatrixXd usrData_freq(nb_x / 2, 5);

    for (size_t i = 0; i < nb_x / 2; ++i)
    {
        double freq = i / periods;
        usrData_freq(i, 0) = freq;
        usrData_freq(i, 1) = 2.0 * sqrt(pow(y[i][REAL], 2) + pow(y[i][IMAG], 2)) / nb_x;
        usrData_freq(i, 2) = y[i][REAL];
        usrData_freq(i, 3) = y[i][IMAG];
        double SPL = 20.0 * log10(usrData_freq(i, 1) / 2.0E-5);
        usrData_freq(i, 4) = SPL; //(SPL < 0.0) ? 0.0 : SPL;
    }

    return usrData_freq;
}

MatrixXd PSD_spec(MatrixXd usrData)
{
    size_t nb_x = usrData.rows(); // getNearestLowerPowerOf2(usrData.rows());

    double sampleRate = 1. / (usrData(nb_x - 1, 0) - usrData(0, 0));

    fftw_complex x[nb_x];
    fftw_complex y[nb_x];

    for (size_t i = 0; i < nb_x; i++)
    {
        x[i][REAL] = usrData(i, 1);
        x[i][IMAG] = 0.0;
    }

    fftw_plan plan = fftw_plan_dft_1d(nb_x, x, y, FFTW_FORWARD, FFTW_ESTIMATE);
    fftw_execute(plan);

    fftw_destroy_plan(plan);
    fftw_cleanup();

    MatrixXd usrData_freq(nb_x / 2 + 1, 3);

    const double fftwNormalizingFactor = 1. / double(nb_x * nb_x);
    const double deltaTNormalizingFactor = 1. / sampleRate;
    const double normalizingFactor = fftwNormalizingFactor * deltaTNormalizingFactor;
    usrData_freq(0, 0) = 0.0;
    usrData_freq(0, 1) = (pow(y[0][REAL], 2) + pow(y[0][IMAG], 2)) * normalizingFactor;
    usrData_freq(0, 2) = 10.0 * log10(usrData_freq(0, 1) / pow(2.0E-5, 2));
    for (size_t i = 1; i < nb_x / 2+1; ++i)
    {
        double freq = i * sampleRate;
        usrData_freq(i, 0) = freq;
        double PSD = pow(y[i][REAL], 2) + pow(y[i][IMAG], 2);
        usrData_freq(i, 1) = 2.0 * PSD * normalizingFactor;
        usrData_freq(i, 2) = 10.0 * log10(usrData_freq(i, 1) / pow(2.0E-5, 2));
    }
    if (nb_x % 2 == 0)
    {
        usrData_freq(nb_x / 2, 1) *= 0.5;
        usrData_freq(nb_x / 2, 2) = 10.0 * log10(usrData_freq(nb_x / 2, 1) / pow(2.0E-5, 2));
    }

    return usrData_freq;
}

int WriteFFT(MatrixXd usrData, std::string filename)
{
    screen_display::write_string("Write SPL file...", BLUE);
    MatrixXd usrData_freq = FFTW_spec(usrData);

    std::ofstream fout(filename.c_str());

    fout << "Frequency Norm Real Imag SPL" << std::endl;
    for (int i = 1; i < usrData_freq.rows(); i++)
    {
        for (int j = 0; j < usrData_freq.cols(); j++)
        {
            fout << usrData_freq(i, j) << " ";
        }
        fout<<std::endl;
    }

    fout.close();

    return 0;
}

int WritePSD(MatrixXd usrData, std::string filename)
{
    screen_display::write_string("Write PSD file...", BLUE);
    MatrixXd usrData_freq = PSD_spec(usrData);

    std::ofstream fout(filename.c_str());

    fout << "Frequency Norm PSD" << std::endl;
    for (int i = 1; i < usrData_freq.rows(); i++)
    {
        for (int j = 0; j < usrData_freq.cols(); j++)
        {
            fout << usrData_freq(i, j) << " ";
        }
        fout<<std::endl;
    }

    fout.close();

    return 0;
}
