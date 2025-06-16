// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Sakura Pi Org <kernel@sakurapi.org>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/device.h>

#include "ws2812.h"

int ws2812_init(struct device* dev, int leds, void* buffer, ws2812_framebuf_t** frame) {

  if(!buffer) return -ENOMEM;

  ws2812_framebuf_t* _alloc = (ws2812_framebuf_t *)
    devm_kzalloc(dev, sizeof(ws2812_framebuf_t), GFP_KERNEL); {
    if (!_alloc) return -ENOMEM;
  }

  // prepare led buffer
  *frame = (ws2812_framebuf_t *)_alloc; {
    _alloc->pixel_count = leds;
    _alloc->buffer = buffer;
    _alloc->anchor.reset = (ws2812_color_t *)_alloc->buffer;
    _alloc->anchor.pixels = _alloc->anchor.reset + 4;
    _alloc->anchor.reset2 = _alloc->anchor.pixels + leds;
  }

  // the reset signals
  memset(_alloc->anchor.reset, ws2812_bit_zero, sizeof(ws2812_color_t) * 4);
  memset(_alloc->anchor.reset2, ws2812_bit_zero, sizeof(ws2812_color_t) * 4);

  // clear with black(all off)
  ws2812_clear(_alloc, ws2812_rgb(0, 0, 0));

  return 0;
}

void ws2812_clear(ws2812_framebuf_t* frame, ws2812_color_t color) {
  for(size_t i = 0; i < frame->pixel_count; ++i) {
    frame->anchor.pixels[i] = color;
  }
}

void ws2812_set_pixel(ws2812_framebuf_t* frame, int index, ws2812_color_t color) {
  frame->anchor.pixels[index] = color;
}
