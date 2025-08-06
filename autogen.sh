#!/bin/sh
set -e

# Optional: detect and error if required tools are missing
for tool in autoconf automake libtool pkg-config; do
    if ! command -v $tool >/dev/null 2>&1; then
        echo "Error: $tool not found. Please install it." >&2
        exit 1
    fi
done

# Optionally clean up old generated files
rm -f configure aclocal.m4 ltmain.sh libtool config.guess config.sub

echo "Running aclocal..."
aclocal

echo "Running autoheader..."
autoheader

echo "Running libtoolize..."
libtoolize --copy --force

echo "Running automake..."
automake --add-missing --copy

echo "Running autoconf..."
autoconf

echo "Generating configure script done."

# Run ./configure if desired
if [ "$1" = "--configure" ]; then
    shift
    echo "Running ./configure $@"
    ./configure "$@"
fi

