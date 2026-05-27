/*
 * unnneded now, plus had it's logic wrong, 
 * will keep hashed for safe keeping, 
 * worse case we reuse, to throw, 
 * dossen't matter here
 *
#ifndef SIGNAL_FORMAT_H
#define SIGNAL_FORMAT_H

#include "../../signal.h"

extern signal_fs_t g_fs;
extern int g_fs_mounted;
extern int  g_fs_inited;


  signal_format()
    writes a fresh Signal filesystem to the block device
    attached to `fs`. Destroys all existing data.
    total_blocks  = how many 4096-byte blocks the partition has.
                    For a 1GB partition: (1024*1024*1024) / 4096 = 262144
    volume_label  = up to 31 chars, e.g. "copeland-sel"
    Call this ONCE on a blank partition, then use signal_mount() normally. 
int signal_format(signal_fs_t *fs,
                  uint32_t     total_blocks,
                  const char  *volume_label);

#endif
*/
