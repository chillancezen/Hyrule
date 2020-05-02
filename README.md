```
$VERBOSITY=5 ROOT=/home/linky/Hyrule/root/ sandbox/vmx root/bin/bash
bash: initialize_job_control: no job control in background: No such process
bash-5.0# uname -a
ZeldaLinux hyrule 5.4.0 v2020.03 riscv32 GNU/Linux
bash-5.0# ls -l /
total 24
drwxrwxr-x    2 1000     1000          4096 May  2 01:56 bin
drwxrwxr-x    2 1000     1000          4096 Apr  4 14:34 coreutils
drwxrwxr-x    2 1000     1000          4096 Mar 21 04:33 dev
drwxrwxr-x    2 1000     1000          4096 May  2 07:05 etc
lrwxrwxrwx    1 1000     1000            11 Mar 21 04:33 init -> bin/busybox
lrwxrwxrwx    1 1000     1000            11 Mar 21 04:33 linuxrc -> bin/busybox
drwxrwxr-x    2 1000     1000          4096 Mar 21 04:33 sbin
drwxrwxr-x    4 1000     1000          4096 Mar 21 04:33 usr
bash-5.0# pwd
/
bash-5.0# cat > text
Hello World
bash-5.0# ls
bin        coreutils  dev        etc        init       linuxrc    sbin       text       usr
bash-5.0# cat -n text
     1	Hello World
bash-5.0# cp text text.bak
bash-5.0# ls -l
total 32
drwxrwxr-x    2 1000     1000          4096 May  2 01:56 bin
drwxrwxr-x    2 1000     1000          4096 Apr  4 14:34 coreutils
drwxrwxr-x    2 1000     1000          4096 Mar 21 04:33 dev
drwxrwxr-x    2 1000     1000          4096 May  2 07:05 etc
lrwxrwxrwx    1 1000     1000            11 Mar 21 04:33 init -> bin/busybox
lrwxrwxrwx    1 1000     1000            11 Mar 21 04:33 linuxrc -> bin/busybox
drwxrwxr-x    2 1000     1000          4096 Mar 21 04:33 sbin
-rw-rw-r--    1 1000     1000            12 May  2 07:13 text
-rw-rw-r--    1 1000     1000            12 May  2 07:13 text.bak
drwxrwxr-x    4 1000     1000          4096 Mar 21 04:33 usr
bash-5.0# rm -f text*
bash-5.0# ls
bin        coreutils  dev        etc        init       linuxrc    sbin       usr
bash-5.0#
```
