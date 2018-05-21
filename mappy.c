/*
The MIT License (MIT)

Copyright (c) 2018 Taylor Adamson

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"
#define RGBA_MAX 255
#define STR_BUF 256
#define BASE_DEC 10
#define ASCII_SPACE 32
#define F_SECTIONS 4
#define F_MAX_ARGS 10
#define M_INIT 64
#define M_SCALE 2

typedef struct
{
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color;

typedef struct
{
    int w;
    int h;
    color empty;
    uint8_t *buf;
} sheet;

typedef struct
{
    int x;
    int y;
    int w;
    int h;
} bounds;

typedef struct
{
    int x;
    int y;
    int id;
    bounds b;
} glyph;

sheet load(char *file)
{
    int w;
    int h;
    uint8_t *buf = stbi_load(file, &w, &h, 0, STBI_rgb_alpha);
    sheet new = {w, h, {}, buf};
    return new;
}

color spot(uint8_t *buf, int index)
{
    color new = {buf[index++], buf[index++], buf[index]};
    return new;
}

int compare(color c1, color c2)
{
    float a1 = (c1.r + c1.g + c1.b) / 3;
    float a2 = (c2.r + c2.g + c2.b) / 3;
    return a1 > a2 ? 1 : (a1 == a2 ? 0 : -1);
}

char *str(int arg)
{
    int len = snprintf(NULL, 0, "%d", arg);
    char *converted = malloc(sizeof(char) * ++len);
    itoa(arg, converted, BASE_DEC);
    return converted;
}

uint8_t *flood(int len)
{
    int size = len * len * STBI_rgb_alpha;
    uint8_t *new = malloc(size);
    for (int i = 0; i < size; i++) new[i] = 0;
    return new;
}

bounds query_bounds(sheet src, int index, bounds s_glyph)
{
    int x[2] = {INT_MAX, -1};
    int y[2] = {INT_MAX, -1};
    for (int i = 0; i < s_glyph.h; i++)
    {
        int shift = (index + i * src.w) * STBI_rgb_alpha;
        for (int j = shift; j < shift + s_glyph.w * STBI_rgb_alpha; j+= STBI_rgb_alpha)
        {
            if (compare(src.empty, spot(src.buf, j)) != 0)
            {
                int col = (j % shift) / STBI_rgb_alpha + 1;
                int row = i + 1;
                y[0] = row < y[0] ? row : y[0];
                x[0] = col < x[0] ? col : x[0];
                y[1] = row > y[1] ? row : y[1];
                x[1] = col > x[1] ? col : x[1];
            }
        }
    }
    int w = x[1] - x[0];
    int h = y[1] - y[0];
    bounds b = {x[0], y[0], w, h};
    return b;
}

void resize(uint8_t **buf, int len)
{
    uint8_t *new = flood(len);
    uint8_t *temp = *buf;
    int prev = len / M_SCALE;
    for (int i = 0; i < prev; i++)
    {
        int shift = (i * prev) * STBI_rgb_alpha;
        for (int j = shift; j < shift + prev * STBI_rgb_alpha; j++)
        {
            int s_shift = shift * M_SCALE;
            new[j - shift + s_shift] = temp[j];
        }
    }
    *buf = new;
}

void color_fix(sheet *src)
{
    int size = src->w * src->h * STBI_rgb_alpha;
    int to = RGBA_MAX;
    int index = 0;
    uint8_t *temp = malloc(size);
    color first = spot(src->buf, 0);
    color test;
    while (index < size)
    {
        test = spot(src->buf, (index / STBI_rgb_alpha) * STBI_rgb_alpha);
        int result = compare(first, test);
        temp[index] = result == 0 ? abs(to - RGBA_MAX) : to;
        index++;
        if (result > 0)
        {
            first = test;
            index = to = 0;
        }
    }
    src->buf = temp;
}

int query_region(sheet src, uint8_t *mem, bounds g, bounds r)
{
    for (int i = r.y; i <= r.y + g.h; i++)
    {
        int shift = (r.x + i * src.w) * STBI_rgb_alpha;
        for (int j = shift; j <= shift + g.w * STBI_rgb_alpha; j+= STBI_rgb_alpha)
        {
            color test = spot(mem, j);
            if (compare(src.empty, test) != 0) return 0;
        }
    }
    return 1;
}

void write_glyph(sheet *dest, sheet src, uint8_t *mem, bounds g, bounds r, int s_index)
{
    for (int i = r.y; i <= r.y + g.h; i++)
    {
        int shift = (r.x + i * dest->w) * STBI_rgb_alpha;
        for (int j = shift; j < shift + (g.w + 1) * STBI_rgb_alpha; j++)
        {
            int s_shift = j - shift + (s_index + (g.x - 1) + (src.w * (g.y - 1)) + (src.w * (i - r.y))) * STBI_rgb_alpha;
            color test = spot(src.buf, (s_shift / STBI_rgb_alpha) * STBI_rgb_alpha);
            dest->buf[j] = compare(src.empty, test) != 0 ? RGBA_MAX : 0;
            mem[j] = RGBA_MAX;
        }
    }
}

void write_atlas(sheet src, char *chars, char *file, glyph *glyphs, int count, bounds s_glyph, bounds s_space)
{
    int read = 0;
    sheet out = {M_INIT, M_INIT, {}, flood(M_INIT)};
    uint8_t *mem = flood(M_INIT);
    color_fix(&src);
    while (read < count)
    {
        int x_step = s_glyph.w + s_space.w;
        int y_step = s_glyph.h + s_space.h;
        int index = read * x_step;
        int col = ((index % src.w) / x_step) * x_step;
        int row = (index / src.w) * (y_step);
        int s_index = col + row * src.w;;
        int full = 1;
        bounds b = query_bounds(src, s_index, s_glyph);
        for (int i = 0; i < out.h - b.h; i++)
        {
            for (int j = 0; j < out.w - b.w; j++)
            {
                bounds r = {j, i};
                if (query_region(out, mem, b, r) > 0)
                {
                    write_glyph(&out, src, mem, b, r, s_index);
                    glyph g = {j, i, chars[read], b};
                    glyphs[read] = g;
                    i = j = INT_MAX - 1;
                    full = 0;
                }
            }
        }
        if (full > 0)
        {
            out.w *= M_SCALE;
            out.h *= M_SCALE;
            resize(&out.buf, out.w);
            resize(&mem, out.w);
            read--;
        }
        read++;
    }
    stbi_write_png(file, out.w, out.h, STBI_rgb_alpha, out.buf, out.w * STBI_rgb_alpha);
}

void write_fnt(FILE *fp, char *file, glyph *glyphs, int count, bounds s_glyph)
{
    char *base[] = {"common", "page", "chars", "char"};
    char *keys[F_SECTIONS][F_MAX_ARGS] = {{"lineHeight"}, {"id", "file"}, {"count"}};
    char *vals[F_SECTIONS][F_MAX_ARGS] = {{str(s_glyph.h)}, {"0", file}, {str(count)}};
    char *gw = str(s_glyph.w);
    fprintf(fp, "info");
    for (int i = 0; i < F_SECTIONS; i++)
    {
        char *prefix = base[i];
        int char_iter = strcmp(prefix, "char") == 0;
        for (int j = 0; j < (char_iter ? count : 1); j++)
        {
            if (char_iter)
            {
                glyph g = glyphs[j];
                char *g_keys[] = {"id", "x", "y", "width", "height", "xoffset", "yoffset", "xadvance"};
                char *g_vals[] = {str(g.id), str(g.x), str(g.y), str(++g.b.w), str(++g.b.h), str(--g.b.x), str(--g.b.y), gw};
                fprintf(fp, j == 0 ? "\nchar id=%d width=%s height=0 xadvance=%s" : NULL, ASCII_SPACE, gw, gw);
                memcpy(&keys[i], &g_keys, j == 0 ? sizeof(g_keys) : 0);
                memcpy(&vals[i], &g_vals, sizeof(g_vals));
            }
            fprintf(fp, "\n%s", prefix);
            for (int k = 0; k < F_MAX_ARGS; k++)
            {
                if (vals[i][k])
                {
                    char arg[STR_BUF];
                    snprintf(arg, STR_BUF, " %s=%s", keys[i][k], vals[i][k]);
                    fprintf(fp, arg);
                }
            }
        }
    }
}

void help()
{
    printf("\nUsage: mappy.exe [input file] [options]\n"
    "-o output file (no extension)\n"
    "-w char width\n"
    "-h char height\n"
    "-c column separation\n"
    "-r row separation\n"
    "-s char sequence\n");
}

int main(int argc, char **argv)
{
    char* file = argv[1];
    sheet loaded = load(file);
    if (!loaded.buf)
    {
        if (argc > 1) printf("%s\n", "Bad input file (corrupt or incorrect path)");
        help();
        return 1;
    }
    char *name = strtok(file, ".");
    char *ext = strtok(NULL, ".");
    char *chars = "!\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~";
    char *output = malloc(sizeof(char) * STR_BUF);
    char png[STR_BUF];
    char fnt[STR_BUF];
    int opt;
    bounds s_glyph = {};
    bounds s_space = {};
    optind++;
    snprintf(output, STR_BUF, "%s_output", name);
    while ((opt = getopt(argc, argv, "o:w:h:c:r:s:")) != -1)
    {
        switch(opt)
        {
            case 'o':
                output = optarg;
                char *ext = strchr(output, '.');
                if (ext) *ext = 0;
                break;
            case 'w':
                s_glyph.w = atoi(optarg);
                break;
            case 'h':
                s_glyph.h = atoi(optarg);
                break;
            case 'c':
                s_space.w = atoi(optarg);
                break;
            case 'r':
                s_space.h = atoi(optarg);
                break;
            case 's':
                chars = optarg;
                break;
            default:
                help();
                return 1;
        }
    }
    snprintf(png, STR_BUF, "%s.png", output);
    snprintf(fnt, STR_BUF, "%s.fnt", output);
    int count = strlen(chars);
    FILE *fp = fopen(fnt, "wb");
    glyph *glyphs = malloc(sizeof(glyph) * count);
    write_atlas(loaded, chars, png, glyphs, count, s_glyph, s_space);
    write_fnt(fp, png, glyphs, count, s_glyph);
    fclose(fp);
    return 0;
}