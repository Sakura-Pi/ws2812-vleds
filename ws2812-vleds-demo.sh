#!/bin/bash

modprobe ws2812-vleds

LED_BASE="/sys/class/leds"
LEDS=("0" "1" "3" "2")
COLORS=("red" "green" "blue")
TOTAL_STEPS=25
STEP_DELAY=0.005
MAX_BRIGHTNESS=40
COLOR_CYCLE_STEPS=100

all_off() {
    for led in "${LEDS[@]}"; do
        for color in "${COLORS[@]}"; do
            echo 0 > "$LED_BASE/ws-led${led}:${color}/brightness"
        done
    done
}

set_brightness() {
    local led=$1
    local color=$2
    local value=$3
    [ $value -gt $MAX_BRIGHTNESS ] && value=$MAX_BRIGHTNESS
    [ $value -lt 0 ] && value=0
    echo $value > "$LED_BASE/ws-led${led}:${color}/brightness"
}

set_led_color_mix() {
    local led=$1
    local color_step=$2
    local brightness=$3
    [ $brightness -gt $MAX_BRIGHTNESS ] && brightness=$MAX_BRIGHTNESS
    [ $brightness -lt 0 ] && brightness=0

    local r=0
    local g=0
    local b=0

    local segment=$((color_step % (COLOR_CYCLE_STEPS * 3)))

    if [ $segment -lt $COLOR_CYCLE_STEPS ]; then
        r=$((255 - (255 * segment / COLOR_CYCLE_STEPS)))
        g=$((255 * segment / COLOR_CYCLE_STEPS))
        b=0
    elif [ $segment -lt $((COLOR_CYCLE_STEPS * 2)) ]; then
        local step_in_segment=$((segment - COLOR_CYCLE_STEPS))
        g=$((255 - (255 * step_in_segment / COLOR_CYCLE_STEPS)))
        b=$((255 * step_in_segment / COLOR_CYCLE_STEPS))
        r=0
    else
        local step_in_segment=$((segment - COLOR_CYCLE_STEPS * 2))
        b=$((255 - (255 * step_in_segment / COLOR_CYCLE_STEPS)))
        r=$((255 * step_in_segment / COLOR_CYCLE_STEPS))
        g=0
    fi

    r=$((r * MAX_BRIGHTNESS / 255))
    g=$((g * MAX_BRIGHTNESS / 255))
    b=$((b * MAX_BRIGHTNESS / 255))

    r=$((r * brightness / MAX_BRIGHTNESS))
    g=$((g * brightness / MAX_BRIGHTNESS))
    b=$((b * brightness / MAX_BRIGHTNESS))

    set_brightness $led "red" $r
    set_brightness $led "green" $g
    set_brightness $led "blue" $b
}

marquee() {
    local delay=${1:-0.05}
    all_off

    declare -a brightness=()
    declare -a color_step=()
    for ((i=0; i<${#LEDS[@]}; i++)); do
        brightness[$i]=0
        color_step[$i]=$((i * COLOR_CYCLE_STEPS * 3 / ${#LEDS[@]}))
    done

    local color_speed=2

    while true; do
        for ((i=0; i<${#LEDS[@]}; i++)); do
            current=$i
            prev=$(( (i-1 + ${#LEDS[@]}) % ${#LEDS[@]} ))
            
            current_led=${LEDS[$current]}
            prev_led=${LEDS[$prev]}
            
            current_start=${brightness[$current]}
            prev_start=${brightness[$prev]}
            
            for ((step=0; step<=TOTAL_STEPS; step++)); do
                current_val=$(( current_start + (MAX_BRIGHTNESS - current_start) * step / TOTAL_STEPS ))
                prev_val=$(( prev_start - prev_start * step / TOTAL_STEPS ))
                
                [ $current_val -gt $MAX_BRIGHTNESS ] && current_val=$MAX_BRIGHTNESS
                [ $prev_val -lt 0 ] && prev_val=0
                
                color_step[$current]=$(( (color_step[$current] + color_speed) % (COLOR_CYCLE_STEPS * 3) ))
                color_step[$prev]=$(( (color_step[$prev] + color_speed) % (COLOR_CYCLE_STEPS * 3) ))
                
                set_led_color_mix $current_led ${color_step[$current]} $current_val
                
                set_led_color_mix $prev_led ${color_step[$prev]} $prev_val
                
                sleep $STEP_DELAY
            done
            
            brightness[$current]=$MAX_BRIGHTNESS
            brightness[$prev]=0
            
            set_led_color_mix $current_led ${color_step[$current]} $MAX_BRIGHTNESS
            set_led_color_mix $prev_led ${color_step[$prev]} 0
            
            sleep $delay
        done
    done
}

trap 'all_off; exit 0' SIGINT

marquee 0.05
