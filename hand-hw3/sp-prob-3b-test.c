#include<stdio.h>

int main() {
    // test start...
    puts("Printing message to stdout\n");

    char x[9];
    scanf("%8s", x);
    fprintf(stdout, "Receiving message from stdout: %s\n", x);
    fprintf(stderr, "Receiving message from stderr: %s\n", x);
    // test end...
}
