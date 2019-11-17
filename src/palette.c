/*
 * Copyright 2017-2019 Matt "MateoConLechuga" Waltz
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "palette.h"
#include "convert.h"
#include "strings.h"
#include "image.h"
#include "log.h"

#include "deps/libimagequant/libimagequant.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <glob.h>

/*
 * Builtin palettes.
 */
static uint8_t palette_xlibc[];
static uint8_t palette_rgb332[];

/*
 * Allocates palette structure.
 */
palette_t *palette_alloc(void)
{
    palette_t *palette = NULL;

    palette = malloc(sizeof(palette_t));
    if (palette == NULL)
    {
        return NULL;
    }

    palette->images = NULL;
    palette->numImages = 0;
    palette->maxEntries = PALETTE_MAX_ENTRIES;
    palette->numEntries = 0;
    palette->numFixedEntries = 0;
    palette->bpp = BPP_8;
    palette->mode = COLOR_MODE_1555_GBGR;
    palette->quantizeSpeed = PALETTE_DEFAULT_QUANTIZE_SPEED;
    palette->automatic = false;

    return palette;
}

/*
 * Adds a image file to a palette (does not load).
 */
static int palette_add_image(palette_t *palette, const char *path)
{
    image_t *image;

    if (palette == NULL || path == NULL)
    {
        return 1;
    }

    palette->images =
        realloc(palette->images, (palette->numImages + 1) * sizeof(image_t));
    if (palette->images == NULL)
    {
        return 1;
    }

    image = &palette->images[palette->numImages];

    image->path = strdup(path);
    image->name = strings_basename(path);
    image->data = NULL;
    image->width = 0;
    image->height = 0;

    palette->numImages++;

    return 0;
}


/*
 * Adds a path which may or may not include images.
 */
int pallete_add_path(palette_t *palette, const char *path)
{
    glob_t *globbuf = NULL;
    char **paths = NULL;
    int i;
    int len;

    if (palette == NULL || path == NULL)
    {
        return 1;
    }

    globbuf = strings_find_images(path);
    paths = globbuf->gl_pathv;
    len = globbuf->gl_pathc;

    if (len == 0)
    {
        LL_ERROR("Could not find file(s): \'%s\'", path);
        return 1;
    }

    for (i = 0; i < len; ++i)
    {
        palette_add_image(palette, paths[i]);
    }

    globfree(globbuf);
    free(globbuf);

    return 0;
}

/*
 * Frees an allocated palette.
 */
void palette_free(palette_t *palette)
{
    int i;

    if (palette == NULL)
    {
        return;
    }

    for (i = 0; i < palette->numImages; ++i)
    {
        image_t *image = &palette->images[i];

        free(image->name);
        image->name = NULL;

        free(image->path);
        image->path = NULL;
    }

    free(palette->images);
    palette->images = NULL;

    free(palette->name);
    palette->name = NULL;
}

/*
 * Generates a palette from a builtin one.
 */
int palette_generate_builtin(palette_t *palette,
                             uint8_t *builtin,
                             int numEntries,
                             color_mode_t mode)
{
    int i;

    for (i = 0; i < numEntries; ++i)
    {
        color_t *ec = &palette->entries[i].color;
        liq_color color =
            {
                .r = builtin[(i * 3) + 0],
                .g = builtin[(i * 3) + 1],
                .b = builtin[(i * 3) + 2],
                .a = 255
            };

        ec->rgb = color;

        color_convert(ec, mode);

        palette->entries[i].index = i;
    }

    palette->numEntries = numEntries;

    return 0;
}

/*
 * In automatic mode, read the images from the converts using the palette.
 */
int palette_automatic_build(palette_t *palette, convert_t **converts, int numConverts)
{
    int i, j, k;
    int ret = 0;

    for (i = 0; i < numConverts; ++i)
    {
        for (j = 0; j < converts[i]->numImages; ++j)
        {
            ret = palette_add_image(palette, converts[i]->images[j].path);
            if (ret != 0)
            {
                goto error;
            }
        }

        for (j = 0; j < converts[i]->numTilesetGroups; ++j)
        {
            tileset_group_t *tilesetGroup = converts[i]->tilesetGroups[j];

            for (k = 0; k < tilesetGroup->numTilesets; ++k)
            {
                ret = palette_add_image(palette, tilesetGroup->tilesets[k].image.path);
                if (ret != 0)
                {
                    goto error;
                }
            }
        }
    }

error:
    return ret;
}

/*
 * Reads all input images, and generates a palette for convert.
 */
int palette_generate(palette_t *palette, convert_t **converts, int numConverts)
{
    liq_attr *attr = NULL;
    liq_histogram *hist = NULL;
    liq_result *liqresult = NULL;
    const liq_palette *liqpalette = NULL;
    liq_error liqerr;
    int i, j;
    int ret;

    if (palette == NULL)
    {
        return 1;
    }

    if (!strcmp(palette->name, "xlibc"))
    {
        return palette_generate_builtin(palette,
                                        palette_xlibc,
                                        PALETTE_MAX_ENTRIES,
                                        COLOR_MODE_1555_GBGR);
    }
    else if (!strcmp(palette->name, "rgb332"))
    {
        return palette_generate_builtin(palette,
                                        palette_rgb332,
                                        PALETTE_MAX_ENTRIES,
                                        COLOR_MODE_1555_GBGR);
    }

    LL_INFO("Generating palette \'%s\'", palette->name);

    if (palette->automatic)
    {
        ret = palette_automatic_build(palette, converts, numConverts);
        if (ret != 0)
        {
            return ret;
        }
    }

    if (palette->numImages == 0)
    {
        LL_ERROR("No images to convert for palette \'%s\'", palette->name);
        return 1;
    }

    attr = liq_attr_create();

    liq_set_speed(attr, palette->quantizeSpeed);
    liq_set_max_colors(attr, palette->maxEntries);

    hist = liq_histogram_create(attr);

    for (i = 0; i < palette->numFixedEntries; ++i)
    {
        color_t *color = &palette->fixedEntries[i].color;

        color_convert(color, palette->mode);

        liq_histogram_add_fixed_color(hist, color->rgb, 0);
    }

    for (i = 0; i < palette->numImages; ++i)
    {
        image_t *image = &palette->images[i];
        liq_image *liqimage;

        LL_INFO(" - Reading \'%s\'",
            image->path);

        if( image_load(image) != 0 )
        {
            LL_ERROR("Failed to load image \'%s\'", image->path);
            liq_histogram_destroy(hist);
            liq_attr_destroy(attr);
            return 1;
        }

        liqimage = liq_image_create_rgba(attr,
                                         image->data,
                                         image->width,
                                         image->height,
                                         0);

        liq_histogram_add_image(hist, attr, liqimage);
        liq_image_destroy(liqimage);
        free(image->data);
    }

    liqerr = liq_histogram_quantize(hist, attr, &liqresult);
    if (liqerr != LIQ_OK)
    {
        LL_ERROR("Failed to generate palette \'%s\'\n", palette->name);
        liq_histogram_destroy(hist);
        liq_attr_destroy(attr);
        return 1;
    }

    liqpalette = liq_get_palette(liqresult);

    palette->numEntries = liqpalette->count;

    for (i = 0; i < (int)liqpalette->count; ++i)
    {
        color_t *ec = &palette->entries[i].color;

        ec->rgb = liqpalette->entries[i];

        color_convert(ec, palette->mode);

        palette->entries[i].index = i;
    }

    for (i = 0; i < palette->numFixedEntries; ++i)
    {
        color_t *fc = &palette->fixedEntries[i].color;

        for (j = 0; j < (int)liqpalette->count; ++j)
        {
            color_t *ec = &palette->entries[j].color;

            if( ec->target == fc->target )
            {
                color_t tmp = *ec;
                *ec = *fc;
                *fc = tmp;

                i = -1;
                break;
            }
        }
    }

    liq_result_destroy(liqresult);
    liq_histogram_destroy(hist);
    liq_attr_destroy(attr);

    return 0;
}

static uint8_t palette_xlibc[] =
{
    0x00,0x00,0x00,
    0x00,0x20,0x08,
    0x00,0x41,0x10,
    0x00,0x61,0x18,
    0x00,0x82,0x21,
    0x00,0xA2,0x29,
    0x00,0xC3,0x31,
    0x00,0xE3,0x39,
    0x08,0x00,0x42,
    0x08,0x20,0x4A,
    0x08,0x41,0x52,
    0x08,0x61,0x5A,
    0x08,0x82,0x63,
    0x08,0xA2,0x6B,
    0x08,0xC3,0x73,
    0x08,0xE3,0x7B,
    0x10,0x00,0x84,
    0x10,0x20,0x8C,
    0x10,0x41,0x94,
    0x10,0x61,0x9C,
    0x10,0x82,0xA5,
    0x10,0xA2,0xAD,
    0x10,0xC3,0xB5,
    0x10,0xE3,0xBD,
    0x18,0x00,0xC6,
    0x18,0x20,0xCE,
    0x18,0x41,0xD6,
    0x18,0x61,0xDE,
    0x18,0x82,0xE7,
    0x18,0xA2,0xEF,
    0x18,0xC3,0xF7,
    0x18,0xE3,0xFF,
    0x21,0x04,0x00,
    0x21,0x24,0x08,
    0x21,0x45,0x10,
    0x21,0x65,0x18,
    0x21,0x86,0x21,
    0x21,0xA6,0x29,
    0x21,0xC7,0x31,
    0x21,0xE7,0x39,
    0x29,0x04,0x42,
    0x29,0x24,0x4A,
    0x29,0x45,0x52,
    0x29,0x65,0x5A,
    0x29,0x86,0x63,
    0x29,0xA6,0x6B,
    0x29,0xC7,0x73,
    0x29,0xE7,0x7B,
    0x31,0x04,0x84,
    0x31,0x24,0x8C,
    0x31,0x45,0x94,
    0x31,0x65,0x9C,
    0x31,0x86,0xA5,
    0x31,0xA6,0xAD,
    0x31,0xC7,0xB5,
    0x31,0xE7,0xBD,
    0x39,0x04,0xC6,
    0x39,0x24,0xCE,
    0x39,0x45,0xD6,
    0x39,0x65,0xDE,
    0x39,0x86,0xE7,
    0x39,0xA6,0xEF,
    0x39,0xC7,0xF7,
    0x39,0xE7,0xFF,
    0x42,0x08,0x00,
    0x42,0x28,0x08,
    0x42,0x49,0x10,
    0x42,0x69,0x18,
    0x42,0x8A,0x21,
    0x42,0xAA,0x29,
    0x42,0xCB,0x31,
    0x42,0xEB,0x39,
    0x4A,0x08,0x42,
    0x4A,0x28,0x4A,
    0x4A,0x49,0x52,
    0x4A,0x69,0x5A,
    0x4A,0x8A,0x63,
    0x4A,0xAA,0x6B,
    0x4A,0xCB,0x73,
    0x4A,0xEB,0x7B,
    0x52,0x08,0x84,
    0x52,0x28,0x8C,
    0x52,0x49,0x94,
    0x52,0x69,0x9C,
    0x52,0x8A,0xA5,
    0x52,0xAA,0xAD,
    0x52,0xCB,0xB5,
    0x52,0xEB,0xBD,
    0x5A,0x08,0xC6,
    0x5A,0x28,0xCE,
    0x5A,0x49,0xD6,
    0x5A,0x69,0xDE,
    0x5A,0x8A,0xE7,
    0x5A,0xAA,0xEF,
    0x5A,0xCB,0xF7,
    0x5A,0xEB,0xFF,
    0x63,0x0C,0x00,
    0x63,0x2C,0x08,
    0x63,0x4D,0x10,
    0x63,0x6D,0x18,
    0x63,0x8E,0x21,
    0x63,0xAE,0x29,
    0x63,0xCF,0x31,
    0x63,0xEF,0x39,
    0x6B,0x0C,0x42,
    0x6B,0x2C,0x4A,
    0x6B,0x4D,0x52,
    0x6B,0x6D,0x5A,
    0x6B,0x8E,0x63,
    0x6B,0xAE,0x6B,
    0x6B,0xCF,0x73,
    0x6B,0xEF,0x7B,
    0x73,0x0C,0x84,
    0x73,0x2C,0x8C,
    0x73,0x4D,0x94,
    0x73,0x6D,0x9C,
    0x73,0x8E,0xA5,
    0x73,0xAE,0xAD,
    0x73,0xCF,0xB5,
    0x73,0xEF,0xBD,
    0x7B,0x0C,0xC6,
    0x7B,0x2C,0xCE,
    0x7B,0x4D,0xD6,
    0x7B,0x6D,0xDE,
    0x7B,0x8E,0xE7,
    0x7B,0xAE,0xEF,
    0x7B,0xCF,0xF7,
    0x7B,0xEF,0xFF,
    0x84,0x10,0x00,
    0x84,0x30,0x08,
    0x84,0x51,0x10,
    0x84,0x71,0x18,
    0x84,0x92,0x21,
    0x84,0xB2,0x29,
    0x84,0xD3,0x31,
    0x84,0xF3,0x39,
    0x8C,0x10,0x42,
    0x8C,0x30,0x4A,
    0x8C,0x51,0x52,
    0x8C,0x71,0x5A,
    0x8C,0x92,0x63,
    0x8C,0xB2,0x6B,
    0x8C,0xD3,0x73,
    0x8C,0xF3,0x7B,
    0x94,0x10,0x84,
    0x94,0x30,0x8C,
    0x94,0x51,0x94,
    0x94,0x71,0x9C,
    0x94,0x92,0xA5,
    0x94,0xB2,0xAD,
    0x94,0xD3,0xB5,
    0x94,0xF3,0xBD,
    0x9C,0x10,0xC6,
    0x9C,0x30,0xCE,
    0x9C,0x51,0xD6,
    0x9C,0x71,0xDE,
    0x9C,0x92,0xE7,
    0x9C,0xB2,0xEF,
    0x9C,0xD3,0xF7,
    0x9C,0xF3,0xFF,
    0xA5,0x14,0x00,
    0xA5,0x34,0x08,
    0xA5,0x55,0x10,
    0xA5,0x75,0x18,
    0xA5,0x96,0x21,
    0xA5,0xB6,0x29,
    0xA5,0xD7,0x31,
    0xA5,0xF7,0x39,
    0xAD,0x14,0x42,
    0xAD,0x34,0x4A,
    0xAD,0x55,0x52,
    0xAD,0x75,0x5A,
    0xAD,0x96,0x63,
    0xAD,0xB6,0x6B,
    0xAD,0xD7,0x73,
    0xAD,0xF7,0x7B,
    0xB5,0x14,0x84,
    0xB5,0x34,0x8C,
    0xB5,0x55,0x94,
    0xB5,0x75,0x9C,
    0xB5,0x96,0xA5,
    0xB5,0xB6,0xAD,
    0xB5,0xD7,0xB5,
    0xB5,0xF7,0xBD,
    0xBD,0x14,0xC6,
    0xBD,0x34,0xCE,
    0xBD,0x55,0xD6,
    0xBD,0x75,0xDE,
    0xBD,0x96,0xE7,
    0xBD,0xB6,0xEF,
    0xBD,0xD7,0xF7,
    0xBD,0xF7,0xFF,
    0xC6,0x18,0x00,
    0xC6,0x38,0x08,
    0xC6,0x59,0x10,
    0xC6,0x79,0x18,
    0xC6,0x9A,0x21,
    0xC6,0xBA,0x29,
    0xC6,0xDB,0x31,
    0xC6,0xFB,0x39,
    0xCE,0x18,0x42,
    0xCE,0x38,0x4A,
    0xCE,0x59,0x52,
    0xCE,0x79,0x5A,
    0xCE,0x9A,0x63,
    0xCE,0xBA,0x6B,
    0xCE,0xDB,0x73,
    0xCE,0xFB,0x7B,
    0xD6,0x18,0x84,
    0xD6,0x38,0x8C,
    0xD6,0x59,0x94,
    0xD6,0x79,0x9C,
    0xD6,0x9A,0xA5,
    0xD6,0xBA,0xAD,
    0xD6,0xDB,0xB5,
    0xD6,0xFB,0xBD,
    0xDE,0x18,0xC6,
    0xDE,0x38,0xCE,
    0xDE,0x59,0xD6,
    0xDE,0x79,0xDE,
    0xDE,0x9A,0xE7,
    0xDE,0xBA,0xEF,
    0xDE,0xDB,0xF7,
    0xDE,0xFB,0xFF,
    0xE7,0x1C,0x00,
    0xE7,0x3C,0x08,
    0xE7,0x5D,0x10,
    0xE7,0x7D,0x18,
    0xE7,0x9E,0x21,
    0xE7,0xBE,0x29,
    0xE7,0xDF,0x31,
    0xE7,0xFF,0x39,
    0xEF,0x1C,0x42,
    0xEF,0x3C,0x4A,
    0xEF,0x5D,0x52,
    0xEF,0x7D,0x5A,
    0xEF,0x9E,0x63,
    0xEF,0xBE,0x6B,
    0xEF,0xDF,0x73,
    0xEF,0xFF,0x7B,
    0xF7,0x1C,0x84,
    0xF7,0x3C,0x8C,
    0xF7,0x5D,0x94,
    0xF7,0x7D,0x9C,
    0xF7,0x9E,0xA5,
    0xF7,0xBE,0xAD,
    0xF7,0xDF,0xB5,
    0xF7,0xFF,0xBD,
    0xFF,0x1C,0xC6,
    0xFF,0x3C,0xCE,
    0xFF,0x5D,0xD6,
    0xFF,0x7D,0xDE,
    0xFF,0x9E,0xE7,
    0xFF,0xBE,0xEF,
    0xFF,0xDF,0xF7,
    0xFF,0xFF,0xFF,
};

static uint8_t palette_rgb332[] =
{
    0x00,0x00,0x00,
    0x00,0x00,0x68,
    0x00,0x00,0xB7,
    0x00,0x00,0xFF,
    0x33,0x00,0x00,
    0x33,0x00,0x68,
    0x33,0x00,0xB7,
    0x33,0x00,0xFF,
    0x5C,0x00,0x00,
    0x5C,0x00,0x68,
    0x5C,0x00,0xB7,
    0x5C,0x00,0xFF,
    0x7F,0x00,0x00,
    0x7F,0x00,0x68,
    0x7F,0x00,0xB7,
    0x7F,0x00,0xFF,
    0xA2,0x00,0x00,
    0xA2,0x00,0x68,
    0xA2,0x00,0xB7,
    0xA2,0x00,0xFF,
    0xC1,0x00,0x00,
    0xC1,0x00,0x68,
    0xC1,0x00,0xB7,
    0xC1,0x00,0xFF,
    0xE1,0x00,0x00,
    0xE1,0x00,0x68,
    0xE1,0x00,0xB7,
    0xE1,0x00,0xFF,
    0xFF,0x00,0x00,
    0xFF,0x00,0x68,
    0xFF,0x00,0xB7,
    0xFF,0x00,0xFF,
    0x00,0x33,0x00,
    0x00,0x33,0x68,
    0x00,0x33,0xB7,
    0x00,0x33,0xFF,
    0x33,0x33,0x00,
    0x33,0x33,0x68,
    0x33,0x33,0xB7,
    0x33,0x33,0xFF,
    0x5C,0x33,0x00,
    0x5C,0x33,0x68,
    0x5C,0x33,0xB7,
    0x5C,0x33,0xFF,
    0x7F,0x33,0x00,
    0x7F,0x33,0x68,
    0x7F,0x33,0xB7,
    0x7F,0x33,0xFF,
    0xA2,0x33,0x00,
    0xA2,0x33,0x68,
    0xA2,0x33,0xB7,
    0xA2,0x33,0xFF,
    0xC1,0x33,0x00,
    0xC1,0x33,0x68,
    0xC1,0x33,0xB7,
    0xC1,0x33,0xFF,
    0xE1,0x33,0x00,
    0xE1,0x33,0x68,
    0xE1,0x33,0xB7,
    0xE1,0x33,0xFF,
    0xFF,0x33,0x00,
    0xFF,0x33,0x68,
    0xFF,0x33,0xB7,
    0xFF,0x33,0xFF,
    0x00,0x5C,0x00,
    0x00,0x5C,0x68,
    0x00,0x5C,0xB7,
    0x00,0x5C,0xFF,
    0x33,0x5C,0x00,
    0x33,0x5C,0x68,
    0x33,0x5C,0xB7,
    0x33,0x5C,0xFF,
    0x5C,0x5C,0x00,
    0x5C,0x5C,0x68,
    0x5C,0x5C,0xB7,
    0x5C,0x5C,0xFF,
    0x7F,0x5C,0x00,
    0x7F,0x5C,0x68,
    0x7F,0x5C,0xB7,
    0x7F,0x5C,0xFF,
    0xA2,0x5C,0x00,
    0xA2,0x5C,0x68,
    0xA2,0x5C,0xB7,
    0xA2,0x5C,0xFF,
    0xC1,0x5C,0x00,
    0xC1,0x5C,0x68,
    0xC1,0x5C,0xB7,
    0xC1,0x5C,0xFF,
    0xE1,0x5C,0x00,
    0xE1,0x5C,0x68,
    0xE1,0x5C,0xB7,
    0xE1,0x5C,0xFF,
    0xFF,0x5C,0x00,
    0xFF,0x5C,0x68,
    0xFF,0x5C,0xB7,
    0xFF,0x5C,0xFF,
    0x00,0x7F,0x00,
    0x00,0x7F,0x68,
    0x00,0x7F,0xB7,
    0x00,0x7F,0xFF,
    0x33,0x7F,0x00,
    0x33,0x7F,0x68,
    0x33,0x7F,0xB7,
    0x33,0x7F,0xFF,
    0x5C,0x7F,0x00,
    0x5C,0x7F,0x68,
    0x5C,0x7F,0xB7,
    0x5C,0x7F,0xFF,
    0x7F,0x7F,0x00,
    0x7F,0x7F,0x68,
    0x7F,0x7F,0xB7,
    0x7F,0x7F,0xFF,
    0xA2,0x7F,0x00,
    0xA2,0x7F,0x68,
    0xA2,0x7F,0xB7,
    0xA2,0x7F,0xFF,
    0xC1,0x7F,0x00,
    0xC1,0x7F,0x68,
    0xC1,0x7F,0xB7,
    0xC1,0x7F,0xFF,
    0xE1,0x7F,0x00,
    0xE1,0x7F,0x68,
    0xE1,0x7F,0xB7,
    0xE1,0x7F,0xFF,
    0xFF,0x7F,0x00,
    0xFF,0x7F,0x68,
    0xFF,0x7F,0xB7,
    0xFF,0x7F,0xFF,
    0x00,0xA2,0x00,
    0x00,0xA2,0x68,
    0x00,0xA2,0xB7,
    0x00,0xA2,0xFF,
    0x33,0xA2,0x00,
    0x33,0xA2,0x68,
    0x33,0xA2,0xB7,
    0x33,0xA2,0xFF,
    0x5C,0xA2,0x00,
    0x5C,0xA2,0x68,
    0x5C,0xA2,0xB7,
    0x5C,0xA2,0xFF,
    0x7F,0xA2,0x00,
    0x7F,0xA2,0x68,
    0x7F,0xA2,0xB7,
    0x7F,0xA2,0xFF,
    0xA2,0xA2,0x00,
    0xA2,0xA2,0x68,
    0xA2,0xA2,0xB7,
    0xA2,0xA2,0xFF,
    0xC1,0xA2,0x00,
    0xC1,0xA2,0x68,
    0xC1,0xA2,0xB7,
    0xC1,0xA2,0xFF,
    0xE1,0xA2,0x00,
    0xE1,0xA2,0x68,
    0xE1,0xA2,0xB7,
    0xE1,0xA2,0xFF,
    0xFF,0xA2,0x00,
    0xFF,0xA2,0x68,
    0xFF,0xA2,0xB7,
    0xFF,0xA2,0xFF,
    0x00,0xC1,0x00,
    0x00,0xC1,0x68,
    0x00,0xC1,0xB7,
    0x00,0xC1,0xFF,
    0x33,0xC1,0x00,
    0x33,0xC1,0x68,
    0x33,0xC1,0xB7,
    0x33,0xC1,0xFF,
    0x5C,0xC1,0x00,
    0x5C,0xC1,0x68,
    0x5C,0xC1,0xB7,
    0x5C,0xC1,0xFF,
    0x7F,0xC1,0x00,
    0x7F,0xC1,0x68,
    0x7F,0xC1,0xB7,
    0x7F,0xC1,0xFF,
    0xA2,0xC1,0x00,
    0xA2,0xC1,0x68,
    0xA2,0xC1,0xB7,
    0xA2,0xC1,0xFF,
    0xC1,0xC1,0x00,
    0xC1,0xC1,0x68,
    0xC1,0xC1,0xB7,
    0xC1,0xC1,0xFF,
    0xE1,0xC1,0x00,
    0xE1,0xC1,0x68,
    0xE1,0xC1,0xB7,
    0xE1,0xC1,0xFF,
    0xFF,0xC1,0x00,
    0xFF,0xC1,0x68,
    0xFF,0xC1,0xB7,
    0xFF,0xC1,0xFF,
    0x00,0xE1,0x00,
    0x20,0xE1,0x68,
    0x00,0xE1,0xB7,
    0x00,0xE1,0xFF,
    0x33,0xE1,0x00,
    0x33,0xE1,0x68,
    0x33,0xE1,0xB7,
    0x33,0xE1,0xFF,
    0x5C,0xE1,0x00,
    0x5C,0xE1,0x68,
    0x5C,0xE1,0xB7,
    0x5C,0xE1,0xFF,
    0x7F,0xE1,0x00,
    0x7F,0xE1,0x68,
    0x7F,0xE1,0xB7,
    0x7F,0xE1,0xFF,
    0xA2,0xE1,0x00,
    0xA2,0xE1,0x68,
    0xA2,0xE1,0xB7,
    0xA2,0xE1,0xFF,
    0xC1,0xE1,0x00,
    0xC1,0xE1,0x68,
    0xC1,0xE1,0xB7,
    0xC1,0xE1,0xFF,
    0xE1,0xE1,0x00,
    0xE1,0xE1,0x68,
    0xE1,0xE1,0xB7,
    0xE1,0xE1,0xFF,
    0xFF,0xE1,0x00,
    0xFF,0xE1,0x68,
    0xFF,0xE1,0xB7,
    0xFF,0xE1,0xFF,
    0x00,0xFF,0x00,
    0x00,0xFF,0x68,
    0x00,0xFF,0xB7,
    0x00,0xFF,0xFF,
    0x33,0xFF,0x00,
    0x33,0xFF,0x68,
    0x33,0xFF,0xB7,
    0x33,0xFF,0xFF,
    0x5C,0xFF,0x00,
    0x5C,0xFF,0x68,
    0x5C,0xFF,0xB7,
    0x5C,0xFF,0xFF,
    0x7F,0xFF,0x00,
    0x7F,0xFF,0x68,
    0x7F,0xFF,0xB7,
    0x7F,0xFF,0xFF,
    0xA2,0xFF,0x00,
    0xA2,0xFF,0x68,
    0xA2,0xFF,0xB7,
    0xA2,0xFF,0xFF,
    0xC1,0xFF,0x00,
    0xC1,0xFF,0x68,
    0xC1,0xFF,0xB7,
    0xC1,0xFF,0xFF,
    0xE1,0xFF,0x00,
    0xE1,0xFF,0x68,
    0xE1,0xFF,0xB7,
    0xE1,0xFF,0xFF,
    0xFF,0xFF,0x00,
    0xFF,0xFF,0x68,
    0xFF,0xFF,0xB7,
    0xFF,0xFF,0xFF,
};
