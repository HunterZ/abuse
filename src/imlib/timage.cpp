/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com or
 *  Jonathan Clark.
 */

#include "config.h"

#include "common.h"

#include "timage.h"

trans_image::trans_image(image *im, char const *name)
{
    m_size = im->Size();

    im->Lock();

    // First find out how much data to allocate
    size_t bytes = 0;
    for (int y = 0; y < m_size.y; y++)
    {
        uint8_t *parser = im->scan_line(y);
        for (int x = 0; x < m_size.x; )
        {
            bytes++;
            while (x < m_size.x && *parser == 0)
            {
                parser++; x++;
            }

            if (x >= m_size.x)
                break;

            bytes++;  // byte for the size of the run
            while (x < m_size.x && *parser != 0)
            {
                bytes++;
                x++;
                parser++;
            }
        }
    }

    uint8_t *parser = m_data = (uint8_t *)malloc(bytes);
    if (!parser)
    {
        printf("size = %d %d (%d bytes)\n",im->Size().x,im->Size().y,bytes);
        CONDITION(parser, "malloc error for trans_image::m_data");
    }

    // Now fill the RLE transparency image
    for (int y = 0; y < m_size.y; y++)
    {
        uint8_t *sl = im->scan_line(y);

        for (int x = 0; x < m_size.x; )
        {
            uint8_t len = 0;
            while (x + len < m_size.x && sl[len] == 0)
                len++;

            *parser++ = len;
            x += len;
            sl += len;

            if (x >= m_size.x)
                break;

            len = 0;
            while (x + len < m_size.x && sl[len] != 0)
            {
                parser[len + 1] = sl[len];
                len++;
            }

            *parser++ = len;
            parser += len;
            x += len;
            sl += len;
        }
    }
    im->Unlock();
}

trans_image::~trans_image()
{
    free(m_data);
}

image *trans_image::ToImage()
{
    image *im = new image(m_size);

    // FIXME: this is required until FILLED mode is fixed
    im->Lock();
    memset(im->scan_line(0), 0, m_size.x * m_size.y);
    im->Unlock();

    PutImage(im, 0, 0);
    return im;
}

uint8_t *trans_image::ClipToLine(image *screen, int x1, int y1, int x2, int y2,
                                 int x, int &y, int &ysteps)
{
    // check to see if it is totally clipped out first
    if (y + m_size.y <= y1 || y >= y2 || x >= x2 || x + m_size.x <= x1)
        return NULL;

    uint8_t *parser = m_data;

    int skiplines = Max(y1 - y, 0); // number of lines to skip
    ysteps = Min(y2 - y, m_size.y - skiplines); // number of lines to draw
    y += skiplines; // first line to draw

    while (skiplines--)
    {
        for (int ix = 0; ix < m_size.x; )
        {
            ix += *parser++; // skip over empty space

            if (ix >= m_size.x)
                break;

            ix += *parser;
            parser += *parser + 1; // skip over data
        }
    }

    screen->AddDirty(Max(x, x1), y, Min(x + m_size.x, x2), y + m_size.y);
    return parser;
}

template<int N>
void trans_image::PutImageGeneric(image *screen, int x, int y, uint8_t color,
                                  image *blend, int bx, int by,
                                  uint8_t *remap, uint8_t *remap2,
                                  int amount, int total_frames,
                                  uint8_t *tint, color_filter *f, palette *pal)
{
    int x1, y1, x2, y2;
    int ysteps, mul = 0;

    screen->GetClip(x1, y1, x2, y2);

    if (N == SCANLINE)
    {
        y1 = Max(y1, y + amount);
        y2 = Min(y2, y + amount + 1);
        if (y1 >= y2)
            return;
    }

    uint8_t *datap = ClipToLine(screen, x1, y1, x2, y2, x, y, ysteps),
            *screen_line, *blend_line = NULL, *paddr = NULL;
    if (!datap)
        return; // if ClipToLine says nothing to draw, return

    CONDITION(N == BLEND && y >= by && y + ysteps < by + blend->Size().y + 1,
              "Blend doesn't fit on trans_image");

    if (N == FADE || N == FADE_TINT || N == BLEND)
        paddr = (uint8_t *)pal->addr();

    if (N == FADE || N == FADE_TINT)
        mul = (amount << 16) / total_frames;
    else if (N == BLEND)
        mul = ((16 - amount) << 16 / 16);

    if (N == PREDATOR)
        ysteps = Min(ysteps, y2 - 1 - y - 2);

    screen->Lock();

    screen_line = screen->scan_line(y) + x;
    int sw = screen->Size().x;
    x1 -= x; x2 -= x;

    for (; ysteps > 0; ysteps--, y++)
    {
        if (N == BLEND)
            blend_line = blend->scan_line(y - by);

        for (int ix = 0; ix < m_size.x; )
        {
            // Handle a run of transparent pixels
            int todo = *datap++;

            // FIXME: implement FILLED mode
            ix += todo;
            screen_line += todo;

            if (ix >= m_size.x)
                break;

            // Handle a run of solid pixels
            todo = *datap++;

            // Chop left side if necessary, but no more than todo
            int tochop = Min(todo, Max(x1 - ix, 0));

            ix += tochop;
            screen_line += tochop;
            datap += tochop;
            todo -= tochop;

            // Chop right side if necessary and process the remaining pixels
            int count = Min(todo, Max(x2 - ix, 0));

            if (N == NORMAL || N == SCANLINE)
            {
                memcpy(screen_line, datap, count);
            }
            else if (N == COLOR)
            {
                memset(screen_line, color, count);
            }
            else if (N == PREDATOR)
            {
                memcpy(screen_line, screen_line + 2 * m_size.x, count);
            }
            else if (N == REMAP)
            {
                uint8_t *sl = screen_line, *sl2 = datap;
                while (count--)
                    *sl++ = remap[*sl2++];
            }
            else if (N == REMAP2)
            {
                uint8_t *sl = screen_line, *sl2 = datap;
                while (count--)
                    *sl++ = remap2[remap[*sl2++]];
            }
            else if (N == FADE || N == FADE_TINT || N == BLEND)
            {
                uint8_t *sl = screen_line;
                uint8_t *sl2 = (N == BLEND) ? blend_line + x + ix - bx : sl;
                uint8_t *sl3 = datap;

                while (count--)
                {
                    uint8_t *p1 = paddr + 3 * *sl2++;
                    uint8_t *p2 = paddr + 3 * (N == FADE_TINT ? tint[*sl3++] : *sl3++);

                    uint8_t r = ((((int)p1[0] - p2[0]) * mul) >> 16) + p2[0];
                    uint8_t g = ((((int)p1[1] - p2[1]) * mul) >> 16) + p2[1];
                    uint8_t b = ((((int)p1[2] - p2[2]) * mul) >> 16) + p2[2];

                    *sl++ = f->lookup_color(r >> 3, g >> 3, b >> 3);
                }
            }

            datap += todo;
            ix += todo;
            screen_line += todo;
        }
        screen_line += sw - m_size.x;
    }
    screen->Unlock();
}

void trans_image::PutImage(image *screen, int x, int y)
{
    PutImageGeneric<NORMAL>(screen, x, y, 0, NULL, 0, 0, NULL, NULL,
                            0, 1, NULL, NULL, NULL);
}

void trans_image::PutRemap(image *screen, int x, int y, uint8_t *remap)
{
    PutImageGeneric<REMAP>(screen, x, y, 0, NULL, 0, 0, remap, NULL,
                           0, 1, NULL, NULL, NULL);
}

void trans_image::PutDoubleRemap(image *screen, int x, int y,
                                 uint8_t *remap, uint8_t *remap2)
{
    PutImageGeneric<REMAP2>(screen, x, y, 0, NULL, 0, 0, remap, remap2,
                            0, 1, NULL, NULL, NULL);
}

// Used when eg. the player teleports, or in rocket trails
void trans_image::PutFade(image *screen, int x, int y,
                          int amount, int total_frames,
                          color_filter *f, palette *pal)
{
    PutImageGeneric<FADE>(screen, x, y, 0, NULL, 0, 0, NULL, NULL,
                          amount, total_frames, NULL, f, pal);
}

void trans_image::PutFadeTint(image *screen, int x, int y,
                              int amount, int total_frames,
                              uint8_t *tint, color_filter *f, palette *pal)
{
    PutImageGeneric<FADE_TINT>(screen, x, y, 0, NULL, 0, 0, NULL, NULL,
                               amount, total_frames, tint, f, pal);
}

void trans_image::PutColor(image *screen, int x, int y, uint8_t color)
{
    PutImageGeneric<COLOR>(screen, x, y, color, NULL, 0, 0, NULL, NULL,
                           0, 1, NULL, NULL, NULL);
}

// This method is unused but is believed to work.
// Assumes that the blend image completely covers the transparent image.
void trans_image::PutBlend(image *screen, int x, int y,
                           image *blend, int bx, int by,
                           int amount, color_filter *f, palette *pal)
{
    PutImageGeneric<BLEND>(screen, x, y, 0, blend, bx, by, NULL, NULL,
                           amount, 1, NULL, f, pal);
}

void trans_image::PutFilled(image *screen, int x, int y, uint8_t color)
{
    PutImageGeneric<FILLED>(screen, x, y, color, NULL, 0, 0, NULL, NULL,
                            0, 1, NULL, NULL, NULL);
}

void trans_image::PutPredator(image *screen, int x, int y)
{
    PutImageGeneric<PREDATOR>(screen, x, y, 0, NULL, 0, 0, NULL, NULL,
                              0, 1, NULL, NULL, NULL);
}

void trans_image::PutScanLine(image *screen, int x, int y, int line)
{
    PutImageGeneric<SCANLINE>(screen, x, y, 0, NULL, 0, 0, NULL, NULL,
                              line, 1, NULL, NULL, NULL);
}

size_t trans_image::MemUsage()
{
    uint8_t *d = m_data;
    size_t ret = 0;

    for (int y = 0; y < m_size.y; y++)
    {
        for (int x = 0; x < m_size.x; x++)
        {
            x += *d++; ret++;

            if (x >= m_size.x)
                break;

            size_t run = *d++; ret += run + 1; d += run; x += run;
        }
    }
    return ret + sizeof(void *) + sizeof(vec2i);
}

