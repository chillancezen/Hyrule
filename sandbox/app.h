/*
 * Copyright (c) 2020 Jie Zheng
 */
#include <vm.h>


void
mmap_setup(struct virtual_machine * vm, uint32_t addr_low, uint32_t len,
           uint32_t flags, void * host_base);

void
application_sandbox_init(struct virtual_machine * vm, const char * app_path,
                         char ** argv, char ** envp);

