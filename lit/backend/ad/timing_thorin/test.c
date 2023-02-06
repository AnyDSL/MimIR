#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>

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





struct int_triple {
  int a;
  int b;
  int c;
};

struct int_pair {
  int a;
  int b;
};

struct ret_tuple {
  int a;
  struct int_pair b;
};

struct int_triple f_diffed_reshape(int a, int b);
struct ret_tuple pow_full_pe_reshape(int a, int b);
struct int_triple pow_pe_reshape(int a, int b);
struct int_triple pow_thorin_reshape(int a, int b);

int pow_manual(int a, int b) {
  if (b == 0) {
    return 1;
  }
  return a*pow_manual(a, b-1);
}

struct ret_tuple pow_c_full_pe(int a, int b) {
  return (struct ret_tuple){pow_manual(a, b), 
    {b*pow_manual(a, b-1), 0}};
}







int main() {
  struct int_pair *tests = (struct int_pair *)malloc(100000*sizeof(*tests));
  for (int i = 0; i < 100000; i++) {
    tests[i].a = (i*37+12)%10000;
    tests[i].b = (i % 100)+1;
  }

  // struct int_triple res = f_diffed_reshape(42, 2);
  // printf("%d %d %d \n", res.a, res.b, res.c);

  void* start = time();
  for (int i = 0; i < 100000; i++) {
    struct int_triple res = f_diffed_reshape(tests[i].a, tests[i].b);
  }
  void* end = time();
  printf("%10s", "Opt: ");
  print_time_diff(start, end);

  start = time();
  for (int i = 0; i < 100000; i++) {
    struct int_triple res = pow_thorin_reshape(tests[i].a, tests[i].b);
  }
  end = time();
  printf("%10s", "Manual: ");
  print_time_diff(start, end);

  start = time();
  for (int i = 0; i < 100000; i++) {
    struct int_triple res = pow_pe_reshape(tests[i].a, tests[i].b);
  }
  end = time();
  printf("%10s", "PE: ");
  print_time_diff(start, end);

  start = time();
  for (int i = 0; i < 100000; i++) {
    struct ret_tuple res = pow_full_pe_reshape(tests[i].a, tests[i].b);
  }
  end = time();
  printf("%10s", "FullPE: ");
  print_time_diff(start, end);


  // C versions


  start = time();
  for (int i = 0; i < 100000; i++) {
    struct ret_tuple res = pow_c_full_pe(tests[i].a, tests[i].b);
  }
  end = time();
  printf("%10s", "C FullPE: ");
  print_time_diff(start, end);

  return 0;
}
