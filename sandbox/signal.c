/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <signal.h>
#include <util.h>

uint32_t
call_sigaction(struct hart * hartptr, uint32_t signum, uint32_t act_addr,
               uint32_t old_act_addr)
{

    // FIXME
    return 0;    
}

uint32_t
call_kill(struct hart * hartptr, uint32_t pid, uint32_t sig)
{
    // FIXME
    return -ESRCH;
}

uint32_t
call_sigprocmask(struct hart * hartptr, uint32_t how, uint32_t set_addr,
                 uint32_t oldset_addr)
{

    // FIXME
    return 0;
}
