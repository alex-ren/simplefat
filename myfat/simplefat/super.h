#ifndef _SFAT_SUPER_H
#define _SFAT_SUPER_H

#include <linux/fs.h>

int sfat_fill_super_impl(struct super_block *sb, void *data, int silent);

#endif

