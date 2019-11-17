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
#include "output.h"
#include "tileset.h"
#include "strings.h"
#include "image.h"
#include "log.h"

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

/*
 * Outputs to C format.
 */
static int output_c(const char *name, unsigned char *arr, size_t size, FILE *fdo)
{
    size_t i;

    fprintf(fdo, "unsigned char %s[%lu] =\r\n{", name, size);

    for (i = 0; i < size; ++i)
    {
        if (i % 32 == 0)
        {
            fputs("\r\n    ", fdo);
        }

        if (i + 1 == size)
        {
            fprintf(fdo, "0x%02x", arr[i]);
        }
        else
        {
            fprintf(fdo, "0x%02x,", arr[i]);
        }
    }
    fputs("\r\n};\r\n", fdo);

    return 0;
}

/*
 * Outputs a converted C image.
 */
int output_c_image(image_t *image)
{
    char *header = strdupcat(image->name, ".h");
    char *source = strdupcat(image->name, ".c");
    FILE *fdh;
    FILE *fds;

    LL_INFO(" - Writing \'%s\'", header);

    fdh = fopen(header, "w");
    if (fdh == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    fprintf(fdh, "#ifndef %s_include_file\r\n", image->name);
    fprintf(fdh, "#define %s_include_file\r\n", image->name);
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#ifdef __cplusplus\r\n");
    fprintf(fdh, "extern \"C\" {\r\n");
    fprintf(fdh, "#endif\r\n");
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#define %s_width %d\r\n", image->name, image->width);
    fprintf(fdh, "#define %s_height %d\r\n", image->name, image->height);
    fprintf(fdh, "#define %s_size %d\r\n", image->name, image->size);
    fprintf(fdh, "#define %s ((gfx_sprite_t*)%s_data)\r\n", image->name, image->name);
    fprintf(fdh, "extern unsigned char %s_data[%d];\r\n", image->name, image->size);
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#ifdef __cplusplus\r\n");
    fprintf(fdh, "}\r\n");
    fprintf(fdh, "#endif\r\n");
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#endif\r\n");

    fclose(fdh);

    LL_INFO(" - Writing \'%s\'", source);

    fds = fopen(source, "w");
    if (fds == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    output_c(image->name, image->data, image->size, fds);

    fclose(fds);

    free(header);
    free(source);

    return 0;

error:
    free(header);
    free(source);
    return 1;
}


/*
 * Outputs a converted C tileset.
 */
int output_c_tileset(tileset_t *tileset)
{
    char *header = strdupcat(tileset->image.name, ".h");
    char *source = strdupcat(tileset->image.name, ".c");
    FILE *fdh;
    FILE *fds;

    LL_INFO(" - Writing \'%s\'", header);

    fdh = fopen(header, "w");
    if (fdh == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    fclose(fdh);

    LL_INFO(" - Writing \'%s\'", source);

    fds = fopen(source, "w");
    if (fds == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    fclose(fds);

    free(header);
    free(source);

    return 0;

error:
    free(header);
    free(source);
    return 1;
}

/*
 * Outputs a converted C tileset.
 */
int output_c_palette(palette_t *palette)
{
    char *header = strdupcat(palette->name, ".h");
    char *source = strdupcat(palette->name, ".c");
    int size = palette->numEntries * 2;
    FILE *fdh;
    FILE *fds;
    int i;

    LL_INFO(" - Writing \'%s\'", header);

    fdh = fopen(header, "w");
    if (fdh == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    fprintf(fdh, "#ifndef %s_include_file\r\n", palette->name);
    fprintf(fdh, "#define %s_include_file\r\n", palette->name);
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#ifdef __cplusplus\r\n");
    fprintf(fdh, "extern \"C\" {\r\n");
    fprintf(fdh, "#endif\r\n");
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#define sizeof_%s %d\r\n", palette->name, size);
    fprintf(fdh, "extern unsigned char %s[%d];\r\n", palette->name, size);
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#ifdef __cplusplus\r\n");
    fprintf(fdh, "}\r\n");
    fprintf(fdh, "#endif\r\n");
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#endif\r\n");

    fclose(fdh);

    LL_INFO(" - Writing \'%s\'", source);

    fds = fopen(source, "w");
    if (fds == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    fprintf(fds, "unsigned char %s[%d] =\r\n{\r\n", palette->name, size);

    for (i = 0; i < palette->numEntries; ++i)
    {
        color_t *color = &palette->entries[i].color;

        fprintf(fds, "    0x%04x /* rgb(%3d, %3d, %3d) */\r\n",
                color->target,
                color->rgb.r,
                color->rgb.g,
                color->rgb.b);
    }
    fprintf(fds, "};\r\n");

    fclose(fds);

    free(header);
    free(source);

    return 0;

error:
    free(header);
    free(source);
    return 1;
}

/*
 * Outputs an include file for the output structure
 */
int output_c_include_file(output_t *output)
{
    char *include = output->includeFileName;
    FILE *fdi;

    LL_INFO(" - Writing \'%s\'", include);

    fdi = fopen(include, "w");
    if (fdi == NULL)
    {
        LL_ERROR(" Could not open file: %s", strerror(errno));
        goto error;
    }

    fclose(fdi);

    free(include);

    return 0;

error:
    free(include);
    return 1;
}
