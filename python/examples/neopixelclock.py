# Based on NeoPixel library and strandtest example by Tony DiCola (tony@tonydicola.com)
# To be used with a 12x1 NeoPixel LED stripe.
# Place the LEDs in a circle an watch the time go by ...
# red = hours
# blue = minutes 1-5
# green = seconds
# (To run the program permanently and with autostart use systemd.)

import time
import datetime
import math

from neopixel import *

# LED strip configuration:
LED_COUNT = 12      # Number of LED pixels.
LED_PIN = 18      # GPIO pin connected to the pixels (must support PWM!).
LED_FREQ_HZ = 800000  # LED signal frequency in hertz (usually 800khz)
LED_DMA = 10       # DMA channel to use for generating signal (try 10)
LED_BRIGHTNESS = 255  # Set to 0 for darkest and 255 for brightest
# True to invert the signal (when using NPN transistor level shift)
LED_INVERT = False

# Main program logic follows:
if __name__ == '__main__':
    # Create NeoPixel object with appropriate configuration.
    strip = Adafruit_NeoPixel(
        LED_COUNT, LED_PIN, LED_FREQ_HZ, LED_DMA, LED_INVERT, LED_BRIGHTNESS)
    # Intialize the library (must be called once before other functions).
    strip.begin()

    for i in range(0, strip.numPixels(), 1):
        strip.setPixelColor(i, Color(0, 0, 0))
    while True:
        now = datetime.datetime.now()

        # Low light during 19-8 o'clock
        if(8 < now.hour < 19):
            strip.setBrightness(200)
        else:
            strip.setBrightness(25)

        hour = now.hour % 12
        minute = now.minute / 5
        second = now.second / 5
        secondmodulo = now.second % 5
        timeslot_in_microseconds = secondmodulo * 1000000 + now.microsecond
        for i in range(0, strip.numPixels(), 1):
            secondplusone = second + 1 if(second < 11) else 0
            secondminusone = second - 1 if(second > 0) else 11
            colorarray = [0, 0, 0]

            if i == second:
                if timeslot_in_microseconds < 2500000:
                    colorarray[0] = int(
                        0.0000508 * timeslot_in_microseconds) + 126
                else:
                    colorarray[0] = 382 - \
                        int(0.0000508 * timeslot_in_microseconds)
            if i == secondplusone:
                colorarray[0] = int(0.0000256 * timeslot_in_microseconds)
            if i == secondminusone:
                colorarray[0] = int(
                    0.0000256 * timeslot_in_microseconds) * -1 + 128
            if i == minute:
                colorarray[2] = 200
            if i == hour:
                colorarray[1] = 200
            strip.setPixelColor(
                i, Color(colorarray[0], colorarray[1], colorarray[2]))
        strip.show()
        time.sleep(0.1)
