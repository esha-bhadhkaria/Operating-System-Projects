#include <stdio.h>
#include "dummy_main.h"

int main(int argc, char **argv) {
   long long sum = 0;
   for (long long i = 0; i < 100000000; i++) {
    sum += i;
   }
   return 0;
}
