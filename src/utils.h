// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Sakura Pi Org <kernel@sakurapi.org>

#ifndef _UTILS_H
#define _UTILS_H

bool hexclr_to_rgb888(const char* hex_color, uint8_t* r, uint8_t* g, uint8_t* b);
bool hexclr_validate(const char* hex_color);
void rgb_to_hsl(uint8_t r, uint8_t g, uint8_t b, int *h, int *s, int *l);
void hsl_to_rgb(int h, int s, int l, uint8_t *r, uint8_t *g, uint8_t *b);

#endif /* _UTILS_H */
