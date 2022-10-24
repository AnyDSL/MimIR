#define N    32
#define xmin 0.
#define xmax 1.
#define ymin 0.
#define ymax 1.

#define RANGE(min, max, i, N) ((max - min) / (N - 1) * i + min)
#define GETnb(x, i, j)        (x)[N * i + j]
#define GET(x, i, j)          GETnb(x, i, j)
//#define GET(x, i, j) ({ assert(i >=0); assert( j>=0); assert(j<N); assert(j<N); GETnb(x, i, j); })

template<typename T>
T brusselator_f(T x, T y, T t) {
    bool eq1 = ((x - 0.3) * (x - 0.3) + (y - 0.6) * (y - 0.6)) <= 0.1 * 0.1;
    bool eq2 = t >= 1.1;
    if (eq1 && eq2) {
        return T(5);
    } else {
        return T(0);
    }
}

void init_brusselator(double* __restrict u, double* __restrict v) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double x = RANGE(xmin, xmax, i, N);
            double y = RANGE(ymin, ymax, j, N);

            GETnb(u, i, j) = 22 * (y * (1 - y)) * sqrt(y * (1 - y));
            GETnb(v, i, j) = 27 * (x * (1 - x)) * sqrt(x * (1 - x));
        }
    }
}

void brusselator_2d_loop(double* __restrict du,
                         double* __restrict dv,
                         const double* __restrict u,
                         const double* __restrict v,
                         const double* __restrict p,
                         double t) {
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

            double u2v = GET(u, i, j) * GET(u, i, j) * GET(v, i, j);

            GETnb(du, i, j) =
                alpha * (GET(u, im1, j) + GET(u, ip1, j) + GET(u, i, jp1) + GET(u, i, jm1) - 4 * GET(u, i, j)) + B +
                u2v - (A + 1) * GET(u, i, j) + brusselator_f(x, y, t);

            GETnb(dv, i, j) =
                alpha * (GET(v, im1, j) + GET(v, ip1, j) + GET(v, i, jp1) + GET(v, i, jm1) - 4 * GET(v, i, j)) +
                A * GET(u, i, j) - u2v;
        }
    }
}

typedef boost::array<double, 2 * N * N> state_type;

double foobar(const double* p, const state_type x, const state_type adjoint, double t) {
    double dp[3] = {0.};

    state_type dx = {0.};

    state_type dadjoint_inp = adjoint;

    state_type dxdu;

    __enzyme_autodiff<void>(
        brusselator_2d_loop,
        //                            enzyme_dup, dxdu.c_array(), dadjoint_inp.c_array(),
        //                            enzyme_dup, dxdu.c_array() + N * N, dadjoint_inp.c_array() + N * N,
        enzyme_dupnoneed, nullptr, dadjoint_inp.data(), enzyme_dupnoneed, nullptr, dadjoint_inp.data() + N * N,
        enzyme_dup, x.data(), dx.data(), enzyme_dup, x.data() + N * N, dx.data() + N * N, enzyme_dup, p, dp,
        enzyme_const, t);

    return dx[0];
}

const double p[3] = {/*A*/ 3.4, /*B*/ 1, /*alpha*/ 10.};

state_type x;
init_brusselator(x.data(), x.data() + N * N);

state_type adjoint;
init_brusselator(adjoint.data(), adjoint.data() + N * N);

double t = 2.1;

{
    struct timeval start, end;
    gettimeofday(&start, NULL);

    double res;
    for (int i = 0; i < 10000; i++) res = foobar(p, x, adjoint, t);

    gettimeofday(&end, NULL);
    printf("Enzyme combined %0.6f res=%f\n", tdiff(&start, &end), res);
}
