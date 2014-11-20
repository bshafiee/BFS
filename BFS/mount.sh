#!/bin/bash
rm -f log.txt
./BFS -f -o auto_unmount -o fsname=BFS -o hard_remove -o umask=0022 -o big_writes -o direct_io -o allow_root mountdir/
