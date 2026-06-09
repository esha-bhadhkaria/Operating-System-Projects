
#include <stdio.h>
#include "dummy_main.h"
int main(int argc, char **argv) {
    volatile long long dummy = 0;
    for (int i = 0; i < 10; i++) {
        for (long long j = 0; j < 100000000; j++) {
            dummy++;
        }
    }
    return 0;
}
