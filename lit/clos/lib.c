#include <bits/types/time_t.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <time.h>
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

void* get_time() {
    clock_t* tv = (clock_t*)malloc(sizeof(*tv));
    *tv = clock();
    return (void*)tv;
}

static float tdiff(clock_t* start, clock_t* end) {
    return (float)(*end - *start) / CLOCKS_PER_SEC * 1000;
}

void print_time_diff(void* tv1, void* tv2) {
    printf("sys\t%0.3fms \n", tdiff((clock_t*)tv1, (clock_t*)tv2));
}

// int main() {
//     // test print_time_diff
//     void* tv1 = get_time();
//     // a long computation
//     for (int i = 0; i < 300000; i++) {
//         int j = i * i;
//         printf("%d\n", j);
//     }
//     void* tv2 = get_time();
//     print_time_diff(tv1, tv2);
// }
