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
#include "appvar.h"
#include "strings.h"
#include "log.h"

#include <string.h>
#include <errno.h>

/*
 * Outputs a converted image to Binary.
 */
int output_appvar_image(image_t *image, appvar_t *appvar)
{
    if (appvar->size + image->size >= APPVAR_MAX_DATA_SIZE)
    {
        LL_ERROR("Too much data for AppVar \'%s\'.", appvar->name);
        return 1;
    }

    memcpy(&appvar->data[appvar->size], image->data, image->size);
    appvar->size += image->size;

    return 0;
}

/*
 * Outputs a converted AppVar tileset.
 */
int output_appvar_tileset(tileset_t *tileset, appvar_t *appvar)
{
    int i;

    for (i = 0; i < tileset->numTiles; ++i)
    {
        tileset_tile_t *tile = &tileset->tiles[i];

        if (appvar->size + tile->size >= APPVAR_MAX_DATA_SIZE)
        {
            LL_ERROR("Too much data for AppVar \'%s\'.", appvar->name);
            return 1;
        }

        memcpy(&appvar->data[appvar->size], tile->data, tile->size);
        appvar->size += tile->size;
    }

    return 0;
}

/*
 * Outputs a converted Assembly tileset.
 */
int output_appvar_palette(palette_t *palette, appvar_t *appvar)
{
    int i;

    for (i = 0; i < palette->numEntries; ++i)
    {
        uint8_t colorBytes[2];
        color_t *color = &palette->entries[i].color;

        if (appvar->size + sizeof colorBytes >= APPVAR_MAX_DATA_SIZE)
        {
            LL_ERROR("Too much data for AppVar \'%s\'.", appvar->name);
            return 1;
        }

        colorBytes[0] = color->target & 255;
        colorBytes[1] = (color->target >> 8) & 255;

        memcpy(&appvar->data[appvar->size], colorBytes, sizeof colorBytes);
        appvar->size += 2;
    }

    return 0;
}

/*
 * Outputs a C style header.
 */
void output_appvar_c_include_file(output_t *output, FILE *fdh)
{
    appvar_t *appvar = &output->appvar;
    int index = 0;
    int i, j, k, l;

    fprintf(fdh, "#ifndef %s_appvar_include_file\r\n", appvar->name);
    fprintf(fdh, "#define %s_appvar_include_file\r\n", appvar->name);
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#ifdef __cplusplus\r\n");
    fprintf(fdh, "extern \"C\" {\r\n");
    fprintf(fdh, "#endif\r\n");
    fprintf(fdh, "\r\n");

    for (i = 0; i < output->numPalettes; ++i)
    {
        palette_t *palette = output->palettes[i];
        int size = palette->numEntries * 2;

        fprintf(fdh, "#define sizeof_%s %d\r\n",
            palette->name,
            size);
        fprintf(fdh, "#define %s (%s_appvar[%d])\r\n",
            palette->name,
            appvar->name,
            index);

        index++;
    }

    for (i = 0; i < output->numConverts; ++i)
    {
        convert_t *convert = output->converts[i];

        for (j = 0; j < convert->numImages; ++j)
        {
            image_t *image = &convert->images[j];

            fprintf(fdh, "#define %s_width %d\n",
                image->name,
                image->width);
            fprintf(fdh, "#define %s_height %d\n",
                image->name,
                image->height);

            if (image->compressed)
            {
                fprintf(fdh, "#define %s_compressed %s_appvar[%d]\n",
                    image->name,
                    appvar->name,
                    index);
            }
            else
            {
                fprintf(fdh, "#define %s ((gfx_sprite_t*)%s_appvar[%d])\n",
                    image->name,
                    appvar->name,
                    index);
            }

            index++;
        }

        for (j = 0; j < convert->numTilesetGroups; ++j)
        {
            tileset_group_t *tilesetGroup = convert->tilesetGroups[j];

            for (k = 0; k < tilesetGroup->numTilesets; ++k)
            {
                tileset_t *tileset = &tilesetGroup->tilesets[k];

                tileset->appvarIndex = index;

                fprintf(fdh, "#define %s_tile_width %d\n",
                    tileset->image.name,
                    tileset->tileWidth);
                fprintf(fdh, "#define %s_tile_height %d\n",
                    tileset->image.name,
                    tileset->tileHeight);

                if (tileset->compressed)
                {
                    fprintf(fdh, "#define %s_compressed %s_appvar[%d]\n",
                        tileset->image.name,
                        appvar->name,
                        index);
                    fprintf(fdh, "#define %s_tiles_num %d\n",
                        tileset->image.name,
                        tileset->numTiles);
                    fprintf(fdh, "extern unsigned char *%s_tiles_compressed[%d];\n",
                        tileset->image.name,
                        tileset->numTiles);
                    for (l = 0; l < tileset->numTiles; l++)
                    {
                        fprintf(fdh, "#define %s_tile_%d_compressed %s_tiles_compressed[%d]\n",
                        tileset->image.name,
                        l,
                        tileset->image.name,
                        l);
                    }
                }
                else
                {
                    fprintf(fdh, "#define %s %s_appvar[%d]\n",
                        tileset->image.name,
                        appvar->name,
                        index);
                    fprintf(fdh, "#define %s_tiles_num %d\n",
                        tileset->image.name,
                        tileset->numTiles);
                    fprintf(fdh, "extern unsigned char *%s_tiles_data[%d];\n",
                        tileset->image.name,
                        tileset->numTiles);
                    fprintf(fdh, "#define %s_tiles ((gfx_sprite_t**)%s_tiles_data)\n",
                        tileset->image.name,
                        tileset->image.name);
                    for (l = 0; l < tileset->numTiles; l++)
                    {
                        fprintf(fdh, "#define %s_tile_%d ((gfx_sprite_t*)%s_tiles_data[%d])\n",
                        tileset->image.name,
                        l,
                        tileset->image.name,
                        l);
                    }
                }

                index++;
            }
        }
    }

    appvar->numEntries = index;

    fprintf(fdh, "extern unsigned char *%s_appvar[%d];\r\n",
        appvar->name,
        appvar->numEntries);

    if (appvar->init)
    {
        if (appvar->compress != COMPRESS_NONE)
        {
            fprintf(fdh, "unsigned char %s_init(void *addr);\r\n",
                appvar->name);
        }
        else
        {
            fprintf(fdh, "unsigned char %s_init(void);\r\n",
                appvar->name);
        }
    }

    fprintf(fdh, "\r\n");
    fprintf(fdh, "#ifdef __cplusplus\r\n");
    fprintf(fdh, "}\r\n");
    fprintf(fdh, "#endif\r\n");
    fprintf(fdh, "\r\n");
    fprintf(fdh, "#endif\r\n");
}

/*
 * Outputs a C style source file.
 */
void output_appvar_c_source_file(output_t *output, FILE *fds)
{
    appvar_t *appvar = &output->appvar;
    int offset = 0;
    int i, j, k, l;

    fprintf(fds, "#include \"%s\"\r\n", output->includeFileName);
    fprintf(fds, "#include <fileioc.h>\r\n");
    fprintf(fds, "\r\n");
    fprintf(fds, "unsigned char *%s_appvar[%d] =\r\n{\r\n",
        appvar->name,
        appvar->numEntries);

    /* output global appvar mapping */
    for (i = 0; i < output->numPalettes; ++i)
    {
        palette_t *palette = output->palettes[i];

        fprintf(fds, "    (unsigned char*)%d,\r\n",
            offset);

        offset += palette->numEntries * 2;
    }

    for (i = 0; i < output->numConverts; ++i)
    {
        convert_t *convert = output->converts[i];

        for (j = 0; j < convert->numImages; ++j)
        {
            fprintf(fds, "    (unsigned char*)%d,\r\n",
                offset);

            offset += convert->images[j].size;
        }

        for (j = 0; j < convert->numTilesetGroups; ++j)
        {
            tileset_group_t *tilesetGroup = convert->tilesetGroups[j];

            for (k = 0; k < tilesetGroup->numTilesets; ++k)
            {
                tileset_t *tileset = &tilesetGroup->tilesets[k];
                int tilesetOffset = 0;

                for (l = 0; l < tileset->numTiles; l++)
                {
                    tilesetOffset += tileset->tiles[l].size;
                }

                fprintf(fds, "    (unsigned char*)%d,\r\n",
                    offset);

                offset += tilesetOffset;
            }
        }
    }

    fprintf(fds, "};\r\n\r\n");

    /* output tilemap tables */
    for (i = 0; i < output->numConverts; ++i)
    {
        convert_t *convert = output->converts[i];

        for (j = 0; j < convert->numTilesetGroups; ++j)
        {
            tileset_group_t *tilesetGroup = convert->tilesetGroups[j];

            for (k = 0; k < tilesetGroup->numTilesets; ++k)
            {
                tileset_t *tileset = &tilesetGroup->tilesets[k];
                int tilesetOffset = 0;

                if (tileset->compressed)
                {
                    fprintf(fds, "unsigned char *%s_tiles_compressed[%d] =\r\n{\r\n",
                        tileset->image.name,
                        tileset->numTiles);
                }
                else
                {
                    fprintf(fds, "unsigned char *%s_tiles_data[%d] =\r\n{\r\n",
                        tileset->image.name,
                        tileset->numTiles);
                }

                for (l = 0; l < tileset->numTiles; l++)
                {
                    fprintf(fds, "    (unsigned char*)%d,\r\n",
                        tilesetOffset);

                    tilesetOffset += tileset->tiles[l].size;
                }

                fprintf(fds, "};\r\n\r\n");
            }
        }
    }

    if (appvar->init)
    {
        if (appvar->compress != COMPRESS_NONE)
        {
            fprintf(fds, "unsigned char %s_init(void *addr)\r\n", appvar->name);
            fprintf(fds, "{\r\n");
            fprintf(fds, "    unsigned int data, i;\r\n\r\n");
            fprintf(fds, "    data = (unsigned int)addr - (unsigned int)%s_appvar[0];\r\n", appvar->name);
            fprintf(fds, "    for (i = 0; i < %d; i++)\r\n", appvar->numEntries);
            fprintf(fds, "    {\r\n");
            fprintf(fds, "        %s_appvar[i] += data;\r\n", appvar->name);
            fprintf(fds, "    }\r\n\r\n");
        }
        else
        {
            fprintf(fds, "unsigned char %s_init(void)\r\n", appvar->name);
            fprintf(fds, "{\r\n");
            fprintf(fds, "    unsigned int data, i;\r\n");
            fprintf(fds, "    ti_var_t appvar;\r\n\r\n");
            fprintf(fds, "    ti_CloseAll();\r\n\r\n");
            fprintf(fds, "    appvar = ti_Open(\"%s\", \"r\");\r\n", appvar->name);
            fprintf(fds, "    if (appvar == 0)\r\n");
            fprintf(fds, "    {\r\n");
            fprintf(fds, "        return 0;\r\n");
            fprintf(fds, "    }\r\n\r\n");
            fprintf(fds, "    data = (unsigned int)ti_GetDataPtr(appvar) - (unsigned int)%s_appvar[0];\n", appvar->name);
            fprintf(fds, "    for (i = 0; i < %d; i++)\r\n", appvar->numEntries);
            fprintf(fds, "    {\r\n");
            fprintf(fds, "        %s_appvar[i] += data;\r\n", appvar->name);
            fprintf(fds, "    }\r\n\r\n");
            fprintf(fds, "    ti_CloseAll();\r\n\r\n");
        }

        /* output tilemap init */
        for (i = 0; i < output->numConverts; ++i)
        {
            convert_t *convert = output->converts[i];

            for (j = 0; j < convert->numTilesetGroups; ++j)
            {
                tileset_group_t *tilesetGroup = convert->tilesetGroups[j];

                for (k = 0; k < tilesetGroup->numTilesets; ++k)
                {
                    tileset_t *tileset = &tilesetGroup->tilesets[k];

                    if (tileset->compressed)
                    {
                        fprintf(fds, "    data = (unsigned int)%s_appvar[%u] - (unsigned int)%s_tiles_compressed[0];\r\n",
                            appvar->name,
                            tileset->appvarIndex,
                            tileset->image.name);
                        fprintf(fds, "    for (i = 0; i < %s_tiles_num; i++)\r\n",
                            tileset->image.name);
                        fprintf(fds, "    {\r\n");
                        fprintf(fds, "        %s_tiles_compressed[i] += data;\r\n",
                            tileset->image.name);
                        fprintf(fds, "    }\r\n\r\n");
                    }
                    else
                    {
                        fprintf(fds, "    data = (unsigned int)%s_appvar[%u] - (unsigned int)%s_tiles_data[0];\n",
                            appvar->name,
                            tileset->appvarIndex,
                            tileset->image.name);
                        fprintf(fds, "    for (i = 0; i < %s_tiles_num; i++)\r\n",
                            tileset->image.name);
                        fprintf(fds, "    {\r\n");
                        fprintf(fds, "        %s_tiles_data[i] += data;\r\n",
                            tileset->image.name);
                        fprintf(fds, "    }\r\n\r\n");
                    }
                }
            }
        }

        fprintf(fds, "    return 1;\r\n");
        fprintf(fds, "};\r\n\r\n");
    }
}

/*
 * Outputs an include file for the output structure
 */
int output_appvar_include_file(output_t *output, appvar_t *appvar)
{
    char *varName = strdupcat(appvar->directory, ".8xv");
    char *varCName = strdupcat(appvar->directory, ".c");
    FILE *fdh;
    FILE *fds;
    FILE *fdv;
    int ret = 1;

    if (appvar == NULL)
    {
        LL_DEBUG("Invalid param in %s", __func__);
        return 1;
    }

    if (appvar->name == NULL)
    {
        LL_ERROR("Missing \"name\" parameter for AppVar.");
        return 1;
    }

    if (varName == NULL || varCName == NULL)
    {
        LL_DEBUG("Memory error in %s", __func__);
        goto error;
    }

    switch (appvar->source)
    {
        case APPVAR_SOURCE_C:
            LL_INFO(" - Writing \'%s\'", output->includeFileName);

            fdh = fopen(output->includeFileName, "w");
            if (fdh == NULL)
            {
                LL_ERROR("Could not open file: %s", strerror(errno));
                goto error;
            }

            output_appvar_c_include_file(output, fdh);

            LL_INFO(" - Writing \'%s\'", varCName);

            fds = fopen(varCName, "w");
            if (fds == NULL)
            {
                fclose(fdh);
                LL_ERROR("Could not open file: %s", strerror(errno));
                goto error;
            }

            output_appvar_c_source_file(output, fds);

            fclose(fdh);
            fclose(fds);
            break;

        case APPVAR_SOURCE_ICE:
            break;
    }

    LL_INFO(" - Writing \'%s\'", varName);

    fdv = fopen(varName, "w");
    if (fdv == NULL)
    {
        LL_ERROR("Could not open file: %s", strerror(errno));
        goto error;
    }

    ret = appvar_write(appvar, fdv);
    if (ret != 0)
    {
        fclose(fdv);
        remove(varName);
        goto error;
    }

    fclose(fdv);

error:
    free(varName);
    free(varCName);
    return ret;
}
