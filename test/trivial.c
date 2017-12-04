#include <stdio.h>

int main(int argc, char *argv []) {
    for (int i; i < argc; i++) {
        printf("%s", argv[i]);
        if (i < argc - 1) {
            printf(" ");
        }
    }
    return 0;
}