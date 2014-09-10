# Adafruit NeoPixel library port to the rpi_ws281x library.
# Author: Tony DiCola (tony@tonydicola.com)
import _rpi_ws281x as ws


def Color(red, green, blue):
	"""Convert the provided red, green, blue color to a 24-bit color value.
	Each color component should be a value 0-255 where 0 is the lowest intensity
	and 255 is the highest intensity.
	"""
	return (red << 16) | (green << 8) | blue


class _LED_Data(object):
	"""Wrapper class which makes a SWIG LED color data array look and feel like
	a Python list of integers.
	"""
	def __init__(self, size):
		self.size = size
		self.data = ws.new_led_data(size)

	def __getitem__(self, pos):
		"""Return the 24-bit RGB color value at the provided position or slice
		of positions.
		"""
		# Handle if a slice of positions are passed in by grabbing all the values
		# and returning them in a list.
		if isinstance(pos, slice):
			return [ws.led_data_getitem(self.data, n) for n in range(pos.indices(self.size))]
		# Else assume the passed in value is a number to the position.
		else:
			return ws.led_data_getitem(self.data, pos)

	def __setitem__(self, pos, value):
		"""Set the 24-bit RGB color value at the provided position or slice of
		positions.
		"""
		# Handle if a slice of positions are passed in by setting the appropriate
		# LED data values to the provided values.
		if isinstance(pos, slice):
			index = 0
			for n in range(pos.indices(self.size)):
				ws.led_data_setitem(self.data, n, value[index])
				index += 1
		# Else assume the passed in value is a number to the position.
		else:
			return ws.led_data_setitem(self.data, pos, value)


class Adafruit_NeoPixel(object):
	def __init__(self, num, pin, freq_hz=800000, dma=5, invert=False):
		"""Class to represent a NeoPixel/WS281x LED display.  Num should be the
		number of pixels in the display, and pin should be the GPIO pin connected
		to the display signal line (must be a PWM pin like 18!).  Optional
		parameters are freq, the frequency of the display signal in hertz (default
		800khz), dma, the DMA channel to use (default 5), and invert, a boolean
		specifying if the signal line should be inverted (default False).
		"""
		# Create ws2811_t structure and fill in parameters.
		self._leds = ws.new_ws2811_t()
		ws.ws2811_t_count_set(self._leds, num)
		ws.ws2811_t_freq_set(self._leds, freq_hz)
		ws.ws2811_t_dmanum_set(self._leds, dma)
		ws.ws2811_t_gpionum_set(self._leds, pin)
		ws.ws2811_t_invert_set(self._leds, 0 if not invert else 1)
		# Create led data array.
		self._led_data = _LED_Data(num)

	def __del__(self):
		# Clean up memory used by the library when not needed anymore.
		if self._leds is not None:
			ws.ws2811_fini(self._leds)
			ws.delete_ws2811_t(self._leds)
			self._leds = None
			# Note that ws2811_fini will free the memory used by led_data internally.

	def begin(self):
		"""Initialize library, must be called once before other functions are
		called.
		"""
		resp = ws.ws2811_init(self._leds)
		if resp != 0:
			raise RuntimeError('ws2811_init failed with code {0}'.format(resp))
		# Set LED data array on the ws2811_t structure.  Be sure to do this AFTER
		# the init function is called as it clears out the LEDs.
		ws.ws2811_t_leds_set(self._leds, self._led_data.data)
		
	def show(self):
		"""Update the display with the data from the LED buffer."""
		resp = ws.ws2811_render(self._leds)
		if resp != 0:
			raise RuntimeError('ws2811_render failed with code {0}'.format(resp))

	def setPixelColor(self, n, color):
		"""Set LED at position n to the provided 24-bit color value (in RGB order).
		"""
		self._led_data[n] = color

	def setPixelColorRGB(self, n, red, green, blue):
		"""Set LED at position n to the provided red, green, and blue color.
		Each color component should be a value from 0 to 255 (where 0 is the
		lowest intensity and 255 is the highest intensity).
		"""
		self.setPixelColor(n, Color(red, green, blue))

	def setBrightness(self, brightness):
		"""Scale each LED in the buffer by the provided brightness.  A brightness
		of 0 is the darkest and 255 is the brightest.  Note that scaling can have
		quantization issues (i.e. blowing out to white or black) if used repeatedly!
		"""
		raise NotImplementedError

	def getPixels(self):
		"""Return an object which allows access to the LED display data as if 
		it were a sequence of 24-bit RGB values.
		"""
		return self._led_data

	def numPixels(self):
		"""Return the number of pixels in the display."""
		return ws.ws2811_t_count_get(self._leds)

	def getPixelColor(self, n):
		"""Get the 24-bit RGB color value for the LED at position n."""
		return self._led_data[n]
