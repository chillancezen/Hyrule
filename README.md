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

To run the GNU bash
```
$VERBOSITY=5 ROOT=/home/linky/Hyrule/root/ sandbox/vmx root/bin/bash
bash: initialize_job_control: no job control in background: No such process
bash-5.0# help
GNU bash, version 5.0.0(1)-rc1 (riscv32-unknown-linux-gnu)
These shell commands are defined internally.  Type `help' to see this list.
Type `help name' to find out more about the function `name'.
Use `info bash' to find out more about the shell in general.
Use `man -k' or `info' to find out more about commands not in this list.

A star (*) next to a name means that the command is disabled.

 job_spec [&]                                                                                                           history [-c] [-d offset] [n] or history -anrw [filename] or history -ps arg [arg...]
 (( expression ))                                                                                                       if COMMANDS; then COMMANDS; [ elif COMMANDS; then COMMANDS; ]... [ else COMMANDS; ] fi
 . filename [arguments]                                                                                                 jobs [-lnprs] [jobspec ...] or jobs -x command [args]
 :                                                                                                                      kill [-s sigspec | -n signum | -sigspec] pid | jobspec ... or kill -l [sigspec]
 [ arg... ]                                                                                                             let arg [arg ...]
 [[ expression ]]                                                                                                       local [option] name[=value] ...
 alias [-p] [name[=value] ... ]                                                                                         logout [n]
 bg [job_spec ...]                                                                                                      mapfile [-d delim] [-n count] [-O origin] [-s count] [-t] [-u fd] [-C callback] [-c quantum] [array]
 bind [-lpsvPSVX] [-m keymap] [-f filename] [-q name] [-u name] [-r keyseq] [-x keyseq:shell-command] [keyseq:readlin>  popd [-n] [+N | -N]
 break [n]                                                                                                              printf [-v var] format [arguments]
 builtin [shell-builtin [arg ...]]                                                                                      pushd [-n] [+N | -N | dir]
 caller [expr]                                                                                                          pwd [-LP]
 case WORD in [PATTERN [| PATTERN]...) COMMANDS ;;]... esac                                                             read [-ers] [-a array] [-d delim] [-i text] [-n nchars] [-N nchars] [-p prompt] [-t timeout] [-u fd] [name ...]
 cd [-L|[-P [-e]] [-@]] [dir]                                                                                           readarray [-d delim] [-n count] [-O origin] [-s count] [-t] [-u fd] [-C callback] [-c quantum] [array]
 command [-pVv] command [arg ...]                                                                                       readonly [-aAf] [name[=value] ...] or readonly -p
 compgen [-abcdefgjksuv] [-o option] [-A action] [-G globpat] [-W wordlist]  [-F function] [-C command] [-X filterpat>  return [n]
 complete [-abcdefgjksuv] [-pr] [-DEI] [-o option] [-A action] [-G globpat] [-W wordlist]  [-F function] [-C command]>  select NAME [in WORDS ... ;] do COMMANDS; done
 compopt [-o|+o option] [-DEI] [name ...]                                                                               set [-abefhkmnptuvxBCHP] [-o option-name] [--] [arg ...]
 continue [n]                                                                                                           shift [n]
 coproc [NAME] command [redirections]                                                                                   shopt [-pqsu] [-o] [optname ...]
 declare [-aAfFgilnrtux] [-p] [name[=value] ...]                                                                        source filename [arguments]
 dirs [-clpv] [+N] [-N]                                                                                                 suspend [-f]
 disown [-h] [-ar] [jobspec ... | pid ...]                                                                              test [expr]
 echo [-neE] [arg ...]                                                                                                  time [-p] pipeline
 enable [-a] [-dnps] [-f filename] [name ...]                                                                           times
 eval [arg ...]                                                                                                         trap [-lp] [[arg] signal_spec ...]
 exec [-cl] [-a name] [command [arguments ...]] [redirection ...]                                                       true
 exit [n]                                                                                                               type [-afptP] name [name ...]
 export [-fn] [name[=value] ...] or export -p                                                                           typeset [-aAfFgilnrtux] [-p] name[=value] ...
 false                                                                                                                  ulimit [-SHabcdefiklmnpqrstuvxPT] [limit]
 fc [-e ename] [-lnr] [first] [last] or fc -s [pat=rep] [command]                                                       umask [-p] [-S] [mode]
 fg [job_spec]                                                                                                          unalias [-a] name [name ...]
 for NAME [in WORDS ... ] ; do COMMANDS; done                                                                           unset [-f] [-v] [-n] [name ...]
 for (( exp1; exp2; exp3 )); do COMMANDS; done                                                                          until COMMANDS; do COMMANDS; done
 function name { COMMANDS ; } or name () { COMMANDS ; }                                                                 variables - Names and meanings of some shell variables
 getopts optstring name [arg]                                                                                           wait [-fn] [id ...]
 hash [-lr] [-p pathname] [-dt] [name ...]                                                                              while COMMANDS; do COMMANDS; done
 help [-dms] [pattern ...]                                                                                              { COMMANDS ; }

bash-5.0# echo "bash builtin command: echo"
bash builtin command: echo
bash-5.0# foo="Hello Link"
bash-5.0# echo "say:$foo"
say:Hello Link
bash-5.0#
```
