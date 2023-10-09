#!/bin/bash
for i in $(seq 1 50); do printf 'n%03g n%03g 0.8\n' $i $((i+1)); done > test.topo
for i in $(seq 1 50); do printf 'n%03g %g 5 10\n' $i $((2*i)); done > test.topo.map
