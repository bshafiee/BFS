BFS
========

BFS is an open-source distributed, scalable, replicated in-memory filesystem. BFS provides a POSIX compatible interface and can be mounted under any directory and be used same as a traditional filesystem such as Ext4 or NTFS as a userspace application without any kernel modification. BFS utilizes memory to store data which means a blazingly fast read/write speed. BFS is backed by Openstack Swift object storage (backend storage can be a disk, Amazon S3, or any other persistent storage as well). BFS nodes can communicate thorough a TCP connection or a faster ZERO_Networking solution. In ZERO_Networking mode the regular operating system network stack is bypassed and raw packets are shared between userspace and kernel.

Refer to http://bshafiee.github.io/BFS/ for more information.
