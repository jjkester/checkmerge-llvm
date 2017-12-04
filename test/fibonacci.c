/*
 * Test program for CheckMerge.
 */
#include <stdio.h>
#include "fibonacci.h"

int fibonacci(int n, int numbers[]) {
    for (int i; i < n; i++) {
        if (i == 0) {
            numbers[i] = 0;
        } else if (i == 1) {
            numbers[i] = 1;
        } else {
            numbers[i] = numbers[i-2] + numbers[i-1];
        }
    }

    return numbers[n - 1];
}

int main() {
    int n = 10;
    int numbers[10] = {0};
    int fib = fibonacci(n, numbers);
    printf("fibonacci %d = %d\n", n, fib);
}
