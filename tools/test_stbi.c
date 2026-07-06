#include <stdio.h>
#include "libMb.h"

int main(int argc, char **argv) {
    if (argc < 2) { printf("usage\n"); return 1; }
    fprintf(stderr, "START\n"); fflush(stderr);
    int w, h, n;
    uint8_t *d = stbi_load(argv[1], &w, &h, &n, 3);
    fprintf(stderr, "LOADED: %p %dx%d ch=%d\n", (void*)d, w, h, n); fflush(stderr);
    if (d) { stbi_image_free(d); fprintf(stderr, "FREED\n"); fflush(stderr); }
    else     { fprintf(stderr, "FAIL: %s\n", stbi_failure_reason()); fflush(stderr); }
    fprintf(stderr, "DONE\n"); fflush(stderr);
    return 0;
}
