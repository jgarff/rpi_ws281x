## Deprecated

This Go code is being phased out and replaced with https://github.com/rpi-ws281x/rpi-ws281x-go

For issues and bugs with (or contributions to) the Go library, please see: https://github.com/rpi-ws281x/rpi-ws281x-go/issues

----

## Run a demo

As this is just a Go wrapper for the library you must clone this into your `$GOPATH` as you would any other Go program. 
Your path to the project should be:
```
  $GOPATH/src/github.com/jgraff/rpi_ws281x
```


As listed in the `ws2811.go` file ensure to copy `ws2811.h`, `rpihw.h`, and `pwm.h` in a GCC include path (e.g. `/usr/local/include`) and
`libws2811.a` in a GCC library path (e.g. `/usr/local/lib`).

To run the basic example run the following commands:
```
  cd golang/examples
  go build basic.go
  sudo ./basic
```

If everything worked you should see a basic color wipe for the first 16 LEDs on your strip.
