/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com or
 *  Jonathan Clark.
 */

#ifndef __JOYSTICK_HPP_
#define __JOYSTICK_HPP_

int joy_init(int argc, char **argv);              // returns 0 if joy stick not avaible
void joy_status(int &b1, int &b2, int &b3, int &xv, int &yv);
void joy_calibrate();

#endif
