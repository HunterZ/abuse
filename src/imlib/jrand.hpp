/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com or
 *  Jonathan Clark.
 */

#ifndef __JRAND_HPP_
#define __JRAND_HPP_

#define RAND_TABLE_SIZE 1024
extern unsigned short rtable[RAND_TABLE_SIZE];     // can be used directly when
extern unsigned short rand_on;                     // speed is of essence

void jrand_init();
inline unsigned short jrand() { return rtable[(rand_on++)&(RAND_TABLE_SIZE-1)]; }
#define jrandom(x) (jrand()%(x))

#endif
