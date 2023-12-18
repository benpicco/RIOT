#!/bin/bash

OUTFILE=/tmp/graph.png

echo $OUTFILE

while true; do
	# Convert the current time in seconds since the Epoch to the desired timestamp format
	current_timestamp=$(date --date="5 seconds ago" +"%H:%M:%S")

	./graph.sh $current_timestamp $OUTFILE
	sleep 1
done
