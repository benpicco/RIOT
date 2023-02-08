#!/usr/bin/bash

NUM=50
SEED=31337

export BOARD=native
export USE_ZEP=1

case $1 in
stop)
    tmux kill-session -t ZEPSession
    ;;
init)
    reset
    tmux kill-session -t ZEPSession

    make -j all

    ./topogen -w 25 -h 25 -r 10 -v 5 -n $NUM -s $SEED > test.topo

    tmux new-session -d -s ZEPSession ./zep_dispatch -s $SEED -t test.topo ::1 17754

    for i in $(seq $NUM); do
        tmux set-option history-limit 5000 \; new-window -d -t ZEPSession: bin/native/tests_nanocoap_cli.elf --id=$i -z [::1]:17754
    done

    sleep 5
    ;;
attach|a)
    tmux a -t ZEPSession:$2
    ;;
start)
    echo Start Test
    START=$(date +%s.%m)

    killall -SIGUSR2 zep_dispatch
    tmux send-keys -t ZEPSession:1 "ncput_non /nvm0/dummy.txt /md5/in" Enter

    for i in $(seq 2 $NUM); do
        until HASH=$(tmux capture-pane -p -t ZEPSession:$i | grep hash); do sleep 1; done
        echo "$i| $HASH"
    done

    killall -SIGUSR2 zep_dispatch
    tmux capture-pane -p -t ZEPSession:0 | grep tx_total
    END=$(date +%s.%m)
    DIFF=$(echo "$END - $START" | bc)
    echo "finished in $DIFF seconds"
    ;;
graph)
    killall -USR1 zep_dispatch
    dot -Tpdf example.gv > example.gv.pdf
    ;;
esac
