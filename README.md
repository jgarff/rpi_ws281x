About this fork
===============

I decided to create this fork in order to maintain and merge the good parts
from the several existing forks of the original library by @jgarff.
This is mainly done to support the development of the node.js-addon
(`node-rpi-ws281x-native`) that uses this library.

rpi_ws281x
==========

Userspace Raspberry Pi PWM library for WS281X LEDs

Background:

The BCM2835 in the Raspberry Pi has a PWM module that is well suited to
driving individually controllable WS281X LEDs.  Using the DMA, PWM FIFO,
and serial mode in the PWM, it's possible to control almost any number
of WS281X LEDs in a chain connected to the PWM output pin.

This library and test program set the clock rate of the PWM controller to
3X the desired output frequency and creates a bit pattern in RAM from an
array of colors where each bit is represented by 3 bits for the PWM
controller as follows.

    Bit 1 - 1 1 0
    Bit 0 - 1 0 0


Hardware:

WS281X LEDs are generally driven at 5V, which requires that the data
signal be at the same level.  Converting the output from a Raspberry
Pi GPIO/PWM to a higher voltage through a level shifter is required.

It is possible to run the LEDs from a 3.3V - 3.6V power source, and
connect the GPIO directly at a cost of brightness, but this isn't
recommended.

The test program is designed to drive a 8x8 grid of LEDs from Adafruit
(http://www.adafruit.com/products/1487).  Please see the Adafruit
website for more information.

Know what you're doing with the hardware and electricity.  I take no
reponsibility for damage, harm, or mistakes.


Build:

- Install Scons (on raspbian, apt-get install scons).
- Make sure to adjust the parameters in main.c to suit your hardare.
  - Signal rate (400kHz to 800kHz).  Default 800kHz.
  - ledstring.invert=1 if using a inverting level shifter.
  - Width and height of LED matrix (height=1 for LED string).
- Type 'scons' from inside the source directory.


Running:

- Type 'sudo scons'.
- Type 'sudo ./test'.
- That's it.  You should see a moving rainbow scroll across the
  display.


Usage:

The API is very simple.  Make sure to create and initialize the ws2811_t
structure as seen in main.c.  From there it can be initialized
by calling ws2811_init().  LEDs are changed by modifying the color in
the .led[index] array and calling ws2811_render().  The rest is handled
by the library, which creates the DMA memory and starts the DMA/PWM.

Make sure to hook a signal handler for SIGKILL to do cleanup.  From the
handler make sure to call ws2811_fini().  It'll make sure that the DMA
is finished before program execution stops.

That's it.  Have fun.  This was a fun little weekend project.  I hope
you find it useful.  I plan to add some diagrams, waveform scope shots,
and a .deb package soon.

