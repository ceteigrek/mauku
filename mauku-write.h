
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef __MAUKU_WRITE_H__
#define __MAUKU_WRITE_H__

#include <gtk/gtk.h>
#include <microfeed-subscriber/microfeedsubscriber.h>
#include "mauku-item.h"

typedef struct _MaukuWrite MaukuWrite;

MaukuWrite* mauku_write_new(const gchar* title, MaukuItem* item, const gchar* prefix, const gchar* text);
void mauku_write_show(MaukuWrite* write);
void mauku_write_add_feed(MaukuWrite* write, const gchar* publisher, const gchar* uri);

#endif
