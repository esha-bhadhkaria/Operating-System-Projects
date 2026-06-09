
#include <stdio.h>
#include "dummy_main.h"
int main(int argc, char **argv) {
    long long product = 1;
    for (long long i = 1; i <= 500000000; i++) {
        product *= i;
        if (product > 1000000000) product = 1;
    }
    return 0;
}
