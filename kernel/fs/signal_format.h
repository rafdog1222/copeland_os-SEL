#ifndef SIGNAL_FORMAT_H
#define SIGNAL_FORMAT_H

#include "../../signal.h"

int signal_format(signal_fs_t *fs,
                  uint32_t     total_blocks,
                  const char  *volume_label);

#endif
