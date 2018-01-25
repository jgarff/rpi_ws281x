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
