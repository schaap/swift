#!/bin/bash

if [ "X$1X" == "XX" -o "X$2X" == "XX" ]; then
    echo "Plotter script for statistics of log version 1"
    echo "Usage: $0 inputfile output-basename"
    echo "Example:"
    echo "    $0 stats graphs"
    echo "will read from stats and write to graphs.time.png, graphs.time_norm.png and graphs.data.png"
    exit 1
fi

if [ ! \( -f "$1" -a -r "$1" \) ]; then
    echo "Can't read $1"
    exit 1
fi

if [ -d "$2.time.png" -o \( -e "$2.time.png" -a \( ! -w "$2.time.png" \) \) ]; then
    echo "Can't write $2.time.png"
    exit 1
fi

if [ -d "$2.time_norm.png" -o \( -e "$2.time_norm.png" -a \( ! -w "$2.time_norm.png" \) \) ]; then
    echo "Can't write $2.time_norm.png"
    exit 1
fi

if [ -d "$2.data.png" -o \( -e "$2.data.png" -a \( ! -w "$2.data.png" \) \) ]; then
    echo "Can't write $2.data.png"
    exit 1
fi

gnuplot <<EOF
set term png notransparent truecolor small size 1024,768
set output '$2.time.png'
plot '$1' using 1:3 title 'Usertime (us)' with lines lc rgbcolor "#0000A0", '$1' using 1:6 title 'Kerneltime (us)' with lines lc rgbcolor "#A00000"
set output '$2.time_norm.png'
set yrange [0:100]
plot '$1' using 1:4 title 'Usertime (%)' with lines lc rgbcolor "#0000A0", '$1' using 1:7 title 'Kerneltime (%)' with lines lc rgbcolor "#A00000", '$1' using 1:(\$4+\$7) title 'Total time (%)' with lines lc rgbcolor "#00A000"
set autoscale
set output '$2.data.png'
plot '$1' using 1:(\$11/(1024*1024)) title 'Data read (MB/s)' with lines lc rgbcolor "#00C000", '$1' using 1:(\$14/(1024*1024)) title 'Data written (MB/s)' with lines lc rgbcolor "#0000C0"
EOF

