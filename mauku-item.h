
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef __MAUKU_ITEM_H__
#define __MAUKU_ITEM_H__

#include <gtk/gtk.h>
#include "mauku-widget.h"

#define MAUKU_TYPE_ITEM (mauku_item_get_type ())
#define MAUKU_ITEM(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), MAUKU_TYPE_ITEM, MaukuItem))
#define MAUKU_IS_ITEM(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MAUKU_TYPE_ITEM))
#define MAUKU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), MAUKU_ITEM, MaukuItemClass))
#define MAUKU_IS_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MAUKU_TYPE_ITEM))
#define MAUKU_ITEM_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), MAUKU_TYPE_ITEM, MaukuItemClass))

typedef struct _MaukuItemPrivate MaukuItemPrivate;

typedef struct _MaukuItem
{
	MaukuWidget parent_instance;
	MaukuItemPrivate* priv;
} MaukuItem;

typedef struct _MaukuItemClass
{
	MaukuWidgetClass parent_class;
	GdkPixbuf* background_top;
	GdkPixbuf* background_middle;
	GdkPixbuf* background_bottom;
	GdkPixbuf* background_unread;
	GdkPixbuf* background_comments;
	GdkPixbuf* comment_top;
	GdkPixbuf* comment_middle;
	GdkPixbuf* comment_bottom;
	GdkPixbuf* comment_unread;
	GdkPixbuf* comment_comments;	
	GdkPixbuf* marked_icon;
} MaukuItemClass;

GtkWidget* mauku_item_new(const gchar* publisher, const gchar* uri, const gchar* uid, GdkPixbuf* avatar, GdkPixbuf* icon, const gchar* text, const gchar* sender, const gchar* sender_uri, time_t timestamp, guint comments, const gchar* comments_uri, const gchar* referred_uid, const gchar* referred_uri, gboolean marked, gboolean unread, const gchar* link);
GtkWidget* mauku_item_new_from_item(MaukuItem* item);
const gchar* mauku_item_get_publisher(MaukuItem* item);
const gchar* mauku_item_get_uri(MaukuItem* item);
const gchar* mauku_item_get_uid(MaukuItem* item);
time_t mauku_item_get_timestamp(MaukuItem* item);
const gchar* mauku_item_get_text(MaukuItem* item);
const gchar* mauku_item_get_sender(MaukuItem* item);
const gchar* mauku_item_get_sender_uri(MaukuItem* item);
const gchar* mauku_item_get_comments_uri(MaukuItem* item);
const gchar* mauku_item_get_referred_uid(MaukuItem* item);
const gchar* mauku_item_get_referred_uri(MaukuItem* item);
const gchar* mauku_item_get_link(MaukuItem* item);
const gboolean mauku_item_get_marked(MaukuItem* item);
const gboolean mauku_item_get_unread(MaukuItem* item);
void mauku_item_set_marked(MaukuItem* item, gboolean marked);
void mauku_item_set_unread(MaukuItem* item, gboolean unread);
void mauku_item_set_avatar(MaukuItem* item, GdkPixbuf* avatar);

#endif
