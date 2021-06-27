#!/bin/bash

sudo ip a a fd00:dead:beef::1/128 dev lo

sudo ./dist/tools/tapsetup/tapsetup -b br0 -t tap_a -c 1
sudo ./dist/tools/tapsetup/tapsetup -b br1 -t tap_b
sudo ./dist/tools/tapsetup/tapsetup -b br2 -t tap_c
sudo ./dist/tools/tapsetup/tapsetup -b br3 -t tap_d -c 1
