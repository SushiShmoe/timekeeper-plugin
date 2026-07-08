#!/bin/sh
path=$(pwd)
mkdir -p $path/dev/
socat PTY,link=$path/dev/ttyS0,echo=0 PTY,link=$path/dev/ttyS1,echo=0
