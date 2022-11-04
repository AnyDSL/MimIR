#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
// #include <setjmp.h>

void print_i32(int32_t i) { printf("%" PRId32 "\n", i); }

void println_i32(int32_t i) { printf("%" PRId32 "\n", i); }
void newline() { printf("\n"); }

// void printInteger(int i) { printf("%d, ", i); }
// void printIntegerNL(int i) { printf("%d\n", i); }
// void printNL() { printf("\n"); }
void printInteger(int i) {}
void printIntegerNL(int i) {}
void printNL() {}

// long jmpbuf_size(){
//     return _JBLEN; // for clos::sjlj
// }

void* time() {
    struct timeval* tv = (struct timeval*)malloc(sizeof(*tv));
    gettimeofday(tv, NULL);
    return (void*)tv;
}

static float tdiff(struct timeval* start, struct timeval* end) {
    return (end->tv_sec - start->tv_sec) + 1e-6 * (end->tv_usec - start->tv_usec);
}

void print_time_diff(void* tv1, void* tv2) {
    printf("real\t%0.6f \n", tdiff((struct timeval*)tv1, (struct timeval*)tv2));
}
