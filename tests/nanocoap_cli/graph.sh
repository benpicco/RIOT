#!/bin/bash

if [ $# -gt 0 ]; then
  TIME=$1
else
  TIME=$(echo -n `date +%H:%M:%S`)
  sleep 1
fi

FILE=/tmp/$TIME\_joined.txt

grep -h $TIME native/*/page.lo | sort | cut -f 1,2,3,5 > /tmp/$TIME.txt
# join /tmp/$TIME.txt test.topo.map > $FILE
join test.topo.map /tmp/$TIME.txt > $FILE

max_x=$(cut -f2 -d' ' $FILE | sort -rn | head -n1)
max_y=$(cut -f3 -d' ' $FILE | sort -rn | head -n1)

gnuplot -e "set terminal pngcairo enhanced size $((max_x*32)),$((max_y*32))" \
        -e "set output '/tmp/${TIME}.png'" \
        -e "set xrange [0:$((max_x + 2))]" \
        -e "set yrange [0:$((max_y + 2))]" \
        -e "rgbfudge(x) = x*51*32768 + (11-x)*51*128 + int(abs(5.5-x)*510/9.)" \
        -e "set style fill transparent solid 0.25 noborder" \
        -e "plot '${FILE}' using 2:3:(sqrt(\$4)):(rgbfudge(\$6)) with circles fc rgb variable notitle, \
                 '${FILE}' using 2:3:7 with labels notitle"

echo /tmp/${TIME}.png
