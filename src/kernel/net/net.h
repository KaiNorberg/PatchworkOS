#pragma once

#include "fs/sysfs.h"

void net_init(void);

sysfs_dir_t* net_get_dir(void);
