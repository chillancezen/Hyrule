aiming at running unmodified busybox utilities.

```
$VERBOSITY=5 ROOT=/home/linky/Hyrule/root/ sandbox/vmx root/coreutils/uname --help
Usage: root/coreutils/uname [OPTION]...
Print certain system information.  With no OPTION, same as -s.

  -a, --all                print all information, in the following order,
                             except omit -p and -i if unknown:
  -s, --kernel-name        print the kernel name
  -n, --nodename           print the network node hostname
  -r, --kernel-release     print the kernel release
  -v, --kernel-version     print the kernel version
  -m, --machine            print the machine hardware name
  -p, --processor          print the processor type (non-portable)
  -i, --hardware-platform  print the hardware platform (non-portable)
  -o, --operating-system   print the operating system
      --help     display this help and exit
      --version  output version information and exit

GNU coreutils online help: <https://www.gnu.org/software/coreutils/>
Report any translation bugs to <https://translationproject.org/team/>
Full documentation <https://www.gnu.org/software/coreutils/uname>
or available locally via: info '(coreutils) uname invocation'

Mar-22 13:17:43 linky@hyrule  ~/Hyrule
$VERBOSITY=5 ROOT=/home/linky/Hyrule/root/ sandbox/vmx root/coreutils/uname -a
ZeldaLinux hyrule 5.4.0 v2020.03 riscv32 GNU/Linux

Mar-22 13:17:48 linky@hyrule  ~/Hyrule
$VERBOSITY=5 ROOT=/home/linky/Hyrule/root/ sandbox/vmx root/coreutils/ls -l /
total 20
drwxrwxr-x 2 1000 1000 4096 Mar 21 04:33 bin
drwxrwxr-x 2 1000 1000 ? 4096 Mar 22 02:15 coreutils
drwxrwxr-x 2 1000 1000 ? 4096 Mar 21 04:33 dev
lrwxrwxrwx 1 1000 1000 ?   11 Mar 21 04:33 init -> bin/busybox
lrwxrwxrwx 1 1000 1000 ?   11 Mar 21 04:33 linuxrc -> bin/busybox
drwxrwxr-x 2 1000 1000 ? 4096 Mar 21 04:33 sbin
drwxrwxr-x 4 1000 1000 ? 4096 Mar 21 04:33 usr
```
