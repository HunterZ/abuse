/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com or
 *  Jonathan Clark.
 */

#ifndef __AUTOMAP_HPP_
#define __AUTOMAP_HPP_

#include "jwindow.hpp"
#include "level.hpp"

class automap
{
  Jwindow *automap_window;
  level *cur_lev;
  int tick,w,h;                // used to draw your position as a blinking spot
  long old_dx,old_dy;
public :
  automap(level *l, int width, int height);
  void toggle_window();
  void handle_event(event &ev);
  void draw();
  ~automap() { if (automap_window) toggle_window(); }
} ;

extern automap *current_automap;

#endif

