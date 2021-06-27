#!/bin/bash

PREFIX_UPLINK=2001:dbc::/64
PREFIX_DOWNLINK=2001:db8::/32

RIOTBASE=../..
RIOTTOOLS=$(RIOTBASE)/dist/tools

# add address to loopback so we can test reaching an
# address outside the test net
sudo ip a a fd00:dead:beef::1/128 dev lo

# delete old taps
sudo $RIOTTOOLS/tapsetup/tapsetup -b br0 -t tap_a -d
sudo $RIOTTOOLS/tapsetup/tapsetup -b br1 -t tap_b -d
sudo $RIOTTOOLS/tapsetup/tapsetup -b br2 -t tap_c -d
sudo $RIOTTOOLS/tapsetup/tapsetup -b br3 -t tap_d -d

# create topology
sudo $RIOTTOOLS/tapsetup/tapsetup -b br0 -t tap_a -c 1
sudo $RIOTTOOLS/tapsetup/tapsetup -b br1 -t tap_b -c 3
sudo $RIOTTOOLS/tapsetup/tapsetup -b br2 -t tap_c -c 3
sudo $RIOTTOOLS/tapsetup/tapsetup -b br3 -t tap_d -c 1

# start radvd & dhcpd
sudo $RIOTTOOLS/radvd/radvd.sh -c br0 $PREFIX_UPLINK
sudo $RIOTTOOLS/dhcpv6-pd_ia/dhcpv6-pd_ia.py -g 60 br0 $PREFIX_DOWNLINK
