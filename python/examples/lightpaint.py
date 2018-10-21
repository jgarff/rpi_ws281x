#!/usr/bin/env python3
# NeoPixel library light-painting example
# Author: Gary Servin (garyservin@gmail.com)
#
# Lightpainting example for displaying images one column at a time and capturing
# it by taking a long exposure photograph.
# Based on https://github.com/scottjgibson/PixelPi

import time
from neopixel import *
import argparse

# Button
import RPi.GPIO as GPIO

# Lightpainting
from PIL import Image

# LED strip configuration:
LED_COUNT      = 128     # Number of LED pixels.
LED_PIN        = 18      # GPIO pin connected to the pixels (18 uses PWM!).
#LED_PIN        = 10      # GPIO pin connected to the pixels (10 uses SPI /dev/spidev0.0).
LED_FREQ_HZ    = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA        = 10      # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 255     # Set to 0 for darkest and 255 for brightest
LED_INVERT     = False   # True to invert the signal (when using NPN transistor level shift)
LED_CHANNEL    = 0       # set to '1' for GPIOs 13, 19, 41, 45 or 53

BUTTON_CHANNEL = 19     # GPIO pin connected to the start button

def UnColor(color):
    return ((color >> 24) & 0xFF , (color >> 16) & 0xFF, (color >> 8) & 0xFF,  color & 0xFF)

def lightpaint(filename, frame_rate=100, column_rate=1, reverse_x=False, reverse_y=False, loop=False):
    img = Image.open(filename).convert("RGB")

    # Check that the height of the image is greater than or equal the number of LEDs on the strip
    if(img.size[1] < LED_COUNT):
        raise Exception("Image height is smaller than led strip size. Required height = {}".format(LED_COUNT))
    elif(img.size[1] > LED_COUNT):
        print "Resizing image"
        new_width  = LED_COUNT * img.size[0] / img.size[1]
        img = img.resize((new_width, LED_COUNT), Image.ANTIALIAS)

    input_image = img.load()
    image_width = img.size[0]

    column = [0 for x in range(image_width)]
    for x in range(image_width):
        column[x] = [None] * (LED_COUNT)

    for x in range(image_width):
        for y in range(LED_COUNT):
            value = input_image[x, y]
            column[x][y] = Color(value[1], value[0], value[2])

    while True:
        # Wait for button to be pressed before displaying image
        if not loop:
            print("Waiting for button to be pressed")
            GPIO.wait_for_edge(BUTTON_CHANNEL, GPIO.FALLING)
            time.sleep(0.5)

        x_range = range(image_width)
        if reverse_x:
            x_range.reverse()

        y_range = range(LED_COUNT)
        if reverse_y:
            y_range.reverse()

        for x in x_range:
            led_pos = 0
            for y in y_range:
                strip.setPixelColor(led_pos, column[x][y])
                led_pos += 1
            strip.show()
            time.sleep(column_rate / 1000.0)

        # Wait for `frame_rate` ms before drawing a new frame
        time.sleep(frame_rate / 1000.0)

# Define functions which animate LEDs in various ways.
def colorWipe(strip, color, wait_ms=50):
    """Wipe color across display a pixel at a time."""
    for i in range(strip.numPixels()):
        strip.setPixelColor(i, color)
        strip.show()
        time.sleep(wait_ms/1000.0)

# Main program logic follows:
if __name__ == '__main__':
    # Process arguments
    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--clear', action='store_true', help='clear the display on exit')
    parser.add_argument('-f', '--file', action='store', help='Filename to display')
    parser.add_argument('-r', '--frame_rate', action='store', default=100, help='Rate between each frame. Defines how fast a new frame is displayed')
    parser.add_argument('-l', '--column_rate', action='store', default=1, help='Rate between each column. Defines how fast or slow you need to move the stick')
    parser.add_argument('-b', '--brightness', action='store', default=10, help='Brightness of the LED. Set to 0 for darkest and 255 for brightest')
    parser.add_argument('-x', '--reverse_x', action='store_true', help='Reverse the image in the X direction')
    parser.add_argument('-y', '--reverse_y', action='store_true', help='Reverse the image in the Y direction')
    parser.add_argument('--loop', action='store_true', help='Play frames in a loop')
    args = parser.parse_args()

    # Create NeoPixel object with appropriate configuration.
    strip = Adafruit_NeoPixel(LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, args.brightness, LED_CHANNEL)
    # Intialize the library (must be called once before other functions).
    strip.begin()

    # Button
    GPIO.setmode(GPIO.BOARD)
    GPIO.setup(BUTTON_CHANNEL, GPIO.IN, pull_up_down=GPIO.PUD_UP)

    print ('Press Ctrl-C to quit.')
    if not args.clear:
        print('Use "-c" argument to clear LEDs on exit')

    try:
        while True:
            print ('Lightpaint.')
            lightpaint(args.file, float(args.frame_rate), float(args.column_rate), args.reverse_x, args.reverse_y, args.loop)

    except KeyboardInterrupt:
        if args.clear:
            colorWipe(strip, Color(0,0,0), 10)
        GPIO.cleanup()
