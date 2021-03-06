#! /bin/sh

# die on error
set -e

# go to source directory
cd "$(dirname "$0")"
pwd

# delete autotools cruft
if [ -f Makefile ]; then
    make maintainer-clean || :
fi

for x in \
    aclocal.m4 \
    ar-lib \
    configure \
    confdefs.h \
    config.guess \
    config.log \
    config.status \
    config.sub \
    config.cache \
    config.h.in \
    config.h \
    compile \
    libtool.m4 \
    ltmain.sh \
    ltoptions.m4 \
    ltsugar.m4 \
    ltversion.m4 \
    lt~obsolete.m4 \
    ltmain.sh \
    libtool \
    ltconfig \
    missing \
    mkinstalldirs \
    depcomp \
    install-sh \
    stamp-h[0-9]* \
    test-driver \
; do
    for path in "./" "./build-aux/" "./build-aux/m4/" "./src/"; do
        if [ -f "${path}${x}" ]; then
            echo "${path}${x}"
            rm -f "${path}${x}"
        fi
        if [ -f "${path}${x}~" ]; then
            echo "${path}${x}~"
            rm -f "${path}${x}~"
        fi
    done
done

find . -type f \( -name Makefile -or -name Makefile.in \) \
    -print -exec rm -f {} \;

# generate it again if needed
if [ "$1" != "clean" ]; then
    autoreconf -vif
fi
rm -Rf autom4te.cache
