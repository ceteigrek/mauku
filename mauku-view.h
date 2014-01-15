
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef __MAUKU_VIEW_H__
#define __MAUKU_VIEW_H__

#include <gtk/gtk.h>
#include <microfeed-subscriber/microfeedsubscriber.h>

typedef struct _MaukuView MaukuView;

MaukuView* mauku_view_new(const gchar* title, gboolean permanent);
void mauku_view_show(MaukuView* view);
void mauku_view_add_feed(MaukuView* view, const gchar* publisher, const gchar* uri);
void mauku_view_remove_feed(MaukuView* view, const gchar* publisher, const gchar* uri);
gboolean mauku_view_scroll_to_item(MaukuView* view, const gchar* publisher, const gchar* uri, const gchar* uid);
void mauku_view_update(MaukuView* view);

#endif
