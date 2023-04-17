#!/bin/sh

MOUNT_SRC_DIR=/build/src

# compile sysy-compiler the way official benchmark does

INCLUDE_DIRS=`find $MOUNT_SRC_DIR -type f \( -name "*.h" -or -name "*.hh" \) -exec dirname {} + | uniq | sed -e 's/^/-I/'`
SRCS=`find $MOUNT_SRC_DIR -type f \( -name "*.cc" -or -name "*.cpp" \)`

echo including $INCLUDE_DIRS
echo compiling $SRCS
clang++ -std=c++17 -O2 -lm -lantlr4-runtime $INCLUDE_DIRS $SRCS -o /tmp/a.out
