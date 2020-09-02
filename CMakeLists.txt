cmake_minimum_required(VERSION 3.0.0)

project(rpi_ws281x VERSION 1.1.0)

option(BUILD_SHARED "Build as shared library" OFF)

set(LIB_TARGET ws2811)

set(LIB_PUBLIC_HEADERS
    ws2811.h
    rpihw.h
    pwm.h
    clk.h
    dma.h
    gpio.h
    mailbox.h
    pcm.h
)

set(LIB_SOURCES
    mailbox.c
    ws2811.c
    pwm.c
    pcm.c
    dma.c
    rpihw.c
)

if(BUILD_SHARED) 
    add_library(${LIB_TARGET} SHARED ${LIB_SOURCES})
    set_target_properties(${LIB_TARGET} PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
    install(TARGETS ${LIB_TARGET} LIBRARY PUBLIC_HEADER)
else()
    add_library(${LIB_TARGET} ${LIB_SOURCES})
    set_target_properties(${LIB_TARGET} PROPERTIES PUBLIC_HEADER "${LIB_PUBLIC_HEADERS}")
    install(TARGETS ${LIB_TARGET} ARCHIVE PUBLIC_HEADER)
endif()
