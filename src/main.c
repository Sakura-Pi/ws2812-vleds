// SPDX-License-Identifier: GPL-2.0-only
// Copyright (c) 2025 Sakura Pi Org <kernel@sakurapi.org>

/*
* WS2812 virtual LED driver
*
* This driver uses SPI to drive WS2812 LEDs.
* Besides, it registers a standard LED control interface for controlling,
* dts-based RGB24 color preset and hsb brightness support.
*
* It uses 8 bits to simulate one WS2812 color bit, so this driver
* needs a 6.4MHz SPI to work properly.
*
* In some cases, you may calibrate the SPI speed manually.
* see 'ws2812.h' for more.
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/spi/spi.h>
#include <linux/of.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/mutex.h>

#include "ws2812.h"
#include "utils.h"

struct driver_data {
  int num_leds;

  struct {
    void* ptr;
    int length;
  } tx_buffer;

  ws2812_framebuf_t* ws_opctx;

  struct spi_device *spi;
  struct mutex mutex;

  struct list_head leds;
};

struct color24 {
  uint8_t r;
  uint8_t g;
  uint8_t b;
};

struct wsled_data {
  struct list_head list;
  struct led_classdev* cls;          // main LED device (for brightness control)
  struct led_classdev* cls_red;      // red channel LED device
  struct led_classdev* cls_green;    // green channel LED device
  struct led_classdev* cls_blue;     // blue channel LED device
  struct color24 color;              // calculated color
  struct color24 origin_color;       // origin color is readonly
  uint8_t lightness; // for HSL color space
};

enum filter_type {
  filter_main = 0,
  filter_red_ch,
  filter_green_ch,
  filter_blue_ch,
};

static int ws2812_vleds_update(struct driver_data* drv);

static void __set_lightness_color24(struct color24* src,
struct color24* dst, uint8_t lightness)
{
  if (!src || !dst) return;
  int h, s, l;
    
  // convert rgb to hsl
  rgb_to_hsl(src->r, src->g, src->b, &h, &s, &l);
  
  // agjust the lightness
  l = (l * lightness / 255);

  // convert back to rgb
  hsl_to_rgb(h, s, l, &dst->r, &dst->g, &dst->b);
}

static int __compare_set_brightness(struct led_classdev* led,
enum led_brightness bright, enum filter_type filter)
{
  struct driver_data* _drv_data = NULL; {
    _drv_data = (struct driver_data*)dev_get_drvdata(led->dev->parent);
    if(!_drv_data) {
      pr_info("failed to get drv context\n");
      return -ENODEV;
    }
  }

  int _index = 0, _ret = 0;
  struct wsled_data* _node = NULL;

  #define search_set_wsled_channel(cls, _do) \
    _index = 0; \
    list_for_each_entry(_node, &_drv_data->leds, list) { \
      if(_node->cls == led) _do \
      ++_index; \
    }

  switch(filter) {
    case filter_red_ch:
      search_set_wsled_channel(cls_red, {
        _node->color.r = bright;
        break;
      });
      break;
    case filter_green_ch:
      search_set_wsled_channel(cls_green, {
        _node->color.g = bright;
        break;
      });
      break;
    case filter_blue_ch:
      search_set_wsled_channel(cls_blue, {
        _node->color.b = bright;
        break;
      });
      break;
    case filter_main:
      list_for_each_entry(_node, &_drv_data->leds, list) {
        if(_node->cls == led) {
          _node->lightness = bright;
          __set_lightness_color24(&_node->origin_color, &_node->color, _node->lightness);
          break;
        }
      }
      break;
  }

  // update leds
  ws2812_set_pixel(_drv_data->ws_opctx, _index,
    ws2812_rgb(_node->color.r, _node->color.g, _node->color.b)); {
    _ret = ws2812_vleds_update(_drv_data);
  }

  return _ret;
}

static int __cb_set_wsled_red(struct led_classdev* led, enum led_brightness bright)
{
  return __compare_set_brightness(led, bright, filter_red_ch);
}

static int __cb_set_wsled_green(struct led_classdev* led, enum led_brightness bright)
{
  return __compare_set_brightness(led, bright, filter_green_ch);
}

static int __cb_set_wsled_blue(struct led_classdev* led, enum led_brightness bright)
{
  return __compare_set_brightness(led, bright, filter_blue_ch);
}

static int __cb_set_wsled(struct led_classdev* led, enum led_brightness bright)
{
  return __compare_set_brightness(led, bright, filter_main);
}

static int ws2812_vleds_get_lednum(struct device_node* node) {
  struct device_node* _entry = of_get_child_by_name(node, "leds"); {
    if(!_entry) return 0;

    int _counter = 0;
    struct device_node *child;
    for_each_child_of_node(_entry, child) ++_counter;

    return _counter;
  }
}

static int ws2812_vleds_update(struct driver_data* drv) {

  int _ret = 0;
  mutex_lock(&drv->mutex);
  _ret = spi_write(drv->spi, drv->tx_buffer.ptr, drv->tx_buffer.length);
  mutex_unlock(&drv->mutex);

  return _ret;
}

static int ws2812_vleds_probe(struct spi_device *spi)
{
  int ret;

  int _leds = ws2812_vleds_get_lednum(spi->dev.of_node); {
    if(_leds == 0) {
      dev_info(&spi->dev, "ws2812 found no leds under the controller, return.\n");
      return -ENODEV;
    }
  }

  // fill driver ctx
  struct driver_data* _drv_data; {
    _drv_data = devm_kzalloc(&spi->dev, sizeof(*_drv_data), GFP_KERNEL);
    if (!_drv_data) return -ENOMEM;

    // save spi dev
    _drv_data->spi = spi;

    // init list
    INIT_LIST_HEAD(&_drv_data->leds);

    // create mutex
    ret = devm_mutex_init(&spi->dev, &_drv_data->mutex);
    if (ret) return ret;

    // allocate tx buffer
    _drv_data->num_leds = _leds;
    _drv_data->tx_buffer.length = ws2812_calc_bufsize(_leds);
    _drv_data->tx_buffer.ptr = devm_kzalloc(&spi->dev, _drv_data->tx_buffer.length, GFP_KERNEL); {
      dev_info(&spi->dev, "ws2812 txbuf allocated: %p\n", _drv_data->tx_buffer.ptr);
      if (!_drv_data->tx_buffer.ptr) return -ENOMEM;
    }

    // wrap the buffer
    ws2812_init(&spi->dev, _leds, _drv_data->tx_buffer.ptr, &_drv_data->ws_opctx);
    dev_info(&spi->dev, "ws2812 init\n");

    dev_set_drvdata(&spi->dev, _drv_data);
    dev_info(&spi->dev, "drv data = %p\n", _drv_data);
  }

  // clear leds
  ws2812_vleds_update(_drv_data);

  struct device_node *_enrty, *child;

  _enrty = of_get_child_by_name(spi->dev.of_node, "leds");
  for_each_child_of_node(_enrty, child) {

    // led name
    const char* _label;
    if (of_property_read_string(child, "label", &_label)) {
      _label = child->name; // fallback to node name
      dev_warn(&spi->dev, "unamed led, fallback to %s\n", _label);
    }

    // Create main LED device
    struct led_classdev* _ledcls;
    _ledcls = devm_kzalloc(&spi->dev, sizeof(*_ledcls), GFP_KERNEL);
    if (!_ledcls) return -ENOMEM;

    // max brightness
    int _max_brightness;
    if (of_property_read_s32(child, "max_brightness", &_max_brightness)) {
      _max_brightness = 255; // fallback to 255
    }

    _ledcls->name = _label;
    _ledcls->brightness_set_blocking = __cb_set_wsled;
    _ledcls->max_brightness = _max_brightness;
    _ledcls->dev = &spi->dev;

    led_classdev_register(&spi->dev, _ledcls);
    dev_info(&spi->dev, "registering led: %s\n", _label);

    // Create RGB sub-devices for this LED
    struct led_classdev* _ledcls_red;
    struct led_classdev* _ledcls_green;
    struct led_classdev* _ledcls_blue;

    _ledcls_red = devm_kzalloc(&spi->dev, sizeof(*_ledcls_red), GFP_KERNEL);
    _ledcls_green = devm_kzalloc(&spi->dev, sizeof(*_ledcls_green), GFP_KERNEL);
    _ledcls_blue = devm_kzalloc(&spi->dev, sizeof(*_ledcls_blue), GFP_KERNEL);

    if (!_ledcls_red || !_ledcls_green || !_ledcls_blue) return -ENOMEM;

    // Create RGB sub-device names
    char* _red_name = devm_kasprintf(&spi->dev, GFP_KERNEL, "%s:red", _label);
    char* _green_name = devm_kasprintf(&spi->dev, GFP_KERNEL, "%s:green", _label);
    char* _blue_name = devm_kasprintf(&spi->dev, GFP_KERNEL, "%s:blue", _label);

    if (!_red_name || !_green_name || !_blue_name) return -ENOMEM;

    // Setup red channel LED
    _ledcls_red->name = _red_name;
    _ledcls_red->brightness_set_blocking = __cb_set_wsled_red;
    _ledcls_red->max_brightness = 255;
    _ledcls_red->dev = &spi->dev;

    // Setup green channel LED
    _ledcls_green->name = _green_name;
    _ledcls_green->brightness_set_blocking = __cb_set_wsled_green;
    _ledcls_green->max_brightness = 255;
    _ledcls_green->dev = &spi->dev;

    // Setup blue channel LED
    _ledcls_blue->name = _blue_name;
    _ledcls_blue->brightness_set_blocking = __cb_set_wsled_blue;
    _ledcls_blue->max_brightness = 255;
    _ledcls_blue->dev = &spi->dev;

    struct wsled_data* _ledctx;
    _ledctx = devm_kzalloc(&spi->dev, sizeof(*_ledctx), GFP_KERNEL); {
      if (!_ledctx) return -ENOMEM;

      // led default color
      const char* _color_value;
      uint8_t _color_r = 0xff, _color_g = 0xff, _color_b = 0xff;
      if (of_property_read_string(child, "color-value", &_color_value)) {
        dev_warn(&spi->dev, "use 0xffffff(white) as default led color\n");
      }

      // parse string color into int
      if(!hexclr_to_rgb888(_color_value, &_color_r, &_color_g, &_color_b)) {
        dev_warn(&spi->dev, "invalid led color format, use 0xffffff\n");
      }

      dev_info(&spi->dev, "led color %s = %d %d %d \n",_color_value,
        _color_r, _color_g, _color_b);

      _ledctx->cls = _ledcls;
      _ledctx->cls_red = _ledcls_red;
      _ledctx->cls_green = _ledcls_green;
      _ledctx->cls_blue = _ledcls_blue;
      _ledctx->origin_color.r = _color_r;
      _ledctx->origin_color.g = _color_g;
      _ledctx->origin_color.b = _color_b;
      // _ledctx->color = _ledctx->origin_color;
      _ledctx->lightness = 0;
      __set_lightness_color24(&_ledctx->origin_color, &_ledctx->color, _ledctx->lightness);

      // set initial brightness values for RGB sub-devices
      _ledcls_red->brightness = _color_r;
      _ledcls_green->brightness = _color_g;
      _ledcls_blue->brightness = _color_b;

      list_add_tail(&_ledctx->list, &_drv_data->leds);
    }

    // Register RGB sub-devices after setting initial values
    led_classdev_register(&spi->dev, _ledcls_red);
    led_classdev_register(&spi->dev, _ledcls_green);
    led_classdev_register(&spi->dev, _ledcls_blue);

    dev_info(&spi->dev, "registering rgb leds: %s, %s, %s\n", _red_name, _green_name, _blue_name);
  }

  return 0;
}

static void ws2812_vleds_remove(struct spi_device *spi) {
  struct driver_data* _drv_data = dev_get_drvdata(&spi->dev);
  struct wsled_data* _node, *_tmp;

  if (_drv_data) {
    list_for_each_entry_safe(_node, _tmp, &_drv_data->leds, list) {
      if (_node->cls) {
        led_classdev_unregister(_node->cls);
      }
      if (_node->cls_red) {
        led_classdev_unregister(_node->cls_red);
      }
      if (_node->cls_green) {
        led_classdev_unregister(_node->cls_green);
      }
      if (_node->cls_blue) {
        led_classdev_unregister(_node->cls_blue);
      }
      list_del(&_node->list);
    }
  }

  pr_info("virtual leds removed for %s\n", dev_name(&spi->dev));
}

static const struct of_device_id match_table[] = {
  { .compatible = "ws2812-vleds" },
  { /* end */ }
};
MODULE_DEVICE_TABLE(of, match_table);

static struct spi_driver ws2812_vleds_driver = {
  .probe = ws2812_vleds_probe,
  .remove = ws2812_vleds_remove,
  .driver = {
    .name = "ws2812-vleds",
    .of_match_table = match_table,
  },
};
module_spi_driver(ws2812_vleds_driver);

MODULE_AUTHOR("Sakura Pi Org <kernel@sakurapi.org>");
MODULE_DESCRIPTION("ws2812 SPI virtual LED driver");
MODULE_LICENSE("GPL v2");
