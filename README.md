rpi_ws281x
==========

Userspace Raspberry Pi library for controlling WS281X LEDs.
This includes WS2812 and SK6812RGB RGB LEDs
Preliminary support is now included for SK6812RGBW LEDs (yes, RGB + W)
The LEDs can be controlled by either the PWM (2 independent channels)
or PCM controller (1 channel) or the SPI interfacei (1 channel).

### Background:

The BCM2835 in the Raspberry Pi has both a PWM and a PCM module that
are well suited to driving individually controllable WS281X LEDs.
Using the DMA, PWM or PCM FIFO, and serial mode in the PWM, it's
possible to control almost any number of WS281X LEDs in a chain connected
to the appropirate output pin.
For SPI the Raspbian spidev driver is used (/dev/spi0.0).
This library and test program set the clock rate to 3X the desired output
frequency and creates a bit pattern in RAM from an array of colors where
each bit is represented by 3 bits as follows.

    Bit 1 - 1 1 0
    Bit 0 - 1 0 0


### Hardware:

WS281X LEDs are generally driven at 5V, which requires that the data
signal be at the same level.  Converting the output from a Raspberry
Pi GPIO/PWM to a higher voltage through a level shifter is required.

It is possible to run the LEDs from a 3.3V - 3.6V power source, and
connect the GPIO directly at a cost of brightness, but this isn't
recommended.

The test program is designed to drive a 8x8 grid of LEDs e.g.from
Adafruit (http://www.adafruit.com/products/1487) or Pimoroni
(https://shop.pimoroni.com/products/unicorn-hat).
Please see the Adafruit and Pimoroni websites for more information.

Know what you're doing with the hardware and electricity.  I take no
reponsibility for damage, harm, or mistakes.

### Build:

- Install Scons (on raspbian, apt-get install scons).
- Make sure to adjust the parameters in main.c to suit your hardare.
  - Signal rate (400kHz to 800kHz).  Default 800kHz.
  - ledstring.invert=1 if using a inverting level shifter.
  - Width and height of LED matrix (height=1 for LED string).
- Type 'scons' from inside the source directory.

### Running:

- Type 'sudo scons'.
- Type 'sudo ./test' (default uses PWM channel 0).
- That's it.  You should see a moving rainbow scroll across the
  display.

### Limitations:

#### PWM

Since this library and the onboard Raspberry Pi audio
both use the PWM, they cannot be used together.  You will need to
blacklist the Broadcom audio kernel module by creating a file
/etc/modprobe.d/snd-blacklist.conf with

    blacklist snd_bcm2835

If the audio device is still loading after blacklisting, you may also
need to comment it out in the /etc/modules file.

Some distributions use audio by default, even if nothing is being played.
If audio is needed, you can use a USB audio device instead.

#### PCM

When using PCM you cannot use digital audio devices which use I2S since I2S
uses the PCM hardware, but you can use analog audio.

#### SPI

When using SPI the ledstring is the only device which can be connected to
the SPI bus. Both digital (I2S/PCM) and analog (PWM) audio can be used.

### Comparison PWM/PCM/SPI

Both PWM and PCM use DMA transfer to output the control signal for the LEDs.
The max size of a DMA transfer is 65536 bytes. SInce each LED needs 12 bytes
(4 colors, 8 symbols per color, 3 bits per symbol) this means you can
control approximately 5400 LEDs for a single strand in PCM and 2700 LEDs per string
for PWM (Only PWM can control 2 independent strings simultaneously)
SPI uses the SPI device driver in the kernel. For transfers larger than
96 bytes the kernel driver also uses DMA.
Of course there are practical limits on power and signal quality. These will
be more constraining in practice than the theoretical limits above.

When controlling a LED string of 240 LEDs the CPU load on the original Pi 2 (BCM2836) are:
  PWM  5%
  PCM  5%
  SPI  1%

### Usage:

The API is very simple.  Make sure to create and initialize the ws2811_t
structure as seen in main.c.  From there it can be initialized
by calling ws2811_init().  LEDs are changed by modifying the color in
the .led[index] array and calling ws2811_render().
The rest is handled by the library, which either creates the DMA memory and
starts the DMA for PWM and PCM or prepares the SPI transfer buffer and sends
it out on the MISO pin.

Make sure to hook a signal handler for SIGKILL to do cleanup.  From the
handler make sure to call ws2811_fini().  It'll make sure that the DMA
is finished before program execution stops and cleans up after itself.
