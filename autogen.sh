#! /bin/sh

srcdir=`dirname $0`
test -z "$srcdir" && srcdir=.

ORIGDIR=`pwd`
cd $srcdir

VERSION=`./git-version-gen -u`
if ! grep -qs "^#define PACKAGE_VERSION \"${VERSION}\"\$" config.h; then
	touch configure.ac
fi

autoreconf -v --install || exit 1
cd $ORIGDIR || exit $?

$srcdir/configure "$@"
