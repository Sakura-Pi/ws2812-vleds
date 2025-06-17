// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Sakura Pi Org <kernel@sakurapi.org>

#ifndef _WS2812_H
#define _WS2812_H

#include <linux/types.h>

typedef struct {
  uint8_t g[8];
  uint8_t r[8];
  uint8_t b[8];
} ws2812_color_t;

typedef void* ws2812_ctx_t;

typedef enum {

  // TH + TL = 1.25us + ±600ns
  // 0 code: high voltage 0.35us, low 0.8us
  // 1 code: high voltage 0.7us,  low 0.6us

  // ws2812_bit_low
  //   _
  //  | |______
  //  1100 0000

  // ws2812_bit_high
  //   _____
  //  |     |__
  //  1111 1000

  // ws2812_bit_zero
  // for reset signal use
  //
  //  _________
  //  0000 0000

  // ws2812_bit_cali
  // this option causes spi output a 50% width clk
  // to help with the manually spi clk calibration
  //  ___
  // |   |____
  // 1111 0000

  ws2812_bit_low  = 0b11000000,
  ws2812_bit_high = 0b11111000,
  ws2812_bit_zero = 0b00000000,
  ws2812_bit_cali = 0b11110000

} ws2812_bit_t;

typedef struct {
  
  uint8_t* buffer;

  struct {

    /**
     * @brief reset signal is ahead of paload data
     * size 32 bytes
     */
    ws2812_color_t* reset;


    /**
     * @brief pixels
     */
    ws2812_color_t* pixels;

    /**
     * @brief reset signal is end of paload data
     * size 32 bytes
     */
     ws2812_color_t* reset2;

  } anchor;

  /**
   * @brief pixel count
   */
  int pixel_count;

} ws2812_framebuf_t;

#define __make_clrbit(_ch, _bit) \
  ((_ch & _bit) ? ws2812_bit_high : ws2812_bit_low)

#define __inflate_ch(_ch) { \
    __make_clrbit(_ch, 0b10000000), \
    __make_clrbit(_ch, 0b01000000), \
    __make_clrbit(_ch, 0b00100000), \
    __make_clrbit(_ch, 0b00010000), \
    __make_clrbit(_ch, 0b00001000), \
    __make_clrbit(_ch, 0b00000100), \
    __make_clrbit(_ch, 0b00000010), \
    __make_clrbit(_ch, 0b00000001) \
  }

int ws2812_init(struct device* dev, int leds, void* buffer, ws2812_framebuf_t** frame);
void ws2812_clear(ws2812_framebuf_t* frame, ws2812_color_t color);
void ws2812_set_pixel(ws2812_framebuf_t* frame, int index, ws2812_color_t color);

/*
  #define ws2812_rgb(_r, _g, _b) \
    ((ws2812_color_t) { \
      .b = __inflate_ch(_b), \
      .g = __inflate_ch(_g), \
      .r = __inflate_ch(_r) \
    })
*/

static ws2812_color_t ws2812_rgb(uint8_t r, uint8_t g, uint8_t b) {
  return ((ws2812_color_t) {
    .r = __inflate_ch(r),
    .g = __inflate_ch(g),
    .b = __inflate_ch(b),
  });
}

#define ws2812_calc_bufsize(leds) (sizeof(ws2812_color_t) * (8 + leds))

#endif /* _WS2812_H */
