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
 
 struct wsled_data {
   struct list_head list;
   struct led_classdev* cls;
 };
 
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
 
 static int w2812_lveds_set_brightness(struct led_classdev* led, enum led_brightness bright) {
   pr_info("wsled brightness set to %d\n", bright);
 
   struct driver_data* _drv_data = NULL; {
     _drv_data = (struct driver_data*)dev_get_drvdata(led->dev->parent);
     if(!_drv_data) {
       pr_info("failed to get drv context\n");
       return -ENODEV;
     }
   }
 
   pr_info("enum %p\n", led);
   pr_info("_ledcls->dev = %p\n", led->dev);
   pr_info("dev_get_drvdata = %p\n", dev_get_drvdata(led->dev));
   pr_info("dev_get_drvdata(_ledcls->dev->parent) = %p\n", dev_get_drvdata(led->dev->parent));
 
   int _index = 0, _ret = 0;
   struct wsled_data* _node;
 
   list_for_each_entry(_node, &_drv_data->leds, list) {
 
     pr_info("%p vs %p\n", _node->cls, led);
 
     // set brightness
     if(_node->cls == led) {
       pr_info("set wsled %p to %d\n", _node->cls, bright);
       ws2812_set_pixel(_drv_data->ws_opctx, _index, ws2812_rgb(bright, bright, bright)); {
         _ret = ws2812_vleds_update(_drv_data);
         break;
       }
     }
 
     ++_index;
   }
 
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
 
   // tested on rk3308b, 1.32us
   // spi->max_speed_hz = 6750000;
   // spi->max_speed_hz = 6750000;
   // ret = spi_setup(spi); {
   //   if(ret) {
   //     dev_info(&spi->dev, "failed to set spi speed to 6750000.\n");
   //     return -ENOMEM;
   //   }
   // }
 
   // clear leds
   ws2812_vleds_update(_drv_data);
   // dev_info(&spi->dev, "preparing vled\n");
 
   int _index = 0;
   struct device_node *_enrty, *child;
 
   _enrty = of_get_child_by_name(spi->dev.of_node, "leds");
   for_each_child_of_node(_enrty, child) {
 
     struct led_classdev* _ledcls;
 
     _ledcls = devm_kzalloc(&spi->dev, sizeof(*_ledcls), GFP_KERNEL); {
 
       const char* _label;
       if (!_ledcls) return -ENOMEM;
 
       // led name
       if (of_property_read_string(child, "label", &_label)) {
         _label = child->name; // fallback to node name
         dev_warn(&spi->dev, "unamed led, fallback to %s\n", _label);
       }
 
       // led default color
       if (of_property_read_string(child, "label", &_label)) {
         _label = child->name; // fallback to node name
         dev_warn(&spi->dev, "unamed led, fallback to %s\n", _label);
       }
 
       _ledcls->name = _label;
       _ledcls->brightness_set_blocking = w2812_lveds_set_brightness;
       _ledcls->max_brightness = 255;
       _ledcls->dev = &spi->dev;
 
       led_classdev_register(&spi->dev, _ledcls);
       dev_info(&spi->dev, "registering led: %s\n", _label);
     }
 
     struct wsled_data* _ledctx;
     _ledctx = devm_kzalloc(&spi->dev, sizeof(*_ledctx), GFP_KERNEL); {
       if (!_ledctx) return -ENOMEM;
       _ledctx->cls = _ledcls;
       list_add_tail(&_ledctx->list, &_drv_data->leds);
     }
   }
 
   return 0;
 }
 
 static void ws2812_vleds_remove(struct spi_device *spi) {
   pr_info("Virtual LEDs removed for %s\n", dev_name(&spi->dev));
 }
 
 static const struct of_device_id match_table[] = {
   { "ws2812-vleds", 0 },
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
 