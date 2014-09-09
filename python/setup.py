from ez_setup import use_setuptools
use_setuptools()
from setuptools import setup, find_packages, Extension

setup(name              = 'rpi_ws281x',
      version           = '1.0.0',
      author            = 'Jeremy Garff',
      author_email      = 'jer@jers.net',
      description       = 'Userspace Raspberry Pi PWM library for WS281X LEDs.',
      license           = 'MIT',
      url               = 'https://github.com/jgarff/rpi_ws281x/',
      ext_modules       = [Extension('_rpi_ws281x', 
                                     sources=['rpi_ws281x.i'],
                                     library_dirs=['/home/pi/rpi_ws281x'],
                                     libraries=['ws2811'])],
      packages          = find_packages())