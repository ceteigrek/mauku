
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#include "mauku.h"
#include "mauku-item.h"
#include <microfeed-common/microfeedprotocol.h>
#include <string.h>

#define ICON_AREA_INDENT 75
#define ICON_AREA_HEIGHT 50
#define ICON_AREA_SPACING 15
#define MARGIN_LEFT 14
#define MARGIN_RIGHT 14
#define MARGIN_TOP 12
#define MARGIN_BOTTOM 14
#define AVATAR_X 15
#define AVATAR_Y 11
#define MARKED_ICON_X 0
#define MARKED_ICON_Y 0

G_DEFINE_TYPE(MaukuItem, mauku_item, MAUKU_TYPE_WIDGET);

enum {
	PROP_0,
};

struct _MaukuItemPrivate {
	gchar* publisher;
	gchar* publisher_part;
	gchar* uri;
	gchar* uid;
	GdkPixbuf* avatar;
	GdkPixbuf* icon;
	gchar* text;
	gchar* sender;
	gchar* sender_uri;
	time_t timestamp;
	guint comments;
	gchar* comments_uri;
	gchar* referred_uid;
	gchar* referred_uri;
	gboolean marked;
	gboolean unread;
	gchar* link;

	GdkPixmap* buffer;
	PangoLayout* layout1;
	guint layout1_lines;
	PangoLayout* layout2;
	PangoLayout* layout3;
	gint layout3_x;
	gint layout3_y;
	gint layout3_height;
	guint timeout_id;
};

static guint do_layout(MaukuItem* item, guint width);
static guint do_timestamp_layout(MaukuItem* item, guint width);

static void mauku_item_set_property(GObject* object, guint prop_id, const GValue* value, GParamSpec* pspec) {
	MaukuItem* item;
	
	item = MAUKU_ITEM(object);

	switch (prop_id) {
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
			break;
	}
}

static void mauku_item_get_property(GObject* object, guint prop_id, GValue* value, GParamSpec* pspec) {
        MaukuItem* item;

	item = MAUKU_ITEM(object);

        switch (prop_id) {
	        default:
		        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		        break;
        }
}

static void mauku_item_finalize(GObject* object) {
	MaukuItem* item;
	
	item = MAUKU_ITEM(object);
	g_free(item->priv->publisher);
	g_free(item->priv->publisher_part);
	g_free(item->priv->uri);
	g_free(item->priv->uid);
	g_free(item->priv->text);
	g_free(item->priv->sender);
	g_free(item->priv->sender_uri);
	g_free(item->priv->link);
	
	G_OBJECT_CLASS (mauku_item_parent_class)->finalize(object);
}

static void mauku_item_dispose(GObject* object) {
	MaukuItem* item;
	
	item = MAUKU_ITEM(object);
	if (item->priv->avatar) {
		g_object_unref(item->priv->avatar);
		item->priv->avatar = NULL;
	}
	if (item->priv->icon) {
		g_object_unref(item->priv->icon);
		item->priv->icon = NULL;
	}

	if (item->priv->layout1) {
		g_object_unref(item->priv->layout1);
		item->priv->layout1 = NULL;
	}
	if (item->priv->layout2) {
		g_object_unref(item->priv->layout2);
		item->priv->layout2 = NULL;
	}
	if (item->priv->layout3) {
		g_object_unref(item->priv->layout3);
		item->priv->layout3 = NULL;
	}
	if (item->priv->timeout_id) {
		g_source_remove(item->priv->timeout_id);
		item->priv->timeout_id = 0;
	}
	if (item->priv->buffer) {
		g_object_unref(item->priv->buffer);
		item->priv->buffer = NULL;
	}
	
	G_OBJECT_CLASS (mauku_item_parent_class)->dispose(object);
}

static void mauku_item_size_request(GtkWidget* widget, GtkRequisition* requisition) {
	MaukuItem* item;
	MaukuItemClass* item_class;
	guint min_height;
	
	item = MAUKU_ITEM(widget);
	item_class = MAUKU_ITEM_GET_CLASS(item);

	if (widget->allocation.width > 1) {
		requisition->width = widget->allocation.width;
	} else {
		requisition->width = 792;
	}

	requisition->height = do_layout(MAUKU_ITEM(widget), requisition->width);
	if (item->priv->unread) {
		min_height = (item_class->background_unread ? gdk_pixbuf_get_height(item_class->background_unread) : 0) +
	        	     (item_class->background_bottom ? gdk_pixbuf_get_height(item_class->background_bottom) : 0);
	} else {
		min_height = (item_class->background_top ? gdk_pixbuf_get_height(item_class->background_top) : 0) +
	        	     (item_class->background_bottom ? gdk_pixbuf_get_height(item_class->background_bottom) : 0);
	}
	if (requisition->height < min_height) {
		requisition->height = min_height;
	}

	if (item->priv->buffer) {
		g_object_unref(item->priv->buffer);
		item->priv->buffer = NULL;
	}
}

static gboolean mauku_item_expose_event(GtkWidget* widget, GdkEventExpose* event) {
	MaukuItem* item;
	MaukuItemClass* item_class;
	cairo_t* cairo;
	cairo_pattern_t* pattern;
	GdkGC* gc;
	GdkColor color;
	PangoLayoutLine* layout_line;
	PangoRectangle rectangle;
	gint x;
	gint y;
	guint line;
	char buffer[1024];
	PangoLayout* layout;
	gint padding;
	
	g_return_val_if_fail(MAUKU_IS_ITEM(widget), FALSE);
	g_return_val_if_fail(event != NULL, FALSE);
	
	if (GTK_WIDGET_VISIBLE(widget) && GTK_WIDGET_MAPPED(widget)) {
		item = MAUKU_ITEM(widget);
		item_class = MAUKU_ITEM_GET_CLASS(item);
		gc = gdk_gc_new(widget->window);

		if (!item->priv->buffer) {
			item->priv->buffer = gdk_pixmap_new(widget->window, widget->requisition.width, widget->requisition.height, -1);

			gtk_paint_flat_box(widget->style, item->priv->buffer, GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL, widget, "maukuitem", 0, 0, -1, -1);
			if (item_class->background_middle) {
				for (y = (item_class->background_top ? gdk_pixbuf_get_height(item_class->background_top) : 0);
				     y < widget->requisition.height - (item_class->background_bottom ? gdk_pixbuf_get_height(item_class->background_bottom) : 0) - gdk_pixbuf_get_height(item_class->background_middle);
				     y += gdk_pixbuf_get_height(item_class->background_middle)) {
				     	if (item->priv->referred_uri && item_class->comment_middle) {
						gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->comment_middle, 0, 0, 0, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
					} else if (item_class->background_middle) {
						gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->background_middle, 0, 0, 0, y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
					}
				}
				if (item->priv->referred_uri && item_class->comment_middle) {
					gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->comment_middle, 0, 0, 0, widget->requisition.height - (item_class->comment_bottom ? gdk_pixbuf_get_height(item_class->background_bottom) : 0) - gdk_pixbuf_get_height(item_class->background_middle), -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
				} else if (item_class->background_middle) {
					gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->background_middle, 0, 0, 0, widget->requisition.height - (item_class->background_bottom ? gdk_pixbuf_get_height(item_class->background_bottom) : 0) - gdk_pixbuf_get_height(item_class->background_middle), -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
				}

			}

			if (item->priv->unread && item->priv->referred_uri && item_class->comment_unread) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->comment_unread, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			} else if (item->priv->referred_uri && item_class->comment_unread) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->comment_top, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			} else if (item->priv->unread && item_class->background_unread) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->background_unread, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			} else if (item_class->background_top) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->background_top, 0, 0, 0, 0, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			}
			if (item->priv->comments && item->priv->referred_uri && item_class->comment_comments) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->comment_comments, 0, 0, 0, widget->requisition.height - gdk_pixbuf_get_height(item_class->comment_comments), -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			} else if (item->priv->referred_uri && item_class->comment_bottom) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->comment_bottom, 0, 0, 0, widget->requisition.height - gdk_pixbuf_get_height(item_class->comment_bottom), -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			} else if (item->priv->comments && item_class->background_comments) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->background_comments, 0, 0, 0, widget->requisition.height - gdk_pixbuf_get_height(item_class->background_comments), -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			} else if (item_class->background_bottom) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->background_bottom, 0, 0, 0, widget->requisition.height - gdk_pixbuf_get_height(item_class->background_bottom), -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			}

			if (item->priv->avatar) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item->priv->avatar, 0, 0, AVATAR_X, AVATAR_Y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			}

			if (!item->priv->layout1) {
				do_layout(item, widget->requisition.width);
			}
			color.red = color.green = color.blue = 0x0000;
			if (item->priv->layout1) {
				x =  MARGIN_LEFT + ICON_AREA_INDENT;
				if (item->priv->layout1_lines) {
					y = 0;
					for (line = 0; line < item->priv->layout1_lines; line++) {
						if ((layout_line = pango_layout_get_line_readonly(item->priv->layout1, line))) {
							pango_layout_line_get_pixel_extents(layout_line, NULL, &rectangle);
							gdk_draw_layout_line_with_colors(item->priv->buffer, gc, x - rectangle.x, MARGIN_TOP + y - rectangle.y, layout_line, &color, NULL);
							y += rectangle.height;
						}
					}
					if (item->priv->layout2) {
						pango_layout_get_pixel_extents(item->priv->layout2, NULL, &rectangle);
						gdk_draw_layout_with_colors(item->priv->buffer, gc, MARGIN_LEFT - rectangle.x, MARGIN_TOP + y - rectangle.y, item->priv->layout2, &color, NULL);
					}
				} else {
					pango_layout_get_pixel_extents(item->priv->layout1, NULL, &rectangle);
					gdk_draw_layout_with_colors(item->priv->buffer, gc, x - rectangle.x, MARGIN_TOP - rectangle.y, item->priv->layout1, &color, NULL);
				}
				gdk_draw_layout_with_colors(item->priv->buffer, gc, MARGIN_LEFT + item->priv->layout3_x, MARGIN_TOP + item->priv->layout3_y, item->priv->layout3, &color, NULL);
			}
			if (item->priv->marked && item_class->marked_icon) {
				gdk_draw_pixbuf(item->priv->buffer, NULL, item_class->marked_icon, 0, 0, MARKED_ICON_X, MARKED_ICON_Y, -1, -1, GDK_RGB_DITHER_NONE, 0, 0);
			}
			if (item->priv->comments) {
				snprintf(buffer, 1024, "<small>%d</small>", item->priv->comments);
				layout = gtk_widget_create_pango_layout(GTK_WIDGET(item), "");
				pango_layout_set_markup(layout, buffer, -1);
				pango_layout_get_pixel_extents(layout, NULL, &rectangle);
				padding = (32 - rectangle.width) / 2;
				gdk_draw_layout_with_colors(item->priv->buffer, gc, widget->requisition.width - rectangle.width - padding - MARGIN_LEFT, widget->requisition.height - rectangle.height - MARGIN_BOTTOM + 5, layout, &color, NULL);
			}
		}
		gdk_draw_drawable(widget->window, gc, item->priv->buffer, 0, 0, 0, 0, -1, -1);
		g_object_unref(gc);
	}

	return FALSE;
}

static void mauku_item_class_init(MaukuItemClass* klass) {
	GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass* gtk_widget_class = GTK_WIDGET_CLASS(klass);

	gobject_class->finalize = mauku_item_finalize;
	gobject_class->dispose = mauku_item_dispose;
	gobject_class->set_property = mauku_item_set_property;
	gobject_class->get_property = mauku_item_get_property;

	gtk_widget_class->size_request = mauku_item_size_request;
	gtk_widget_class->expose_event = mauku_item_expose_event;

	klass->background_top = gdk_pixbuf_new_from_file(IMAGE_DIR "/background_top.png", NULL);
	klass->background_middle = gdk_pixbuf_new_from_file(IMAGE_DIR "/background_middle.png", NULL);
	klass->background_bottom = gdk_pixbuf_new_from_file(IMAGE_DIR "/background_bottom.png", NULL);
	klass->background_unread = gdk_pixbuf_new_from_file(IMAGE_DIR "/background_unread.png", NULL);
	klass->background_comments = gdk_pixbuf_new_from_file(IMAGE_DIR "/background_comments.png", NULL);
	klass->comment_top = gdk_pixbuf_new_from_file(IMAGE_DIR "/comment_top.png", NULL);
	klass->comment_middle = gdk_pixbuf_new_from_file(IMAGE_DIR "/comment_middle.png", NULL);
	klass->comment_bottom = gdk_pixbuf_new_from_file(IMAGE_DIR "/comment_bottom.png", NULL);
	klass->comment_unread = gdk_pixbuf_new_from_file(IMAGE_DIR "/comment_unread.png", NULL);
	klass->comment_comments = gdk_pixbuf_new_from_file(IMAGE_DIR "/comment_comments.png", NULL);
	klass->marked_icon = gdk_pixbuf_new_from_file(IMAGE_DIR "/marked.png", NULL);

	g_type_class_add_private (gobject_class, sizeof (MaukuItemPrivate));
}

static void mauku_item_init(MaukuItem* item) {
	item->priv = G_TYPE_INSTANCE_GET_PRIVATE(item, MAUKU_TYPE_ITEM, MaukuItemPrivate);
}

GtkWidget* mauku_item_new(const gchar* publisher, const gchar* uri, const gchar* uid, GdkPixbuf* avatar, GdkPixbuf* icon, const gchar* text, const gchar* sender, const gchar* sender_uri, time_t timestamp, guint comments, const gchar* comments_uri, const gchar* referred_uid, const gchar* referred_uri, gboolean marked, gboolean unread, const gchar* link) {
	GtkWidget* widget;
	gchar* s;
	
	widget = GTK_WIDGET(g_object_new(MAUKU_TYPE_ITEM, NULL));
	MAUKU_ITEM(widget)->priv->publisher = g_strdup(publisher);
	if ((s = strchr(publisher, MICROFEED_PUBLISHER_IDENTIFIER_SEPARATOR_CHAR))) {
		MAUKU_ITEM(widget)->priv->publisher_part = g_strndup(publisher, s - publisher);
	} else {
		MAUKU_ITEM(widget)->priv->publisher_part = g_strdup(publisher);
	}
	MAUKU_ITEM(widget)->priv->uri = g_strdup(uri);
	MAUKU_ITEM(widget)->priv->uid = g_strdup(uid);
	if (avatar) {
		MAUKU_ITEM(widget)->priv->avatar = g_object_ref(avatar);
	}
	if (icon) {
		MAUKU_ITEM(widget)->priv->icon = g_object_ref(icon);
	}
	MAUKU_ITEM(widget)->priv->text = g_strdup(text);
	MAUKU_ITEM(widget)->priv->sender = g_strdup(sender);
	MAUKU_ITEM(widget)->priv->sender_uri = g_strdup(sender_uri);
	MAUKU_ITEM(widget)->priv->timestamp = timestamp;
	MAUKU_ITEM(widget)->priv->comments = comments;
	MAUKU_ITEM(widget)->priv->comments_uri = g_strdup(comments_uri);
	MAUKU_ITEM(widget)->priv->referred_uid = g_strdup(referred_uid);
	MAUKU_ITEM(widget)->priv->referred_uri = g_strdup(referred_uri);
	MAUKU_ITEM(widget)->priv->marked = marked;
	MAUKU_ITEM(widget)->priv->unread = unread;
	MAUKU_ITEM(widget)->priv->link = g_strdup(link);

	return widget;
}

GtkWidget* mauku_item_new_from_item(MaukuItem* item) {

	return mauku_item_new(item->priv->publisher, item->priv->uri, item->priv->uid, item->priv->avatar, item->priv->icon, item->priv->text, item->priv->sender, item->priv->sender_uri, item->priv->timestamp, item->priv->comments, item->priv->comments_uri, item->priv->referred_uid, item->priv->referred_uri, item->priv->marked, item->priv->unread, item->priv->link);
}


time_t mauku_item_get_timestamp(MaukuItem* item) {
	g_return_val_if_fail(MAUKU_IS_ITEM(item), 0);
	
	return item->priv->timestamp;
}

const gchar* mauku_item_get_publisher(MaukuItem* item) {
	
	return item->priv->publisher;
}

const gchar* mauku_item_get_uri(MaukuItem* item) {

	return item->priv->uri;
}

const gchar* mauku_item_get_uid(MaukuItem* item) {

	return item->priv->uid;
}

const gchar* mauku_item_get_text(MaukuItem* item) {

	return item->priv->text;
}

const gchar* mauku_item_get_sender(MaukuItem* item) {

	return item->priv->sender;
}

const gchar* mauku_item_get_sender_uri(MaukuItem* item) {

	return item->priv->sender_uri;
}

const gchar* mauku_item_get_comments_uri(MaukuItem* item) {

	return item->priv->comments_uri;
}

const gchar* mauku_item_get_referred_uid(MaukuItem* item) {

	return item->priv->referred_uid;
}

const gchar* mauku_item_get_referred_uri(MaukuItem* item) {

	return item->priv->referred_uri;
}

const gchar* mauku_item_get_link(MaukuItem* item) {

	return item->priv->link;
}

const gboolean mauku_item_get_marked(MaukuItem* item) {

	return item->priv->marked;
}

const gboolean mauku_item_get_unread(MaukuItem* item) {

	return item->priv->unread;
}

void mauku_item_set_marked(MaukuItem* item, gboolean marked) {
	if ((item->priv->marked && !marked) || (!item->priv->marked && marked)) {
		item->priv->marked = (marked ? TRUE : FALSE);
		if (item->priv->buffer) {
			g_object_unref(item->priv->buffer);
			item->priv->buffer = NULL;
		}
		gtk_widget_queue_draw(GTK_WIDGET(item));
	}		
}

void mauku_item_set_unread(MaukuItem* item, gboolean unread) {
	if ((item->priv->unread && !unread) || (!item->priv->unread && unread)) {
		item->priv->unread = (unread ? TRUE : FALSE);
		if (item->priv->buffer) {
			g_object_unref(item->priv->buffer);
			item->priv->buffer = NULL;
		}
		gtk_widget_queue_draw(GTK_WIDGET(item));
	}
}

void mauku_item_set_avatar(MaukuItem* item, GdkPixbuf* avatar) {
	if (item->priv->avatar) {
		g_object_unref(item->priv->avatar);
	}
	item->priv->avatar = g_object_ref(avatar);
	if (item->priv->buffer) {
		g_object_unref(item->priv->buffer);
		item->priv->buffer = NULL;
	}
	gtk_widget_queue_draw(GTK_WIDGET(item));	
}


static guint get_icons_width(MaukuItem* item) {
	guint width;
	MaukuItemClass* item_class;

	item_class = MAUKU_ITEM_GET_CLASS(item);
	width = 0;
/*	if (item->priv->comments > 0) {
		width += gdk_pixbuf_get_width(item_class->comments_icon) + ICON_AREA_SPACING;
	}
*/
	return width;
}

static guint do_layout(MaukuItem* item, guint width) {
	guint height;
	PangoLayoutLine* layout_line;
	PangoRectangle rectangle;

	if (item->priv->layout1) {
		g_object_unref(item->priv->layout1);
	}
	if (item->priv->layout2) {
		g_object_unref(item->priv->layout2);
	}

	item->priv->layout1 = gtk_widget_create_pango_layout(GTK_WIDGET(item), item->priv->text);
	pango_layout_set_width(item->priv->layout1, (width - MARGIN_LEFT - ICON_AREA_INDENT - get_icons_width(item) - MARGIN_RIGHT) * PANGO_SCALE);
	pango_layout_set_wrap(item->priv->layout1, PANGO_WRAP_WORD_CHAR);
	pango_layout_set_ellipsize(item->priv->layout1, PANGO_ELLIPSIZE_NONE);
	height = 0;
	for (item->priv->layout1_lines = 0;
	     (layout_line = pango_layout_get_line_readonly(item->priv->layout1, item->priv->layout1_lines));
	     item->priv->layout1_lines++) {
		pango_layout_line_get_pixel_extents(layout_line, NULL, &rectangle);
		if (height >= ICON_AREA_HEIGHT) {
			break;
		}
		height += rectangle.height;
	}
	if (layout_line) {
		item->priv->layout2 = gtk_widget_create_pango_layout(GTK_WIDGET(item), item->priv->text + layout_line->start_index);
		pango_layout_set_width(item->priv->layout2, (width - MARGIN_LEFT - MARGIN_RIGHT) * PANGO_SCALE);
		pango_layout_set_wrap(item->priv->layout2, PANGO_WRAP_WORD_CHAR);
		pango_layout_set_ellipsize(item->priv->layout2, PANGO_ELLIPSIZE_NONE);			
		pango_layout_get_pixel_extents(item->priv->layout2, NULL, &rectangle);
		height += rectangle.height;
	} else {
		item->priv->layout2 = NULL;
		item->priv->layout1_lines = 0;
	}
	item->priv->layout3_height = do_timestamp_layout(item, width);
	height += item->priv->layout3_height + MARGIN_TOP + MARGIN_BOTTOM;
	
	return height;
}

/* t1 = t1 - t2; */
static void substract_time(struct tm* t1, struct tm* t2) {
	t1->tm_sec -= t2->tm_sec;
	if (t1->tm_sec < 0) {
		t1->tm_sec += 60;
		t1->tm_min -= 1;
	}
	t1->tm_min -= t2->tm_min;
	if (t1->tm_min < 0) {
		t1->tm_min += 60;
		t1->tm_hour -= 1;
	}
	t1->tm_hour -= t2->tm_hour;
	if (t1->tm_hour < 0) {
		t1->tm_hour += 24;
		t1->tm_mday -= 1;
	}
	t1->tm_mday -= t2->tm_mday;
	if (t1->tm_mday < 0) {
		t1->tm_mday += 30; /* FIXME */
		t1->tm_mon -= 1;
	}		
	t1->tm_mon -= t2->tm_mon;
	if (t1->tm_mon < 0) {
		t1->tm_mon += 12;
		t1->tm_year -= 1;
	}
	t1->tm_year -= t2->tm_year;
}

static guint create_timestamp_text(MaukuItem* item, gchar** text_return) {
	GString* string;
	time_t t;
	struct tm now;
	struct tm tm;
	guint secs_to_next = 30;
	
	string = g_string_new("<small>");
	g_string_append(string, item->priv->sender);
	g_string_append(string, " in ");
	g_string_append(string, item->priv->publisher_part);
	t = time(NULL);
	gmtime_r(&t, &now);
	gmtime_r(&item->priv->timestamp, &tm);
	substract_time(&now, &tm);
	if (now.tm_year < 0) {
		g_string_append(string, " in future");
		secs_to_next = item->priv->timestamp - t;
	} else if (now.tm_year > 0) {
		if (now.tm_mon > 0) {
			g_string_append_printf(string, " %d %s, %d %s ago", now.tm_year, (now.tm_year == 1 ? "year" : "years"), now.tm_mon, (now.tm_mon == 1 ? "month" : "months"));
		} else {
			g_string_append_printf(string, " %d %s ago", now.tm_year, (now.tm_year == 1 ? "year" : "years"));
		}
		secs_to_next = 24 * 60 * 60 - (now.tm_hour * 60 * 60 + now.tm_min * 60 + now.tm_sec);
	} else if (now.tm_mon > 0) {
		if (now.tm_mday > 0) {
			g_string_append_printf(string, " %d %s, %d %s ago", now.tm_mon, (now.tm_mon == 1 ? "month" : "months"), now.tm_mday, (now.tm_mday == 1 ? "day" : "days"));
		} else {
			g_string_append_printf(string, " %d %s ago", now.tm_mon, (now.tm_mon == 1 ? "month" : "months"));
		}
		secs_to_next = 24 * 60 * 60 - (now.tm_hour * 60 * 60 + now.tm_min * 60 + now.tm_sec);
	} else if (now.tm_mday > 0) {
		if (now.tm_hour > 0) {
			g_string_append_printf(string, " %d %s, %d %s ago", now.tm_mday, (now.tm_mday == 1 ? "day" : "days"), now.tm_hour, (now.tm_hour == 1 ? "hour" : "hours"));
		} else {
			g_string_append_printf(string, " %d days ago", now.tm_mday);
		}
		secs_to_next = 60 * 60 - (now.tm_min * 60 + now.tm_sec);
	} else if (now.tm_hour > 0) {
		if (now.tm_min > 0) {
			g_string_append_printf(string, " %d %s, %d %s ago", now.tm_hour, (now.tm_hour == 1 ? "hour" : "hours"), now.tm_min, (now.tm_min == 1 ? "minute" : "minutes"));
		} else {
			g_string_append_printf(string, " %d %s ago", now.tm_hour, (now.tm_hour == 1 ? "hour" : "hours"));
		}
		secs_to_next = 60 - now.tm_sec;
	} else if (now.tm_min > 0) {
		g_string_append_printf(string, " %d %s ago", now.tm_min, (now.tm_min == 1 ? "minute" : "minutes"));
		secs_to_next = 60 - now.tm_sec;
	} else {
		g_string_append(string, " a moment ago");
		secs_to_next = 60 - now.tm_sec;
	}
	g_string_append(string, "</small>");

	*text_return = g_string_free(string, FALSE);

	return secs_to_next;
}


static gboolean on_update_timeout(gpointer data) {
	MaukuItem* item;

	/* gdk_threads_enter(); */
	item = MAUKU_ITEM(data);
	if (do_timestamp_layout(item, GTK_WIDGET(item)->allocation.width) != item->priv->layout3_height) {
		gtk_widget_queue_resize(GTK_WIDGET(item));
	} else {
		gtk_widget_queue_draw(GTK_WIDGET(item));
	}
	if (item->priv->buffer) {
		g_object_unref(item->priv->buffer);
		item->priv->buffer = NULL;
	}
	/* gdk_threads_leave(); */

	return FALSE;
}


static guint do_timestamp_layout(MaukuItem* item, guint width) {
	guint height;
	PangoContext* context;
	gchar* s;
	guint text_width;
	gint line;
	PangoLayoutLine* layout_line;
	PangoRectangle rectangle;

	if (item->priv->layout3) {
		g_object_unref(item->priv->layout3);
	}
	context = gtk_widget_get_pango_context(GTK_WIDGET(item));

	if (item->priv->timeout_id) {
		g_source_remove(item->priv->timeout_id);
	}
	item->priv->timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 1000 * create_timestamp_text(item, &s) + 250, on_update_timeout, item, NULL);
	item->priv->layout3 = pango_layout_new(context);
	pango_layout_set_markup(item->priv->layout3, s, -1);
	g_free(s);
	pango_layout_get_pixel_extents(item->priv->layout3, NULL, &rectangle);
	text_width = rectangle.width;
	height = rectangle.height;
	
	if (item->priv->layout1_lines) {
		item->priv->layout3_y = 0;
		for (line = 0; line < item->priv->layout1_lines; line++) {
			if ((layout_line = pango_layout_get_line_readonly(item->priv->layout1, line))) {
				pango_layout_line_get_pixel_extents(layout_line, NULL, &rectangle);
				item->priv->layout3_y += rectangle.height;
			}
		}
	} else {
		pango_layout_get_pixel_extents(item->priv->layout1, NULL, &rectangle);
		item->priv->layout3_y = rectangle.height;
	}	
	if (item->priv->layout2) {
		pango_layout_get_pixel_extents(item->priv->layout2, NULL, &rectangle);	
		item->priv->layout3_y += rectangle.height;
		layout_line = pango_layout_get_line_readonly(item->priv->layout2, pango_layout_get_line_count(item->priv->layout2) - 1);
		pango_layout_line_get_pixel_extents(layout_line, NULL, &rectangle);
		if (text_width < width - rectangle.width - MARGIN_LEFT - MARGIN_RIGHT - 16) {
			item->priv->layout3_x = rectangle.width + 16;
			item->priv->layout3_y -= height + 1;
			height = 0;
		} else {
			item->priv->layout3_x = 0;
		}
	} else {
		layout_line = pango_layout_get_line_readonly(item->priv->layout1, pango_layout_get_line_count(item->priv->layout1) - 1);
		pango_layout_line_get_pixel_extents(layout_line, NULL, &rectangle);
		if (text_width < width - rectangle.width - MARGIN_LEFT - ICON_AREA_INDENT - get_icons_width(item) - MARGIN_RIGHT - 16) {
			item->priv->layout3_x = rectangle.width + ICON_AREA_INDENT + 16;
			item->priv->layout3_y -= height + 1;
			height = 0;
		} else {
			item->priv->layout3_x = ICON_AREA_INDENT;
		}
	}
	
	return height;
}
