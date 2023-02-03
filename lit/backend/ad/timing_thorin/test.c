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

struct int_triple f_diffed_reshape(int a, int b);

struct int_pair {
  int a;
  int b;
};

int main() {
  struct int_pair *tests = (struct int_pair *)malloc(100000*sizeof(*tests));
  for (int i = 0; i < 100000; i++) {
    tests[i].a = (i*37+12)%10000;
    tests[i].b = i % 100;
  }

  // struct int_triple res = f_diffed_reshape(42, 2);
  // printf("%d %d %d \n", res.a, res.b, res.c);

  void* start = time();
  for (int i = 0; i < 100000; i++) {
    struct int_triple res = f_diffed_reshape(tests[i].a, tests[i].b);
  }
  void* end = time();

  print_time_diff(start, end);

  return 0;
}
