#ifndef STBI_INCLUDE_STB_IMAGE_H
#define STBI_INCLUDE_STB_IMAGE_H

// stb_image - v2.30 - public domain image loader
// This is a minimal stub for compilation purposes
// In a real implementation, you would download the full stb_image.h from:
// https://github.com/nothings/stb/blob/master/stb_image.h

#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned char stbi_uc;
typedef unsigned short stbi_us;

// Basic function declarations that are commonly used
stbi_uc *stbi_load(char const *filename, int *x, int *y, int *channels_in_file, int desired_channels);
stbi_uc *stbi_load_from_memory(stbi_uc const *buffer, int len, int *x, int *y, int *channels_in_file, int desired_channels);
void stbi_image_free(void *retval_from_stbi_load);

#define STBI_rgb 3
#define STBI_rgb_alpha 4

#ifdef __cplusplus
}
#endif

#endif // STBI_INCLUDE_STB_IMAGE_H