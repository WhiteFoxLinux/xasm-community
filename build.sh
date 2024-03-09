COMPILER=gcc
$COMPILER compile.c -o builds/compile -Ofast
$COMPILER xasm.c -o builds/xasm -Ofast