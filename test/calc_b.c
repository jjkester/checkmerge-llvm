#include <stdio.h>

int calc(int a, int b) {
    int c = a + b;
    printf("c=%d\n", c);
    return c - 1;
}

int main() {
    return calc(1,1);
}