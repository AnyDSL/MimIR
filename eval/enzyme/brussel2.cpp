#include <algorithm>
#include <array>
#include <iostream>

#include <cmath>
#include <sys/time.h>

const int N    = 32;
const int xmin = 0.0;
const int xmax = 1.0;
const int ymin = 0.0;
const int ymax = 1.0;

double RANGE(double min, double max, double i, double N) { return ((max - min) / (N - 1.0) * i + min); }

void init_brusselator(double* u, double* v) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double x = RANGE(xmin, xmax, i, N);
            double y = RANGE(ymin, ymax, j, N);

            u[N * i + j] = 22 * (y * (1 - y)) * sqrt(y * (1 - y));
            v[N * i + j] = 27 * (x * (1 - x)) * sqrt(x * (1 - x));
        }
    }
}

double brusselator_f(double x, double y, double t) {
    bool eq1 = ((x - 0.3) * (x - 0.3) + (y - 0.6) * (y - 0.6)) <= 0.1 * 0.1;
    bool eq2 = t >= 1.1;
    if (eq1 && eq2) {
        return 5.0;
    } else {
        return 0.0;
    }
}

void brusselator_2d_loop(double* du, double* dv, const double* u, const double* v, const double* p, double t) {
    double A     = p[0];
    double B     = p[1];
    double alpha = p[2];
    double dx    = (double)1 / (N - 1);

    alpha = alpha / (dx * dx);

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double x = RANGE(xmin, xmax, i, N);
            double y = RANGE(ymin, ymax, j, N);

            unsigned ip1 = (i == N - 1) ? i : (i + 1);
            unsigned im1 = (i == 0) ? i : (i - 1);

            unsigned jp1 = (j == N - 1) ? j : (j + 1);
            unsigned jm1 = (j == 0) ? j : (j - 1);

            double u2v = u[N * i + j] * u[N * i + j] * v[N * i + j];

            (du)[N * i + j] = alpha * ((u)[N * im1 + j] + (u)[N * ip1 + j] + (u)[N * i + jp1] + (u)[N * i + jm1] -
                                       4 * (u)[N * i + j]) +
                              B + u2v - (A + 1) * (u)[N * i + j] + brusselator_f(x, y, t);

            (dv)[N * i + j] = alpha * ((v)[N * im1 + j] + (v)[N * ip1 + j] + (v)[N * i + jp1] + (v)[N * i + jm1] -
                                       4 * (v)[N * i + j]) +
                              A * (u)[N * i + j] - u2v;
        }
    }
}

double foobar_exec(const double* p,
                   const std::array<double, 2 * N * N> x,
                   const std::array<double, 2 * N * N> adjoint,
                   double t) {
    double dp[3] = {0.};

    // std::array<double, 2 * N* N> dx = {0.};

    std::array<double, 2 * N* N> dadjoint_inp = adjoint;

    // state_type dxdu;

    brusselator_2d_loop(dadjoint_inp.data(),         // du
                        dadjoint_inp.data() + N * N, // dv
                        x.data(),                    // u
                        x.data() + N * N,            // v
                        p,                           // p
                        t                            // t
    );

    // __enzyme_autodiff<void>(
    // brusselator_2d_loop,
    // //                            enzyme_dup, dxdu.c_array(), dadjoint_inp.c_array(),
    // //                            enzyme_dup, dxdu.c_array() + N * N, dadjoint_inp.c_array() + N * N,
    // enzyme_dupnoneed, nullptr, dadjoint_inp.data(), enzyme_dupnoneed, nullptr, dadjoint_inp.data() + N * N,
    // enzyme_dup, x.data(), dx.data(), enzyme_dup, x.data() + N * N, dx.data() + N * N, enzyme_dup, p, dp,
    // enzyme_const, t);

    return dadjoint_inp[0];
}

static float tdiff(struct timeval* start, struct timeval* end) {
    return (end->tv_sec - start->tv_sec) + 1e-6 * (end->tv_usec - start->tv_usec);
}

int main() {
    const double p[3] = {/*A*/ 3.4, /*B*/ 1, /*alpha*/ 10.};

    std::array<double, 2 * N * N> x;
    init_brusselator(x.data(), x.data() + N * N);

    std::array<double, 2 * N * N> adjoint;
    init_brusselator(adjoint.data(), adjoint.data() + N * N);

    double t = 2.1;
    {
        struct timeval start, end;
        gettimeofday(&start, NULL);

        double res;
        for (int i = 0; i < 10000; i++) res = foobar_exec(p, x, adjoint, t);

        gettimeofday(&end, NULL);
        printf("Run %0.6f res=%f\n", tdiff(&start, &end), res);
    }

    // std::cout << "\u221A 2 = " << sqrt(2.0) << std::endl;
    return 0;
}

// .cn printInteger [mem: %mem.M, val: i32, return : .Cn [%mem.M]];
// .cn printIntegerNL [mem: %mem.M, val: i32, return : .Cn [%mem.M]];
// .cn printNL [mem: %mem.M, return : .Cn [%mem.M]];
