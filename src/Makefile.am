# Set the name of the library.
lib_LTLIBRARIES = libws2811.la

# Set the source files for the library.
libws2811_la_SOURCES = \
    mailbox.c \
    ws2811.c \
    pwm.c \
    pcm.c \
    dma.c \
    rpihw.c

# Set the public headers to be installed.
pkgincludedir = $(includedir)/ws2811
pkginclude_HEADERS = \
    ws2811.h \
    rpihw.h \
    pwm.h \
    clk.h \
    dma.h \
    gpio.h \
    mailbox.h \
    pcm.h

# Set build flags using AM_CFLAGS, which will be combined with the user's CFLAGS.
if BUILD_DEBUG
AM_CFLAGS = -g -Wall
else
AM_CFLAGS = -O2 -Wall
endif

# Define the test executable, which is built conditionally.
if BUILD_TEST
bin_PROGRAMS = test
test_SOURCES = main.c
test_LDADD = libws2811.la
endif

# Define the installation of the pkg-config file.
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libws2811.pc

# Define how to clean up the build directory.
CLEANFILES = libws2811.pc
