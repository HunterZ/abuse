/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com or
 *  Jonathan Clark.
 */

#ifndef _VIDEO_HPP_
#define _VIDEO_HPP_
#include "system.h"

#define TRI_1024x768x256 0x62
#define TRI_800x600x256  0x5e
#define TRI_640x480x256  0x5c
#define VGA_320x200x256  0x13
#define CGA_640x200x2    6
#define XWINDOWS_256     256
#define XWINDOWS_2       2

#include "image.h"


extern unsigned char current_background;
extern int xres,yres;
extern int xoff,yoff;
extern image *screen;

void set_mode(int mode, int argc=0, char **argv=NULL);
void close_graphics();
void fill_image(image *im, int x1, int y1, int x2, int y2);
void update_dirty(image *im, int xoff=0, int yoff=0);


void clear_put_image(image *im, int x, int y);
int get_vmode();

#endif
