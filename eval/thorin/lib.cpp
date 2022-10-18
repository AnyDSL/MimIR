// #include <alloca.h>
#include <cmath>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
// #include <setjmp.h>

extern "C" {

// void* malloc(size_t size);
// void* alloc(size_t size) { return alloca(size); }
// void* alloc(size_t size) { return malloc(size); }

// void test() {
//     void* a = malloc(1);
//     void* b = alloca(1);
// }

void* time() {
    struct timeval* tv = new timeval;
    gettimeofday(tv, NULL);
    return (void*)tv;
}

static float tdiff(struct timeval* start, struct timeval* end) {
    return (end->tv_sec - start->tv_sec) + 1e-6 * (end->tv_usec - start->tv_usec);
}

// void printInteger(int i) { printf("%d, ", i); }
// void printIntegerNL(int i) { printf("%d\n", i); }
// void printNL() { printf("\n"); }
void print_time_diff(void* tv1, void* tv2) {
    printf("real\t%0.6f \n", tdiff((struct timeval*)tv1, (struct timeval*)tv2));
}

void printInteger(int i) {}
void printIntegerNL(int i) {}
void printNL() {}
// void print_time_diff(void* tv1, void* tv2) { printf("%0.6f", tdiff((struct timeval*)tv1, (struct timeval*)tv2)); }

// long jmpbuf_size(){
//     return _JBLEN; // wird für sjlj gebraucht
// }
}
