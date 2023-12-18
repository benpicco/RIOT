#!/bin/bash
start_timestamp=$1
seconds=$2

# Convert the start and end timestamps to seconds since the Epoch
start_seconds=$(date -d "$start_timestamp" +%s)
end_seconds=$((start_seconds + seconds))

# Generate sequence of timestamps with second intervals
for (( i=start_seconds; i<=end_seconds; i++ )); do
	# Convert the current time in seconds since the Epoch to the desired timestamp format
	current_timestamp=$(date -d "@$i" +"%H:%M:%S")

	./graph.sh $current_timestamp
done
