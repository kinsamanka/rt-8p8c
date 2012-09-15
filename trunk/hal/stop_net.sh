#!/bin/sh
sudo rmmod rtudp
sudo sh -c "echo 0 > /proc/rtai/rtdm/open_fildes"
sudo /usr/local/rtnet/sbin/rtifconfig rteth0 down
sudo /usr/local/rtnet/sbin/rtifconfig rtlo down
sudo rmmod rt_8139too
sudo rmmod rt_loopback
sudo rmmod rtpacket
sudo rmmod rtipv4
sudo rmmod rtnet

