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
 * Outputs to Binary format.
 */
static int output_bin(unsigned char *data, size_t size, FILE *fdo)
{
    int ret = fwrite(data, size, 1, fdo);

    return ret == 1 ? 0 : 1;
}

/*
 * Outputs a converted image to Binary.
 */
int output_bin_image(image_t *image)
{
    char *source = strdupcat(image->directory, ".bin");
    FILE *fds;
    int ret;

    LL_INFO(" - Writing \'%s\'", source);

    fds = fopen(source, "w");
    if (fds == NULL)
    {
        LL_ERROR("Could not open file: %s", strerror(errno));
        goto error;
    }

    ret = output_bin(image->data, image->size, fds);

    fclose(fds);

    free(source);

    return ret;

error:
    free(source);
    return 1;
}

/*
 * Outputs a converted Assembly tileset.
 */
int output_bin_tileset(tileset_t *tileset)
{
    char *source = strdupcat(tileset->directory, ".bin");
    FILE *fds;
    int i;

    LL_INFO(" - Writing \'%s\'", source);

    fds = fopen(source, "w");
    if (fds == NULL)
    {
        LL_ERROR("Could not open file: %s", strerror(errno));
        goto error;
    }

    if (tileset->pTable == true)
    {
        int offset = tileset->numTiles * 3;

        for (i = 0; i < tileset->numTiles; ++i)
        {
            unsigned char tileOffset[3];

            tileOffset[0] = offset & 255;
            tileOffset[1] = (offset >> 8) & 255;
            tileOffset[2] = (offset >> 16) & 255;

            output_bin(tileOffset, sizeof tileOffset, fds);

            offset += tileset->tiles[i].size;
        }
    }

    for (i = 0; i < tileset->numTiles; ++i)
    {
        tileset_tile_t *tile = &tileset->tiles[i];

        output_bin(tile->data, tile->size, fds);
    }

    fclose(fds);

    free(source);

    return 0;

error:
    free(source);
    return 1;
}

/*
 * Outputs a converted Assembly tileset.
 */
int output_bin_palette(palette_t *palette)
{
    char *source = strdupcat(palette->directory, ".bin");
    FILE *fds;
    int i;

    LL_INFO(" - Writing \'%s\'", source);

    fds = fopen(source, "w");
    if (fds == NULL)
    {
        LL_ERROR("Could not open file: %s", strerror(errno));
        goto error;
    }

    for (i = 0; i < palette->numEntries; ++i)
    {
        color_t *color = &palette->entries[i].color;

        fwrite(&color->target, sizeof(uint16_t), 1, fds);
    }

    fclose(fds);

    free(source);

    return 0;

error:
    free(source);
    return 1;
}

/*
 * Outputs an include file for the output structure
 */
int output_bin_include_file(output_t *output)
{
    char *includeFile = output->includeFileName;
    char *includeName = strdup(output->includeFileName);
    char *tmp;
    FILE *fdi;
    int i, j, k;

    if (output->includeFileName == NULL)
    {
        return 0;
    }

    tmp = strchr(includeName, '.');
    if (tmp != NULL)
    {
        *tmp = '\0';
    }

    LL_INFO(" - Writing \'%s\'", includeFile);

    fdi = fopen(includeFile, "w");
    if (fdi == NULL)
    {
        LL_ERROR("Could not open file: %s", strerror(errno));
        goto error;
    }

    for (i = 0; i < output->numPalettes; ++i)
    {
        fprintf(fdi, "%s.bin\r\n", output->palettes[i]->name);
    }

    for (i = 0; i < output->numConverts; ++i)
    {
        convert_t *convert = output->converts[i];

        for (j = 0; j < convert->numImages; ++j)
        {
            image_t *image = &convert->images[j];

            fprintf(fdi, "%s.bin\r\n", image->name);
        }

        for (j = 0; j < convert->numTilesetGroups; ++j)
        {
            tileset_group_t *tilesetGroup = convert->tilesetGroups[j];

            for (k = 0; k < tilesetGroup->numTilesets; ++k)
            {
                tileset_t *tileset = &tilesetGroup->tilesets[k];

                fprintf(fdi, "%s.bin\r\n", tileset->image.name);
            }
        }
    }

    fclose(fdi);

    free(includeName);

    return 0;

error:
    free(includeName);
    return 1;
}
