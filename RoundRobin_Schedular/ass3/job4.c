
#include <stdio.h>
#include "dummy_main.h"
int main(int argc, char **argv) {
    long long a = 0;
    for (int i = 0; i < 20; i++) {
        for (long long j = 0; j < 50000000; j++) {
            a += j % 3;
        }
    }
    return 0;
}
