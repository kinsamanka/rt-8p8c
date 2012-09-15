#!/bin/sh
sudo insmod /usr/realtime-2.6.32-122-rtai/modules/rtai_rtdm.ko
sudo rmmod 8139too
sudo insmod /usr/local/rtnet/modules/rtnet.ko
sudo insmod /usr/local/rtnet/modules/rtipv4.ko
sudo insmod /usr/local/rtnet/modules/rtpacket.ko
sudo insmod /usr/local/rtnet/modules/rt_loopback.ko
sudo insmod /usr/local/rtnet/modules/rt_8139too.ko
sudo /usr/local/rtnet/sbin/rtifconfig rteth0 up 10.0.0.1
sudo /usr/local/rtnet/sbin/rtifconfig rtlo up 127.0.0.1
sudo insmod /usr/local/rtnet/modules/rtudp.ko
sudo /usr/local/rtnet/sbin/rtroute solicit 10.0.0.2 dev rteth0

