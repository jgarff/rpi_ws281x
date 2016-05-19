# NeoPixel library strandtest example
# Author: Tony DiCola (tony@tonydicola.com)
#
# Direct port of the Arduino NeoPixel library strandtest example.  Showcases
# various animations on a strip of NeoPixels.
import time

from neopixel import *

# LED strip configuration:
LED_1_COUNT      = 16      # Number of LED pixels.
LED_1_PIN        = 18      # GPIO pin connected to the pixels (must support PWM!).
LED_1_FREQ_HZ    = 800000  # LED signal frequency in hertz (usually 800khz)
LED_1_DMA        = 5       # DMA channel to use for generating signal (try 5)
LED_1_BRIGHTNESS = 255     # Set to 0 for darkest and 255 for brightest
LED_1_INVERT     = False   # True to invert the signal (when using NPN transistor level shift)
LED_1_CHANNEL    = 0
LED_1_STRIP      = ws.SK6812_STRIP_GRBW	

LED_2_COUNT      = 16      # Number of LED pixels.
LED_2_PIN        = 13      # GPIO pin connected to the pixels (must support PWM!).
LED_2_FREQ_HZ    = 800000  # LED signal frequency in hertz (usually 800khz)
LED_2_DMA        = 10       # DMA channel to use for generating signal (try 5)
LED_2_BRIGHTNESS = 255     # Set to 0 for darkest and 255 for brightest
LED_2_INVERT     = False   # True to invert the signal (when using NPN transistor level shift)
LED_2_CHANNEL    = 1
LED_2_STRIP      = ws.WS2811_STRIP_GRB	

# Define functions which animate LEDs in various ways.
def colorWipe(strip, color, wait_ms=50):
	"""Wipe color across display a pixel at a time."""
	for i in range(strip.numPixels()):
		strip.setPixelColor(i, color)
		strip.show()
		time.sleep(wait_ms/1000.0)

def theaterChase(strip, color, wait_ms=50, iterations=10):
	"""Movie theater light style chaser animation."""
	for j in range(iterations):
		for q in range(3):
			for i in range(0, strip.numPixels(), 3):
				strip.setPixelColor(i+q, color)
			strip.show()
			time.sleep(wait_ms/1000.0)
			for i in range(0, strip.numPixels(), 3):
				strip.setPixelColor(i+q, 0)

def wheel(pos):
	"""Generate rainbow colors across 0-255 positions."""
	if pos < 85:
		return Color(pos * 3, 255 - pos * 3, 0)
	elif pos < 170:
		pos -= 85
		return Color(255 - pos * 3, 0, pos * 3)
	else:
		pos -= 170
		return Color(0, pos * 3, 255 - pos * 3)

def rainbow(strip, wait_ms=20, iterations=1):
	"""Draw rainbow that fades across all pixels at once."""
	for j in range(256*iterations):
		for i in range(strip.numPixels()):
			strip.setPixelColor(i, wheel((i+j) & 255))
		strip.show()
		time.sleep(wait_ms/1000.0)

def rainbowCycle(strip, wait_ms=20, iterations=5):
	"""Draw rainbow that uniformly distributes itself across all pixels."""
	for j in range(256*iterations):
		for i in range(strip.numPixels()):
			strip.setPixelColor(i, wheel(((i * 256 / strip.numPixels()) + j) & 255))
		strip.show()
		time.sleep(wait_ms/1000.0)

def theaterChaseRainbow(strip, wait_ms=50):
	"""Rainbow movie theater light style chaser animation."""
	for j in range(256):
		for q in range(3):
			for i in range(0, strip.numPixels(), 3):
				strip.setPixelColor(i+q, wheel((i+j) % 255))
			strip.show()
			time.sleep(wait_ms/1000.0)
			for i in range(0, strip.numPixels(), 3):
				strip.setPixelColor(i+q, 0)


# Main program logic follows:
if __name__ == '__main__':
	# Create NeoPixel object with appropriate configuration.
	strip1 = Adafruit_NeoPixel(LED_1_COUNT, LED_1_PIN, LED_1_FREQ_HZ, LED_1_DMA, LED_1_INVERT, LED_1_BRIGHTNESS, LED_1_CHANNEL, LED_1_STRIP)
	strip2 = Adafruit_NeoPixel(LED_2_COUNT, LED_2_PIN, LED_2_FREQ_HZ, LED_2_DMA, LED_2_INVERT, LED_2_BRIGHTNESS, LED_2_CHANNEL, LED_2_STRIP)

	# Intialize the library (must be called once before other functions).
	strip1.begin()
	strip2.begin()

	print ('Press Ctrl-C to quit.')
	while True:
		# Color wipe animations.
		colorWipe(strip1, Color(255, 0, 0))  # Red wipe - Strip #1
		colorWipe(strip2, Color(255, 0, 0))  # Red wipe - Strip #2
		colorWipe(strip1, Color(0, 255, 0))  # Blue wipe - Strip #1
		colorWipe(strip2, Color(0, 255, 0))  # Blue wipe - Strip #2
		colorWipe(strip1, Color(0, 0, 255))  # Green wipe - Strip #1
		colorWipe(strip2, Color(0, 0, 255))  # Green wipe - Strip #2
		colorWipe(strip1, Color(0, 0, 0, 255))  # White wipe - Strip #1
		colorWipe(strip2, Color(255, 0, 0))  # Red wipe - Strip #2
		colorWipe(strip1, Color(255, 255, 255))  # Composite White wipe - Strip #1
		colorWipe(strip2, Color(0, 255, 0))  # Blue wipe - Strip #2
		colorWipe(strip1, Color(255, 255, 255, 255))  # Composite White + White LED wipe - Strip #1
		colorWipe(strip2, Color(0, 0, 255))  # Green wipe - Strip #2
		# # Theater chase animations.
		# theaterChase(strip1, Color(127, 0, 0))  # Red theater chase
		# theaterChase(strip1, Color(0, 127, 0))  # Green theater chase
		# theaterChase(strip1, Color(0, 0, 127))  # Blue theater chase
		# theaterChase(strip1, Color(0, 0, 0, 127))  # White theater chase
		# theaterChase(strip1, Color(127, 127, 127, 0))  # Composite White theater chase
		# theaterChase(strip1, Color(127, 127, 127, 127))  # Composite White + White theater chase
		# theaterChase(strip2, Color(127, 0, 0))  # Red theater chase
		# theaterChase(strip2, Color(0, 127, 0))  # Green theater chase
		# theaterChase(strip2, Color(0, 0, 127))  # Blue theater chase
		# # Rainbow animations.
		# rainbow(strip)
		# rainbowCycle(strip)
		# theaterChaseRainbow(strip)
