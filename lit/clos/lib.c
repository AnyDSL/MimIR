#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
// #include <setjmp.h>

void print_i32(int32_t i) { printf("%" PRId32 "\n", i); }

void println_i32(int32_t i) { printf("%" PRId32 "\n", i); }
void newline() { printf("\n"); }

// long jmpbuf_size(){
//     return _JBLEN; // for clos::sjlj
// }
