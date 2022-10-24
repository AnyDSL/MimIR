inline void swap(double* a, double* b) {
    double temp = *a;
    *a          = *b;
    *b          = temp;
}

static void recursiveApply(double* data, int iSign, unsigned N) {
    if (N == 1) return;
    recursiveApply(data, iSign, N / 2);
    recursiveApply(data + N, iSign, N / 2);

    double wtemp = iSign * sin(M_PI / N);
    double wpi   = -iSign * sin(2 * M_PI / N);
    double wpr   = -2.0 * wtemp * wtemp;
    double wr    = 1.0;
    double wi    = 0.0;

    for (unsigned i = 0; i < N; i += 2) {
        int iN = i + N;

        double tempr = data[iN] * wr - data[iN + 1] * wi;
        double tempi = data[iN] * wi + data[iN + 1] * wr;

        data[iN]     = data[i] - tempr;
        data[iN + 1] = data[i + 1] - tempi;
        data[i] += tempr;
        data[i + 1] += tempi;

        wtemp = wr;
        wr += wr * wpr - wi * wpi;
        wi += wi * wpr + wtemp * wpi;
    }
}

static void scramble(double* data, unsigned N) {
    int j = 1;
    for (int i = 1; i < 2 * N; i += 2) {
        if (j > i) {
            swap(&data[j - 1], &data[i - 1]);
            swap(&data[j], &data[i]);
        }
        int m = N;
        while (m >= 2 && j > m) {
            j -= m;
            m >>= 1;
        }
        j += m;
    }
}

static void rescale(double* data, unsigned N) {
    double scale = ((double)1) / N;
    for (unsigned i = 0; i < 2 * N; i++) { data[i] *= scale; }
}

static void fft(double* data, unsigned N) {
    scramble(data, N);
    recursiveApply(data, 1, N);
}

static void ifft(double* data, unsigned N) {
    scramble(data, N);
    recursiveApply(data, -1, N);
    rescale(data, N);
}

void foobar(double* data, unsigned len) {
    fft(data, len);
    ifft(data, len);
}

extern "C" {
int enzyme_dupnoneed;
}

static double foobar_and_gradient(unsigned len) {
    double* inp = new double[2 * len];
    for (int i = 0; i < 2 * len; i++) inp[i] = 2.0;
    double* dinp = new double[2 * len];
    for (int i = 0; i < 2 * len; i++) dinp[i] = 1.0;
    __enzyme_autodiff<void>(foobar, enzyme_dupnoneed, inp, dinp, len);
    double res = dinp[0];
    delete[] dinp;
    delete[] inp;
    return res;
}

static void enzyme_sincos(double inp, unsigned len) {
    {
        struct timeval start, end;
        gettimeofday(&start, NULL);

        double res2 = foobar_and_gradient(len);

        gettimeofday(&end, NULL);
        printf("Enzyme combined %0.6f res'=%f\n", tdiff(&start, &end), res2);
    }
}

double inp = -2.1;

for (unsigned iters = max(1, N >> 5); iters <= N; iters *= 2) { enzyme_sincos(inp, iters); }
