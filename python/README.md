## Deprecated

This Python code is being phased out and replaced with https://github.com/rpi-ws281x/rpi-ws281x-python

If you're just looking to install the Python library, you can: `sudo pip install rpi_ws281x` or `sudo pip3 install rpi_ws281x` depending on your Python version of choice or find releases here: https://github.com/rpi-ws281x/rpi-ws281x-python/releases

For issues and bugs with (or contributions to) the Python library, please see: https://github.com/rpi-ws281x/rpi-ws281x-python/issues

----

## Build

As this is just a python wrapper for the library you must first follow
the build instructions in the parent directory.
When complete, you can build this python wrapper:
```
  sudo apt-get install python-dev swig
  python ./setup.py build
```


If you are rebuilding after fetching some updated commits, you might need to
remove the build directory first
```
  rm -rf ./build
```

## Install

If you want to install the library (in a virtualenv or in the system), so that you can `import neopixel` from anywhere, you need to run:

```
  python ./setup.py install
```

Depending on where you are installing, root privileges may be needed (prepend `sudo` to the install command).


## Run a demo

```
  sudo PYTHONPATH=".:build/lib.linux-armv7l-2.7" python examples/strandtest.py
```

If you installed the library, there is no need to specify `PYTHONPATH`:

```
  sudo python examples/strandtest.py
```
