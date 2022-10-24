#include <cmath>
#include <stdio.h>
#include <sys/time.h>

extern "C" {
void* time() {
    struct timeval* tv = new timeval;
    gettimeofday(tv, NULL);
    return (void*)tv;
}

static float tdiff(struct timeval* start, struct timeval* end) {
    return (end->tv_sec - start->tv_sec) + 1e-6 * (end->tv_usec - start->tv_usec);
}

void print_time_diff(void* tv1, void* tv2) {
    printf("Run %0.6f \n", tdiff((struct timeval*)tv1, (struct timeval*)tv2));
}

void print_newline() { printf("\n"); }
}
