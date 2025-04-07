# WS2812 vleds

WS2812 virtual LED driver for mainline linux

[![meow](https://img.shields.io/badge/Sakura%20Pi%20🌸-Drivers-pink)](#)
[![meow](https://img.shields.io/badge/License-GNU%20GPLv2-blue)](#)

### Device Tree

```dts
&spi1 {

  status = "okay";

  ws2812@0 {

    compatible = "ws2812-vleds";
    reg = <0>;

    /* spi transmission clk
     * may need manually calbrate it
     */
    spi-max-frequency = <6400000>;

    leds {

      /* ws led 0 */
      ws_led0: vled0 {
        label = "ws-led0";
        color-value = "#eb698f";
      };

      /* more leds can define to here */
    };

  };
};

```

### Fine-tune the spi
In theory, the spi output speed is 6400000Hz. But in some cases (eg. level shifting, parasitic capacitance, different circuit designs), the default speed may not work. So you have to manually calibrate the output speed to fit with the ws2812 timing if you jump into this case.

``` bash
# TH + TL = 1.25us + ±600ns
# 0 code: high voltage 0.35us, low 0.8us
# 1 code: high voltage 0.7us,  low 0.6us

# the pulse of 0 code
 _
| |______
1100 0000

# the pulse of 1 code
 _____
|     |__
1111 1000
```

We use 8 bits to represent one ws2812 color bit(0 code and 1 code), which can cause 8 times memory usage. So the general formula in hz:

$$
freq = (\frac{1}{1.25 us / 1000000}) * 8
$$


To calibrate the spi output speed, we need a 50% width pulse which is `0b11110000`, increase or decrease the output speed, measure the valid voltage time of the high pulse (>= 3v [^note1]) is close to 0.625us.

[^note1]: ws2812 is not always the same as other ws2812s, you may refer to your manufacturer's user manual.

## LICENSE
Licensed under GPLv2
