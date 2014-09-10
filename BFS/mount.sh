#!/bin/bash


perf record ./BFS -f -o auto_unmount -o fsname=BFS -o hard_remove -o direct_io -o umask=777 -o big_writes -o no_splice_write mountdir/
