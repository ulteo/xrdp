#!/bin/bash

# check needed tools
tools=( aclocal libtoolize autoheader autoconf automake libtool )
for tool in "${tools[@]}"; do
  which $tool > /dev/null
  if ! test $? -eq 0
  then
    echo "error, install $tool"
    exit 1
  fi
done

# build configure.in using svn revno
if [ -d .svn ]; then
  revision=$(LC_ALL=C svn info . | awk '/^Revision: / {printf "%05d\n", $2}')
else
  revision=0
fi
sed -e "s/@REVISION@/${revision}/g" < configure.ac.in > configure.in

# ????
CONFIG_PATH=$PWD/sesman/tools/config.c
if [ ! -e $CONFIG_PATH ]; then
  echo "# create sesman/tools/config.c link #"
  ln -s ../config.c $CONFIG_PATH
fi

# create configure file
echo "# launch aclocal #"
aclocal

echo "# launch libtoolize #"
libtoolize --force

echo "# launch autoconf #"
autoconf --force

echo "# launch autoheader #"
autoheader --force

echo "# launch automake #"
automake --add-missing --copy --force-missing
