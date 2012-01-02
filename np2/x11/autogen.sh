#!/bin/sh
aclocal
autoheader
automake -aci --foreign
autoconf
rm -f config.h.in~
./configure "$*"
( cd ../np2tool && unzip -j -o np2tool.zip ) 2>&1 > /dev/null
make maintainer-clean 2>&1 > /dev/null
