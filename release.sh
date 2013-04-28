#!/bin/sh
#
# release.sh: script to roll a release tarball.
# copyright (c) 2006-2013 Emil Mikulic.
#
# This is for developer use only and lives in the git repo but
# shouldn't end up in a tarball.
#
if [ $# -ne 1 ]; then
  echo "usage: $0 1.123" >&2
  exit 1
fi

NAME=darkhttpd
VERSION="$1"

files="\
Makefile \
darkhttpd.c \
README \
"

say() {
  echo ==\> "$@" >&2
}

run() {
  say "$@"
  "$@" || { say ERROR!; exit 1; }
}

PKG=$NAME-$VERSION
say releasing $PKG
devel/clang-warns
devel/warns
# TODO: checker, tests
if git status --porcelain | egrep -v '^\?\?' -q; then
  say ERROR: uncommitted changes:
  git status
  exit 1
fi
run mkdir $PKG
run cp -r $files $PKG/.
sed -r -e \
  '/pkgname\[\]\s+= "/s/"darkhttpd\/[^"]+"/"'$NAME'\/'$VERSION'"/' \
  darkhttpd.c > $PKG/darkhttpd.c || { echo sed failed; exit 1; }
# package it up
run tar chof $PKG.tar $PKG
run bzip2 -9vv $PKG.tar
say output:
ls -l $PKG.tar.bz2
say FINISHED!
