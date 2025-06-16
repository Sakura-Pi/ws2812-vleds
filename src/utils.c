// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Sakura Pi Org <kernel@sakurapi.org>

#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/types.h>

/**
* convert 8bits hex string into int8
* @param c char
*/
static uint8_t __hex2int8(const char* c)
{
  uint8_t _value = 0;
  char _wrap[3] = {c[0], c[1], '\0'};
  kstrtou8(_wrap, 16, &_value);
  return _value;
}

bool hexclr_validate(const char* hex_color)
{
  if(!hex_color || (*hex_color != '#'))
    return false;

  if(strlen(hex_color) != 7)
    return false;

  return true;
}

bool hexclr_to_rgb888(const char* hex_color, uint8_t* r, uint8_t* g, uint8_t* b)
{
  if(!hexclr_validate(hex_color))
    return false;

  // accept color format like #AABBCC
  if(r) *r = __hex2int8(hex_color+1);
  if(g) *g = __hex2int8(hex_color+3);
  if(b) *b = __hex2int8(hex_color+5);

  return true;
}

static uint8_t hsl_to_rgb_component(int p, int q, int t)
{
    if (t < 0) t += 255;
    if (t > 255) t -= 255;
    if (t < 42) return p + ((q - p) * 6 * t) / 255;
    if (t < 128) return q;
    if (t < 170) return p + ((q - p) * (170 - t) * 6) / 255;
    return p;
}

void rgb_to_hsl(uint8_t r, uint8_t g, uint8_t b, int *h, int *s, int *l)
{
    int max_val = max(r, max(g, b));
    int min_val = min(r, min(g, b));
    int delta = max_val - min_val;
    
    // Lightness (0-255)
    *l = (max_val + min_val) / 2;
    
    if (delta == 0) {
        *h = 0;
        *s = 0;
        return;
    }
    
    // Saturation (0-255)
    if (*l < 128)
        *s = (delta * 255) / (max_val + min_val);
    else
        *s = (delta * 255) / (510 - max_val - min_val);
    
    // Hue (0-359)
    if (max_val == r)
        *h = ((g - b) * 60) / delta;
    else if (max_val == g)
        *h = 120 + ((b - r) * 60) / delta;
    else
        *h = 240 + ((r - g) * 60) / delta;
    
    if (*h < 0) *h += 360;
}

void hsl_to_rgb(int h, int s, int l, uint8_t *r, uint8_t *g, uint8_t *b)
{
    if (s == 0) {
        *r = *g = *b = l;
        return;
    }
    
    int q = (l < 128) ? (l * (255 + s)) / 255 : l + s - (l * s) / 255;
    int p = 2 * l - q;
    
    int h_norm = (h * 255) / 360;
    
    *r = hsl_to_rgb_component(p, q, h_norm + 85);
    *g = hsl_to_rgb_component(p, q, h_norm);
    *b = hsl_to_rgb_component(p, q, h_norm - 85);
}

