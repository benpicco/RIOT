#!/usr/bin/bash

reset
tmux kill-session -t ZEPSession

NUM=50
SEED=31337

export BOARD=native
export USE_ZEP=1

make -j all

./topogen -w 25 -h 25 -r 200 -v 5 -n $NUM -s $SEED > test.topo

tmux new-session -d -s ZEPSession ./zep_dispatch -s $SEED -t test.topo ::1 17754

for i in $(seq $NUM); do
     tmux new-window -d -t ZEPSession: bin/native/tests_nanocoap_cli.elf --id=$i -z [::1]:17754
done

tmux a -t ZEPSession:1
