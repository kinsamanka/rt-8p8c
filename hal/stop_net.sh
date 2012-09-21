#!/bin/sh
sudo rmmod rt8p8c
sudo rmmod rtudp
sudo /usr/local/rtnet/sbin/rtifconfig rteth0 down
sudo /usr/local/rtnet/sbin/rtifconfig rtlo down
sudo rmmod rt_8139too
sudo rmmod rt_loopback
sudo rmmod rtpacket
sudo rmmod rtipv4
sudo rmmod rtnet
sudo rmmod rtai_rtdm

