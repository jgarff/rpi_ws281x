# Initialize Autoconf and define the project name, version, and bug report address.
AC_INIT([rpi_ws281x],[1.1.0],[jer@jers.net])
AM_INIT_AUTOMAKE([foreign -Wall -Werror])
AC_CONFIG_SRCDIR([src/ws2811.c])
AC_CONFIG_HEADERS([config.h])

# Check for a C compiler.
AC_PROG_CC
AC_PROG_RANLIB
AM_PROG_AR
LT_INIT

# Check for the math library 'm'.
AC_SEARCH_LIBS([cos], [m])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_BIGENDIAN

# Define version macros to be used in C code.
AC_DEFINE_UNQUOTED([VERSION_MAJOR], [1], [Major version number])
AC_DEFINE_UNQUOTED([VERSION_MINOR], [1], [Minor version number])
AC_DEFINE_UNQUOTED([VERSION_MICRO], [0], [Micro version number])

# Define build options that mirror the CMake options.
AC_ARG_ENABLE([shared],
    AS_HELP_STRING([--disable-shared], [Build a static library only (default is both)]),
    [case "${enableval}" in
      yes) build_shared=yes ;;
      no) build_shared=no ;;
      *) AC_MSG_ERROR([bad value ${enableval} for --enable-shared]) ;;
    esac],
    [build_shared=yes])
AM_CONDITIONAL([BUILD_SHARED], [test "x$build_shared" = xyes])

AC_ARG_ENABLE([test],
    AS_HELP_STRING([--disable-test], [Do not build the test application]),
    [case "${enableval}" in
      yes) build_test=yes ;;
      no) build_test=no ;;
      *) AC_MSG_ERROR([bad value ${enableval} for --enable-test]) ;;
    esac],
    [build_test=yes])
AM_CONDITIONAL([BUILD_TEST], [test "x$build_test" = xyes])

# Check for the debug option
AC_ARG_ENABLE([debug],
    AS_HELP_STRING([--disable-debug], [Turn off debugging]),
    [case "${enableval}" in
      yes) build_debug=yes ;;
      no) build_debug=no ;;
      *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
    esac],
    [build_debug=yes])
AM_CONDITIONAL([BUILD_DEBUG], [test "x$build_debug" = xyes])

# Create the output files.
AC_CONFIG_FILES([Makefile src/Makefile src/libws2811.pc])
AC_OUTPUT
