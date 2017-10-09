# Build

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

# Run a demo

```
  sudo PYTHONPATH=".:build/lib.linux-armv7l-2.7" python examples/strandtest.py
```
