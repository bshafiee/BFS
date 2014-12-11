#!/bin/bash
rm -f log.txt
ulimit -c unlimited
sudo setcap cap_net_admin+eip BFS
./BFS -f -o auto_unmount -o fsname=BFS -o hard_remove -o umask=0022 -o big_writes -o direct_io mountdir/
