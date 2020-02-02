#!/bin/bash
aclocal || exit 10
libtoolize || exit 20
autoreconf -fis || exit 30
automake --add-missing || exit 40
git submodule init || exit 50
git submodule update 
