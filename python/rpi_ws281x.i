// SWIG interface file to define rpi_ws281x library python wrapper.
// Author: Tony DiCola (tony@tonydicola.com)

// Define module name rpi_ws281x.  This will actually be imported under
// the name _rpi_ws281x following the SWIG & Python conventions.
%module rpi_ws281x

// Include standard SWIG types & array support for support of uint32_t
// parameters and arrays.
%include "stdint.i"
%include "carrays.i"

// Declare functions which will be exported as anything in the ws2811.h header.
%{
#include "../ws2811.h"
%}

// Have SWIG automatically generate functions to manipulate a uint32_t array
// of pixel data.  See http://www.swig.org/Doc1.3/Library.html#Library_carrays
// for details on the functions that will be generated.
%array_functions(uint32_t, led_data)

// Process ws2811.h header and export all included functions.
%include "../ws2811.h"
