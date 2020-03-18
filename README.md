aiming at running unmodified busybox utilities.

```
#VERBOSITY=5 ./sandbox/vmx  root/bin/uname --help
BusyBox v1.31.1 (2020-02-20 09:29:26 EST) multi-call binary.

Usage: uname [-amnrspvio]

Print system information

    -a  Print all
    -m  The machine (hardware) type
    -n  Hostname
    -r  Kernel release
    -s  Kernel name (default)
    -p  Processor type
    -v  Kernel version
    -i  The hardware platform
    -o  OS name

Mar-18 05:55:44 root@my-container-host  ~/workspace/Zelda.RISCV.Emulator
#VERBOSITY=5 ./sandbox/vmx  root/bin/uname -a
ZeldaLinux hyrule 5.4.0 v2020.03 riscv32 GNU/Linux

```
