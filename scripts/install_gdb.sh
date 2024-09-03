#!/usr/bin/bash

export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"

cd $HOME/src

# The $PREFIX/bin dir _must_ be in the PATH. We did that above.
which -- $TARGET-as || echo $TARGET-as is not in the PATH

mkdir -p build-gdb
cd build-gdb
../gdb-15.1/configure --target=$TARGET --prefix="$PREFIX" --disable-werror 
make all-gdb
make install-gdb

