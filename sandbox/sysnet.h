/*
 * Copyright (c) 2020 Jie Zheng
 */
#ifndef _SYSNET_H
#define _SYSNET_H
#include <hart.h>
#include <vm.h>

uint32_t
do_socket(struct hart * hartptr, uint32_t socket_famlily,
          uint32_t socket_type, uint32_t protocol);

#endif


