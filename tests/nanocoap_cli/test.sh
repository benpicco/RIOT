#!/usr/bin/bash

NUM=36
SEED=31337

export BOARD=native
export USE_ZEP=1

n=0
ELFFILE=bin/native/tests_nanocoap_cli.elf

while [[ $# -gt 1 ]]; do
  case $1 in
    -n|--num)
        NUM=$2
        shift 2
        ;;
    -i|--id)
        n=$2
        shift 2
        ;;
    -e|--elf)
        ELFFILE=$2
        shift 2
        ;;
  esac
done

ZEP_PORT=$((17754 + n))
SESSION=ZEPSession_$n
PIDFILE=/tmp/$SESSION.pid

case $1 in
stop)
    tmux kill-session -t $SESSION
    ;;
init)
    reset
    tmux kill-session -t $SESSION

    make -j all BOARD=native

#    ./topogen -w 250 -h 5 -r 10 -v 5 -n $NUM -s $SEED > test.topo
     DIM=$(echo "4*sqrt($NUM)" | bc)
    ./topogen -g -w $DIM -h $DIM -r 5 -v 0 -n $NUM -s $SEED > test.topo
    tail -n $NUM test.topo | cut -d ' ' -f 2 | cut -f 1,2,3,4 > test.topo.map

    tmux new-session -d -s $SESSION ./zep_dispatch -p $PIDFILE -s $SEED -t test.topo ::1 $ZEP_PORT

    for i in $(seq $NUM); do
        MAC=$(printf "%x:%x:%x:%x:%x:%x:%x:%x" $i $i $i $i $i $i $i $i)
        tmux set-option history-limit 8192 \; new-window -d -t $SESSION: $ELFFILE --id=$i --eui64=$MAC -z [::1]:$ZEP_PORT
    done

    sleep 7
    ;;
attach|a)
    tmux a -t $SESSION:$2
    ;;
start)
    echo Start Test
    START=$(date +%s.%m)

    kill -SIGUSR2 $(cat $PIDFILE)
    tmux send-keys -t $SESSION:1 "ncput_non /nvm0/dummy.txt /md5/in" Enter

    for i in $(seq 2 $NUM); do
        until HASH=$(tmux capture-pane -p -t $SESSION:$i | grep hash); do sleep 1; done
        echo "$i| $HASH"
    done

    kill -SIGUSR2 $(cat $PIDFILE)
    tmux capture-pane -p -t $SESSION:0 | grep tx_total
    END=$(date +%s.%m)
    DIFF=$(echo "$END - $START" | bc)
    echo "finished in $DIFF seconds"
    ;;
graph)
    kill -SIGUSR1 $(cat $PIDFILE)
    dot -Tpdf example.gv > example.gv.pdf
    ;;
esac
