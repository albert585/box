#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "libMb.h"

static void _log(const char *msg) { fprintf(stderr, "%s\n", msg); fflush(stderr); }

int main(int argc, char **argv) {
    int w, h, n;
    if (argc < 2) { fprintf(stderr, "Usage: %s <image>\n", argv[0]); return 1; }

    _log("stbi_load...");
    uint8_t *rgb = stbi_load(argv[1], &w, &h, &n, 3);
    if (!rgb) { _log(stbi_failure_reason()); return 1; }
    _log("loaded");

    /* 1. in-place dither */
    {
        _log("dither...");
        uint8_t *cpy = malloc(w * h * 3);
        memcpy(cpy, rgb, w * h * 3);
        MbImg img = { .data = cpy, .width = w, .height = h, .channels = 3, ._reserved = 0 };
        int r = MMJ_PrinterImgBin(&img, NULL, 0, 0);
        fprintf(stderr, "dither: %d\n", r); fflush(stderr);
        if (r == 0) { stbi_write_bmp("dithered.bmp", w, h, 3, img.data); _log("-> dithered.bmp"); }
        free(cpy);
    }

    /* 2. grayscale packed data (in-place on copy) */
    {
        _log("gray...");
        uint8_t *cpy = malloc(w * h * 3);
        memcpy(cpy, rgb, w * h * 3);
        int glen = 0;
        mbImg2GrayscaleData(cpy, w, h, 2, 8, &glen);
        fprintf(stderr, "gray: %d bytes\n", glen); fflush(stderr);
        if (glen > 0) {
            FILE *f = fopen("out.gray", "wb");
            if (f) { fwrite(cpy, 1, glen, f); fclose(f); _log("-> out.gray"); }
        }
        free(cpy);
    }

    /* 3. CLAHE */
    {
        _log("clahe...");
        MbImg *img = CreateImg(w, h, 3);
        if (img) {
            memcpy(img->data, rgb, w * h * 3);
            Color2Gray(img, 0, 0);
            ImgCLAHE(img->data, w, h, 40, 8);
            Gray2Color(img, 3);
            stbi_write_bmp("clahe.bmp", w, h, 3, img->data);
            _log("clahe: -> clahe.bmp");
            free(img->data);
            free(img);
            _log("clahe: freed");
        }
    }

    _log("free rgb...");
    stbi_image_free(rgb);
    _log("done");
    return 0;
}
