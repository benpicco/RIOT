#!/bin/bash

TIME=$1
FILE=/tmp/$TIME\_joined.txt

grep -h $TIME native/*/page.lo | sort | cut -f 1,2,3,5 > /tmp/$TIME.txt
# join /tmp/$TIME.txt test.topo.map > $FILE
join test.topo.map /tmp/$TIME.txt > $FILE

gnuplot -e "set terminal pngcairo enhanced size 2048,128" \
        -e "set output '/tmp/${TIME}.png'" \
        -e "set xrange [0:105]" \
        -e "set yrange [0:10]" \
        -e "rgbfudge(x) = x*51*32768 + (11-x)*51*128 + int(abs(5.5-x)*510/9.)" \
        -e "set style fill transparent solid 0.25 noborder" \
        -e "plot '${FILE}' using 2:3:(sqrt(\$4)):(rgbfudge(\$6)) with circles fc rgb variable notitle, \
                 '${FILE}' using 2:3:7 with labels notitle"
