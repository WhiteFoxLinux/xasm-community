COMPILER=gcc
$COMPILER compile.c -o builds/compile -Oz
$COMPILER xasm.c -o builds/xasm -Oz