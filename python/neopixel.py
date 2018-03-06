# Adafruit NeoPixel library port to the rpi_ws281x library.
# Author: Tony DiCola (tony@tonydicola.com), Jeremy Garff (jer@jers.net)
import atexit

import _rpi_ws281x as ws


def Color(red, green, blue, white = 0):
	"""Convert the provided red, green, blue color to a 24-bit color value.
	Each color component should be a value 0-255 where 0 is the lowest intensity
	and 255 is the highest intensity.
	"""
	return (white << 24) | (red << 16)| (green << 8) | blue


class _LED_Data(object):
	"""Wrapper class which makes a SWIG LED color data array look and feel like
	a Python list of integers.
	"""
	def __init__(self, channel, size):
		self.size = size
		self.channel = channel

	def __getitem__(self, pos):
		"""Return the 24-bit RGB color value at the provided position or slice
		of positions.
		"""
		# Handle if a slice of positions are passed in by grabbing all the values
		# and returning them in a list.
		if isinstance(pos, slice):
			return [ws.ws2811_led_get(self.channel, n) for n in xrange(*pos.indices(self.size))]
		# Else assume the passed in value is a number to the position.
		else:
			return ws.ws2811_led_get(self.channel, pos)

	def __setitem__(self, pos, value):
		"""Set the 24-bit RGB color value at the provided position or slice of
		positions.
		"""
		# Handle if a slice of positions are passed in by setting the appropriate
		# LED data values to the provided values.
		if isinstance(pos, slice):
			index = 0
			for n in xrange(*pos.indices(self.size)):
				ws.ws2811_led_set(self.channel, n, value[index])
				index += 1
		# Else assume the passed in value is a number to the position.
		else:
			return ws.ws2811_led_set(self.channel, pos, value)


class Adafruit_NeoPixel(object):
	def __init__(self, num, pin, freq_hz=800000, dma=10, invert=False,
			brightness=255, channel=0, strip_type=ws.WS2811_STRIP_RGB):
		"""Class to represent a NeoPixel/WS281x LED display.  Num should be the
		number of pixels in the display, and pin should be the GPIO pin connected
		to the display signal line (must be a PWM pin like 18!).  Optional
		parameters are freq, the frequency of the display signal in hertz (default
		800khz), dma, the DMA channel to use (default 10), invert, a boolean
		specifying if the signal line should be inverted (default False), and
		channel, the PWM channel to use (defaults to 0).
		"""
		# Create ws2811_t structure and fill in parameters.
		self._leds = ws.new_ws2811_t()

		# Initialize the channels to zero
		for channum in range(2):
			chan = ws.ws2811_channel_get(self._leds, channum)
			ws.ws2811_channel_t_count_set(chan, 0)
			ws.ws2811_channel_t_gpionum_set(chan, 0)
			ws.ws2811_channel_t_invert_set(chan, 0)
			ws.ws2811_channel_t_brightness_set(chan, 0)

		# Initialize the channel in use
		self._channel = ws.ws2811_channel_get(self._leds, channel)
		ws.ws2811_channel_t_count_set(self._channel, num)
		ws.ws2811_channel_t_gpionum_set(self._channel, pin)
		ws.ws2811_channel_t_invert_set(self._channel, 0 if not invert else 1)
		ws.ws2811_channel_t_brightness_set(self._channel, brightness)
		ws.ws2811_channel_t_strip_type_set(self._channel, strip_type)

		# Initialize the controller
		ws.ws2811_t_freq_set(self._leds, freq_hz)
		ws.ws2811_t_dmanum_set(self._leds, dma)

		# Grab the led data array.
		self._led_data = _LED_Data(self._channel, num)

		# Substitute for __del__, traps an exit condition and cleans up properly
		atexit.register(self._cleanup)

	def _cleanup(self):
		# Clean up memory used by the library when not needed anymore.
		if self._leds is not None:
			ws.delete_ws2811_t(self._leds)
			self._leds = None
			self._channel = None

	def begin(self):
		"""Initialize library, must be called once before other functions are
		called.
		"""
		resp = ws.ws2811_init(self._leds)
		if resp != ws.WS2811_SUCCESS:
			message = ws.ws2811_get_return_t_str(resp)
			raise RuntimeError('ws2811_init failed with code {0} ({1})'.format(resp, message))

	def show(self):
		"""Update the display with the data from the LED buffer."""
		resp = ws.ws2811_render(self._leds)
		if resp != ws.WS2811_SUCCESS:
			message = ws.ws2811_get_return_t_str(resp)
			raise RuntimeError('ws2811_render failed with code {0} ({1})'.format(resp, message))

	def setPixelColor(self, n, color):
		"""Set LED at position n to the provided 24-bit color value (in RGB order).
		"""
		self._led_data[n] = color

	def setPixelColorRGB(self, n, red, green, blue, white = 0):
		"""Set LED at position n to the provided red, green, and blue color.
		Each color component should be a value from 0 to 255 (where 0 is the
		lowest intensity and 255 is the highest intensity).
		"""
		self.setPixelColor(n, Color(red, green, blue, white))

	def setBrightness(self, brightness):
		"""Scale each LED in the buffer by the provided brightness.  A brightness
		of 0 is the darkest and 255 is the brightest.
		"""
		ws.ws2811_channel_t_brightness_set(self._channel, brightness)

	def getBrightness(self):
		"""Get the brightness value for each LED in the buffer. A brightness
		of 0 is the darkest and 255 is the brightest.
		"""
		return ws.ws2811_channel_t_brightness_get(self._channel)

	def getPixels(self):
		"""Return an object which allows access to the LED display data as if
		it were a sequence of 24-bit RGB values.
		"""
		return self._led_data

	def numPixels(self):
		"""Return the number of pixels in the display."""
		return ws.ws2811_channel_t_count_get(self._channel)

	def getPixelColor(self, n):
		"""Get the 24-bit RGB color value for the LED at position n."""
		return self._led_data[n]
