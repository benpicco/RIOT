#!/usr/bin/bash

reset

export BOARD=native
export USE_ZEP=1

make -j all

case $1 in
0 | root)
	TERMFLAGS="--id=1" make PORT=tap_a0 ZEP_MAC=AA:AA:AA:AA:AA:AA:AA:AA term
	;;
1 | a2)
	make PORT=tap_a2 ZEP_MAC=A2:A2:A2:A2:A2:A2:A2:A2 term
	;;
2 | b0a1)
	make PORT="tap_b0 tap_a1" ZEP_MAC=BB:BB:BB:BB:BB:BB:BB:BB term
	;;
3 | b2)
	make PORT=tap_b2 ZEP_MAC=B2:B2:B2:B2:B2:B2:B2:B2 term
	;;
4 | c0b1)
	make PORT="tap_c0 tap_b1" ZEP_MAC=CC:CC:CC:CC:CC:CC:CC:CC term
	;;
5 | c2)
	make PORT=tap_c2 ZEP_MAC=C2:C2:C2:C2:C2:C2:C2:C2 term
	;;
zep)
    ../../dist/tools/zep_dispatch/bin/zep_dispatch -t network.topo ::1 17754
    ;;
esac

