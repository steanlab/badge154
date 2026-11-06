#pragma once
#include <stdint.h>

/* Text on the tag screen: black background, red heading, white lines (5x7 font, doubled).
 * Image format as in slides.c: 200 x 200, 2 bits per pixel, 4 pixels per byte, MSB first;
 * 0 black, 1 white, 2 yellow, 3 red.  Upper case letters, digits and  space % ( ) + , - . / :  only. */
#define TXT_COLS  16  /* characters per line */
#define TXT_LINES 7   /* lines under the heading */

void textscr_draw(uint8_t *img, const char *heading, char (*lines)[TXT_COLS + 1], uint8_t n);
