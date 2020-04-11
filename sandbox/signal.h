/*
 * Copyright (c) 2020 Jie Zheng
 */
#ifndef _SIGNAL_H
#define _SIGNAL_H
#include <hart.h>


uint32_t
call_sigaction(struct hart * hartptr, uint32_t signum, uint32_t act_addr,
               uint32_t old_act_addr);


uint32_t
call_kill(struct hart * hartptr, uint32_t pid, uint32_t sig);


uint32_t
call_sigprocmask(struct hart * hartptr, uint32_t how, uint32_t set_addr,
                 uint32_t oldset_addr);

#endif
