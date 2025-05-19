# C-Project-1
This project implements 'vsfsck', a file system consistency checker for a custom virtual file system called VSFS (Very Simple File System). This code analyzes a disk image ('vsfs.img') and detects inconsistencies in key file system structures such as the superblock, inodes, data blocks, and bitmaps. It also attempts to repair these issues to restore the integrity of the file system.

Features :
- Superblock validation (magic number, block size, key pointers)
- Inode and data bitmap consistency checks
- Detection of bad blocks and duplicate block references
- Identification of unused or invalid inodes

Objective:
The goal of this project is to deeply understand file system structure and consistency mechanisms by writing a code that mimics real-world checkers like 'fsck'.


# To Run the code:
- go to the terminal of linux
- create: "touch code.c"
- write/edit: "gedit code.c"
- compile: "gcc -o c code.c"
           " ./c "
- 1st run will show error and 2nd run will solve the errors and show no error msg i=of that file !
