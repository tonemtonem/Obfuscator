#include <stdio.h>
#include <string.h>

int compute(int a, int b) {
    int x = a + b;
    int y = a - b;
    int z = x * 3;
    int w = (a ^ b) | (x & y);
    return x + y + z + w;
}

int main(void) {
    const char *msg = "hello, obfuscated world!";
    const char *tag = "SECRET_TOKEN_12345";
    printf("%s\n", msg);
    printf("%s\n", tag);
    printf("compute(7,3) = %d\n", compute(7, 3));
    printf("strlen(msg) = %zu\n", strlen(msg));
    return 0;
}