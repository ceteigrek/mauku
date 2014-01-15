
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef MAUKU_SCROLLING_BOX_H
#define MAUKU_SCROLLING_BOX_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

typedef struct _MaukuScrollingBox MaukuScrollingBox;
typedef struct _MaukuScrollingBoxClass MaukuScrollingBoxClass;
typedef struct _MaukuScrollingBoxPrivate MaukuScrollingBoxPrivate;

struct _MaukuScrollingBox {
	GtkContainer parent;

	/*< private >*/
	gboolean horizontal;
	GList* children;
	GdkWindow* scrolling_window;
	GtkAdjustment *hadjustment;
	GtkAdjustment *vadjustment;
};

struct _MaukuScrollingBoxClass {
	GtkContainerClass parent;

	void (*set_scroll_adjustments)(MaukuScrollingBox* scrolling_box, GtkAdjustment* hadjustment, GtkAdjustment* vadjustment);
};

GType mauku_scrolling_box_get_type(void);

#define MAUKU_TYPE_SCROLLING_BOX (mauku_scrolling_box_get_type())
#define MAUKU_SCROLLING_BOX(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), MAUKU_TYPE_SCROLLING_BOX, MaukuScrollingBox))
#define MAUKU_SCROLLNG_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), MAUKU_TYPE_SCROLLING_BOX, MaukuScrollingBoxClass))
#define MAUKU_IS_SCROLLING_BOX(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MAUKU_TYPE_SCROLLING_BOX))
#define MAUKU_IS_SCROLLING_BOX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MAUKU_TYPE_SCROLLING_BOX))
#define MAUKU_SCROLLING_BOX_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), MAUKU_TYPE_SCROLLING_BOX, MaukuScrollingBoxClass))

GtkWidget* mauku_scrolling_box_new_horizontal(GtkAdjustment* hadjustment, GtkAdjustment* vadjustment);
GtkWidget* mauku_scrolling_box_new_vertical(GtkAdjustment* hadjustment, GtkAdjustment* vadjustment);

void mauku_scrolling_box_add_after(MaukuScrollingBox* box, GtkWidget* child, GtkWidget* existing_child);
void mauku_scrolling_box_add_before(MaukuScrollingBox* box, GtkWidget* child, GtkWidget* existing_child);

#endif
