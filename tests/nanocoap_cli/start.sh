#!/usr/bin/bash

reset

export USE_ZEP=1

case $1 in
0 | root)
	TERMFLAGS="--id=1" make PORT=tap_a0 ZEP_MAC=AA:AA:AA:AA:AA:AA:AA:AA all term
	;;
1 | a2)
	make PORT=tap_a2 ZEP_MAC=A2:A2:A2:A2:A2:A2:A2:A2 all term
	;;
2 | b0a1)
	make PORT="tap_b0 tap_a1" ZEP_MAC=BB:BB:BB:BB:BB:BB:BB:BB all term
	;;
3 | b2)
	make PORT=tap_b2 ZEP_MAC=B2:B2:B2:B2:B2:B2:B2:B2 all term
	;;
4 | c0b1)
	make PORT="tap_c0 tap_b1" ZEP_MAC=CC:CC:CC:CC:CC:CC:CC:CC all term
	;;
5 | c2)
	make PORT=tap_c2 ZEP_MAC=C2:C2:C2:C2:C2:C2:C2:C2 all term
	;;
zep)
    ../../dist/tools/zep_dispatch/bin/zep_dispatch -t network.topo ::1 17754
esac

