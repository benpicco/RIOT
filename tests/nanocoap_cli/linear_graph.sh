#!/bin/bash

start_timestamp=$1
seconds=$2

# Convert the start and end timestamps to seconds since the Epoch
start_seconds=$(date -d "$start_timestamp" +%s)
end_seconds=$((start_seconds + seconds))

data_file="/tmp/all.txt"
plot_file="/tmp/$start_timestamp-$seconds.png"
rm $data_file

# Generate sequence of timestamps with second intervals
for (( i=start_seconds; i<=end_seconds; i++ )); do
	# Convert the current time in seconds since the Epoch to the desired timestamp format
	current_timestamp=$(date -d "@$i" +"%H:%M:%S")

	y_coord=$((2*(i - start_seconds)))

	grep -h $current_timestamp native/*/page.lo | sort | cut -f 1,2,3,5 | sed s/$current_timestamp/$y_coord/g | sed s/\+//g | join test.topo.map - >> "$data_file"
done

num_nodes=$(cut -f1 -d' ' $data_file | sort | uniq | wc -l)

gnuplot -e "set terminal pngcairo enhanced size $((num_nodes*44)),$((seconds*34))" \
        -e "set output '${plot_file}'" \
        -e "rgbfudge(x) = x*51*32768 + (11-x)*51*128 + int(abs(5.5-x)*510/9.)" \
        -e "set style fill transparent solid 0.25 noborder" \
        -e "plot '${data_file}' using 2:(\$5*3):(sqrt(\$4)/3):(rgbfudge(\$6)) with circles fc rgb variable notitle, \
                 '${data_file}' using 2:(\$5*3):7 with labels notitle"

eom $plot_file
