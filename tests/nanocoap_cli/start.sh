#!/usr/bin/bash

reset

case $1 in
root)
	make PORT=tap_a0 all term
	;;
a2)
	echo "server start" | make PORT=tap_a2 all term
	;;
b0a1)
	echo "server start" | make PORT="tap_b0 tap_a1" all term
	;;
b2)
	echo "server start" | make PORT=tap_b2 all term
	;;
c0b1)
	echo "server start" | make PORT="tap_c0 tap_b1" all term
	;;
c1)
	echo "server start" | make PORT=tap_c1 all term
	;;
esac

