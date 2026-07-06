#ifndef LIBMB_H
#define LIBMB_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 * Core image structure
 *   uint8_t *data  -- pixel data (row-major)
 *   int width      -- width in pixels
 *   int height     -- height in pixels
 *   int channels   -- 1=gray, 3=RGB, 4=RGBA
 * ================================================================ */
typedef struct {
    uint8_t *data;
    int      width;
    int      height;
    int      channels;
    int      _reserved;  /* offset +0x10, set by CreateImg */
} MbImg;

/* ================================================================
 * stb_image I/O
 * ================================================================ */
const char *stbi_failure_reason(void);
void        stbi_image_free(void *retval_from_stbi_load);
int         stbi_set_flip_vertically_on_load(int flag);

uint8_t    *stbi_load(const char *filename, int *x, int *y,
                      int *channels_in_file, int desired_channels);
uint8_t    *stbi_load_from_file(FILE *f, int *x, int *y,
                                int *channels_in_file, int desired_channels);
uint8_t    *stbi_load_from_memory(const uint8_t *buffer, int len,
                                  int *x, int *y, int *channels_in_file,
                                  int desired_channels);
uint8_t    *stbi_load_16_from_memory(const uint8_t *buffer, int len,
                                     int *x, int *y, int *channels_in_file,
                                     int desired_channels);
uint8_t    *stbi_loadf_from_memory(const uint8_t *buffer, int len,
                                   int *x, int *y, int *channels_in_file,
                                   int desired_channels);
uint8_t    *stbi_load_gif_from_memory(const uint8_t *buffer, int len,
                                      int **delays, int *x, int *y,
                                      int *z, int *comp, int req_comp);

int         stbi_info(const char *filename, int *x, int *y, int *comp);
int         stbi_info_from_memory(const uint8_t *buffer, int len,
                                  int *x, int *y, int *comp);
int         stbi_info_from_file(FILE *f, int *x, int *y, int *comp);
int         stbi_info_from_callbacks(void *clbk, void *user,
                                     int *x, int *y, int *comp);

int         stbi_is_hdr(const char *filename);
int         stbi_is_hdr_from_file(FILE *f);
int         stbi_is_hdr_from_memory(const uint8_t *buffer, int len);
int         stbi_is_16_bit(const char *filename);
int         stbi_is_16_bit_from_file(FILE *f);
int         stbi_is_16_bit_from_memory(const uint8_t *buffer, int len);

void        stbi_set_unpremultiply_on_load(int flag);
void        stbi_convert_iphone_png_to_rgb(int flag);
void        stbi_hdr_to_ldr_gamma(float gamma);
void        stbi_hdr_to_ldr_scale(float scale);
void        stbi_ldr_to_hdr_gamma(float gamma);
void        stbi_ldr_to_hdr_scale(float scale);

int         stbi_write_bmp(const char *filename, int w, int h,
                           int comp, const void *data);
int         stbi_write_png(const char *filename, int w, int h,
                           int comp, const void *data, int stride_bytes);
int         stbi_write_jpg(const char *filename, int w, int h,
                           int comp, const void *data, int quality);
int         stbi_write_tga(const char *filename, int w, int h,
                           int comp, const void *data);
int         stbi_write_hdr(const char *filename, int w, int h,
                           int comp, const float *data);

void        stbi_flip_vertically_on_write(int flag);
void        stbi_write_force_png_filter(int filter);
void        stbi_write_tga_with_rle(int flag);
int         stbi_write_png_compression_level;

int         stbi_zlib_compress(uint8_t *data, int data_len,
                               uint8_t **out, int *out_len, int quality);
uint8_t    *stbi_zlib_decode_malloc_guesssize(const char *buffer, int len,
                                               int initial_size, int *out_len);
uint8_t    *stbi_zlib_decode_malloc(const char *buffer, int len, int *out_len);
uint8_t    *stbi_zlib_decode_malloc_guesssize_headerflag(
    const char *buffer, int len, int initial_size, int *out_len, int flag);
int         stbi_zlib_decode_buffer(char *obuffer, int olen,
                                    const char *ibuffer, int ilen);
uint8_t    *stbi_zlib_decode_noheader_malloc(const char *buffer, int len,
                                              int *out_len);
int         stbi_zlib_decode_noheader_buffer(char *obuffer, int olen,
                                             const char *ibuffer, int ilen);

/* ================================================================
 * Image processing
 * ================================================================ */

/* color space */
int  RGB2Gray(const uint8_t *rgb, int w, int h, int stride, uint8_t *gray);
int  Gray2Color(MbImg *img, int channels);
int  Color2Gray(MbImg *img, int reserved1, int reserved2);
int  Color2RGB(const uint8_t *in, int w, int h, int stride, uint8_t *out);
int  RGB2Color(const uint8_t *in, int w, int h, int stride, uint8_t *out);
int  RGB2HSB(const uint8_t *rgb, int w, int h, float *hsb);

/* binarization */
int  Threshold(const uint8_t *gray, int w, int h, int stride, uint8_t *bw, int thresh);
int  ErrorDiffusion(const uint8_t *gray, int w, int h, int stride, uint8_t *bw);
int  ErrorDiffusionByShort(const short *gray, int w, int h, int stride, uint8_t *bw);

/* enhancement */
void ImgCLAHE(uint8_t *gray, int w, int h, int clip, int grid_size);
void ImgLaplacianSharpen(uint8_t *gray, int w, int h);
void ImgGaussianBlur(const uint8_t *src, int w, int h, int stride,
                     uint8_t *dst, int ksize, float sigma);
void ImgBlur(const uint8_t *src, int w, int h, int stride, uint8_t *dst, int ksize);
void ImgBrightnessContrastNew(uint8_t *gray, int w, int h, int brightness, int contrast);
void ImgBrightnessContrastOld(uint8_t *gray, int w, int h, int brightness, int contrast);
void ImgColorGradation(uint8_t *gray, int w, int h, ...);
void ImgNormalizeMaxMin(uint8_t *gray, int w, int h);
void SetImgSaturation(uint8_t *rgb, int w, int h, int saturation);

/* geometry */
void ImgResize  (const uint8_t *src, int sw, int sh,
                 uint8_t *dst, int dw, int dh, int channels);
void ImgRotate  (const uint8_t *src, int w, int h, int stride,
                 uint8_t *dst, float angle, int cx, int cy);
void ImgRotateFull(const uint8_t *src, int w, int h, int channels,
                   uint8_t *dst, float angle);
void ImgFlip    (uint8_t *img, int w, int h, int channels, int dir);

/* morphological */
void ImgDilate(uint8_t *img, int w, int h);
void ImgErode (uint8_t *img, int w, int h);
void ImgDivide(const uint8_t *a, const uint8_t *b, int w, int h, uint8_t *out);
void ImgMin   (const uint8_t *a, const uint8_t *b, int w, int h, uint8_t *out);
void ImgMax   (const uint8_t *a, const uint8_t *b, int w, int h, uint8_t *out);

/* connected components */
int  FindImgConComs(const uint8_t *bw, int w, int h, ...);
void FreeImgConComs(void *coms);
void RemoveConComFromImg(void);

/* filters (artistic) */
int  mbImgFilter       (uint8_t *rgb, int w, int h, int filter_id, int param);
int  mbKatongFilter    (uint8_t *rgb, int w, int h);
int  mbBlackWhiteFilter(uint8_t *rgb, int w, int h);
int  mbCrosshatchFilter(uint8_t *rgb, int w, int h);
int  mbSketchFilter    (uint8_t *rgb, int w, int h);
int  mbLineDraftFilter (uint8_t *rgb, int w, int h);
int  mbHalftoneFilter  (uint8_t *rgb, int w, int h);
int  mbToonFilter      (uint8_t *rgb, int w, int h);
int  mbWaterColorFilter(uint8_t *rgb, int w, int h);

/* MbImg API */
MbImg *CreateImg(int w, int h, int channels);
void   FreeImg(MbImg *img);
MbImg *ImgStructClone(const MbImg *src);
void   ImgReverse(MbImg *img);
int    IsImgEqual(const MbImg *a, const MbImg *b);
void   ImgSwop(MbImg *a, MbImg *b);
void   SwapImg(MbImg *a, MbImg *b);
void   GetImgMinMax(const MbImg *img, int *min, int *max);
void   FreeImgConComs(void *data);
void   FreeSendDataBag(void *data);

/* grayscale conversion (returns malloc'd data or writes to file) */
/* mode: 0/1 = already gray, 2 = RGB input. gray_levels: 8 = binary.
 * Writes packed result in-place to data, out_len receives size. Returns void. */
void mbImg2GrayscaleData(uint8_t *data, unsigned int w, unsigned int h,
                         unsigned int mode, int gray_levels, int *out_len);
int  mbImg2GrayscaleData_File(const char *filename, const char *outfile, int method);
int  mbImgBin_File(const char *filename, const char *outfile);
int  mbGrayscaleData2Img(uint8_t *gray, int w, int h, uint8_t *out);
int  mbImg2GrayscaleData_text(const uint8_t *rgb, int w, int h, int channels,
                              int method, int *out_len);
int  mbGetWriteRows(void);
void mbWriteGray(void);

/* scanner / document image pre-processing */
int  mbImgScan(const uint8_t *rgb, int w, int h, int channels,
               uint8_t **out, int *out_len);
int  GetSuctionSidePointByLsd(const uint8_t *gray, int w, int h,
                              float *points, int max_pts);
int  GetSuctionSidePointByLsdForBook(const uint8_t *gray, int w, int h,
                                     float *points, int max_pts);
int  GetPerspectiveImg(const uint8_t *src, int w, int h,
                       const float *pts, uint8_t *dst);
int  GetLines(void);
int  LineFit(void);
int  RegionGrow(void);
int  IsAligned(int a, int b);

/* YUV */
void mbYUV420spToRGB888(const uint8_t *yuv, int w, int h, uint8_t *rgb);
void mbRGB888ToYUV420sp(const uint8_t *rgb, int w, int h, uint8_t *yuv);

/* ================================================================
 * MMJ Printer API (thermal printer data generation)
 * ================================================================ */

/* Main entry: in-place dither. Modifies img->data to a printer-ready image.
 * img          - [in/out] MbImg with RGB data, dithered result written back
 * dither_data  - optional dither matrix (NULL = default)
 * dither_size  - matrix size in bytes, must be 4-aligned (0 = use default)
 * mode         - 0=default dither, 1=alternative
 * Returns: 0 on success, non-zero on error
 */
int MMJ_PrinterImgBin(MbImg *img, const void *dither_data,
                      unsigned int dither_size, int mode);

uint8_t *MMJ_PrinterImgBinData(MbImg *img, void *extra, unsigned int extra_size,
                               int mode, unsigned int width, size_t *out_len);

uint8_t *MMJ_PrinterImgBinA4(const uint8_t *rgb, int w, int h, int channels,
                             uint8_t **out_data, int *out_len);

uint8_t *MMJ_GrayScalePrinterByErrorDiffusion(
    MbImg *gray, int gray_levels,
    uint8_t **out_data, int *out_len);

uint8_t *MMJ_BinaryImgToPrinterData(
    const uint8_t *bw, int w, int h,
    uint8_t **out_data, int *out_len);

uint8_t *MMJ_BinImg2HfmData(
    const uint8_t *bw, int w, int h,
    uint8_t **out_data, int *out_len);

uint8_t *MMJ_HfmData2ImgData(
    const uint8_t *hfm, int len,
    uint8_t **out_data, int *out_len);

uint8_t *MMJ_HfmData2ImgData_more(
    const uint8_t *hfm, int len,
    uint8_t **out_data, int *out_len);

uint8_t *MMJ_Img2BinHfmData(
    const uint8_t *rgb, int w, int h, int channels,
    uint8_t **out_data, int *out_len, int mode);

int  MMJ_Threshold(MbImg *img, int thresh, int flags);
int  MMJ_ErrorDiffusionByShort(const short *gray, int w, int h, uint8_t *bw);
int  MMJ_Color2Gray(MbImg *img, int reserved1, int reserved2);
int  MMJ_ImgResize(const uint8_t *src, int sw, int sh,
                   uint8_t *dst, int dw, int dh);

void MMJ_SetIDBrightness(MbImg *img);
void MMJ_SetScanImgAdjust(int contrast, int brightness, int details);

void MMJ_SetIDBrightness(MbImg *img);
void *MMJ_GetGrayScaleDataAndImg(const uint8_t *rgb, int w, int h, int channels,
                                 int *out_len);

uint8_t *MMJ_BinImgDataByte2Bit(const uint8_t *byte_data,
                                int w, int h, int *out_len);
uint8_t *MMJ_BitData2PrinterData(const uint8_t *bit_data,
                                 int w, int h, int *out_len);

/* printer data buffer API (for building print jobs incrementally) */
void mbSetPrintStart(int width, int height, int dpi);
void mbSetPrintData(const uint8_t *data, int len);
void mbSetPrintEnd(void);
void mbSetPrintHfmTree(const uint8_t *tree, int len);
int  mbGetPrintStart(void);
int  mbGetPrintEnd(void);
void *mbGetPrintHfmTree(int *out_len);
void *mbGetPrintData(int *out_len);
int  mbPrintProAnalysis(const uint8_t *data, int len);

/* text-to-image for printing */
int  GetFontSize(const char *text, int font_id, int *w, int *h);
int  IsToPrintByFont(const char *text, int font_id);
int  ImageText2BW(const char *text, int font_id, int max_w, int max_h,
                  uint8_t **out, int *w, int *h);
int  MMJ_ImageText2BW(const char *text, const char *font_path,
                      int max_w, int max_h, uint8_t **out, int *w, int *h);
int  MMJ_A4ImageText2BW(const char *text, const char *font_path,
                        int max_w, int max_h, uint8_t **out, int *w, int *h);
void FreeTextImgs(void);

/* red/blue text handling */
int  IsRed(const uint8_t *rgb);
int  IsBlue(const uint8_t *rgb);
int  IsCol(const uint8_t *rgb);
void TextDelRedBlue(uint8_t *rgb, int w, int h, int stride);
void TextBinary(uint8_t *gray, int w, int h, int stride, ...);
void MMJ_TextBinary(void);
void MMJ_TextAdjust(void);
void MMJ_TextEraserMerge(void);

/* ================================================================
 * Audio watermark encode
 * ================================================================ */
int  WM_DataEncode(const uint8_t *data, int len, uint8_t *out);
int  MMJ_WM_DataEncode(const uint8_t *data, int len, uint8_t *out);

/* ================================================================
 * Huffman encoding / decoding
 * ================================================================ */
void *GetHfmTree(const uint8_t *data, int len);
void  MakeHfmTree(void *nodes, int count, uint8_t *tree_out, int *tree_len);
void  MakeTree(void *nodes, int count);
void  MakeTreeNew(void *nodes, int count);
void  MakeHfmTreeNew(void *nodes, int count, ...);

int   HFMencoding(const uint8_t *in, int in_len, uint8_t *out, int out_max);
int   HFMencoding_mem(const uint8_t *in, int in_len, uint8_t *out, int out_max);
int   HFMdecoding(const uint8_t *in, int in_len, uint8_t *out, int out_max);
int   HFMdecoding_mem(const uint8_t *in, int in_len, uint8_t *out, int out_max);
int   HFMdecodingNew(const uint8_t *in, int in_len, uint8_t *out, int out_max);
int   HFMDGencoding(const uint8_t *in, int in_len, uint8_t *out, int out_max);

/* tree traversal */
void *TreePreorder(void *root, int *out_len);
void *GetTreePreorder(void *root, int *out_len);
void *TreeMiddleorder(void *root, int *out_len);
void *GetTreeMiddleorder(void *root, int *out_len);
void *GetPreMidTree(uint8_t *pre, int pre_len, uint8_t *mid, int mid_len);
int   GetMinTreeNode(void);

/* printer matrix data */
int  ylImgData2PrinterData(void);
int  ylGetInkJetPrinterData(void);
int  sn_to_out4(const uint8_t *in, int len, uint8_t *out);

/* ================================================================
 * Image scanning / perspective correction
 * ================================================================ */
int  MMJ_GetSuctionSidePointByLsd(const uint8_t *gray, int w, int h,
                                  float *points, int max_pts);
int  MMJ_GetSuctionSidePointByLsdForBook(const uint8_t *gray, int w, int h,
                                         float *points, int max_pts);
int  MMJ_GetPerspectiveImg(const uint8_t *src, int w, int h,
                           const float *pts, uint8_t *dst);
int  MMJ_GetScanFilterImg(const uint8_t *rgb, int w, int h, int channels,
                          uint8_t **out, int *out_len);
void FreeMMJImg(void *img);

/* ================================================================
 * Misc
 * ================================================================ */
void *GetBrightenUpImg(void);
void *RGBImgBlur(void);
void *RGBImgStdDev(void);
void *GetEnhanceSharpenImg(void);
void *GetGrayModelImg(void);
void *GetBlackWhiteModelImg(void);
void *GetSaveInkModelImg(void);
void *GetPicKaTong(void);
void *GetPicWhiteBackground(void);
void *GetPicLineDraft(void);
int   GetFitHeightForTextImg(void);
int   GetFitHeightImgForTextImg(void);
int   GetFitWidthImgForTextImg(void);
int   GetWidthByFontSize(void);
void  ImgBlackWhitePs(void);
void  ImgNoise(void);
void  ImgCurveData(void);
int   GetAngleAndGradient(void);
void  SetMat8_9(void);
void  SetMat9_10(void);
uint8_t GetPixel(const uint8_t *img, int w, int h, int channels, int x, int y);
int  GetPixelByRGB565(uint16_t rgb565);
void PointRotate(int *x, int *y, float angle, int cx, int cy);

#ifdef __cplusplus
}
#endif

#endif /* LIBMB_H */
