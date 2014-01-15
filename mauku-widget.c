
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#include "mauku-widget.h"
#include <string.h>

G_DEFINE_TYPE(MaukuWidget, mauku_widget, GTK_TYPE_WIDGET);

struct _MaukuWidgetPrivate {
	gint deviation_x;
	gint deviation_y;
	gint pending_deviation_x;
	gint pending_deviation_y;
	gint shown : 1;
};

static guint deviation_timeout_id;
static GList* deviating_widgets;

static void mauku_widget_realize(GtkWidget* widget);
static void mauku_widget_unrealize(GtkWidget* widget);
static void mauku_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static gboolean deviation_timeout(gpointer data);

void mauku_widget_add_deviation(MaukuWidget* mauku_widget, gint x, gint y) {
	g_return_if_fail(MAUKU_IS_WIDGET(mauku_widget));

	mauku_widget->priv->pending_deviation_x += x;
	mauku_widget->priv->pending_deviation_y += y;
}

gboolean mauku_widget_is_shown(MaukuWidget* mauku_widget) {
	g_return_val_if_fail(MAUKU_IS_WIDGET(mauku_widget), FALSE);
	
	return (mauku_widget->priv->shown ? TRUE : FALSE);
}

static void mauku_widget_class_init(MaukuWidgetClass* klass) {
	GtkWidgetClass* gtk_widget_class;
	GObjectClass* object_class;

	gtk_widget_class = GTK_WIDGET_CLASS(klass);
	gtk_widget_class->realize = mauku_widget_realize;
	gtk_widget_class->unrealize = mauku_widget_unrealize;
	gtk_widget_class->size_allocate = mauku_widget_size_allocate;
	
	object_class = G_OBJECT_CLASS(klass);
	g_type_class_add_private (object_class, sizeof(MaukuWidgetPrivate));
}

static void mauku_widget_init(MaukuWidget* item) {
	item->priv = G_TYPE_INSTANCE_GET_PRIVATE(item, MAUKU_TYPE_WIDGET, MaukuWidgetPrivate);

	GTK_WIDGET_UNSET_FLAGS(item, GTK_NO_WINDOW);
}

static void mauku_widget_realize(GtkWidget* widget) {
	MaukuWidget* mauku_widget;
	GdkWindowAttr attributes;
	gint attributes_mask;
	gboolean visible_window;

	mauku_widget = MAUKU_WIDGET(widget);

	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) | GDK_BUTTON_MOTION_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_EXPOSURE_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
	gdk_window_set_user_data (widget->window, widget);
	gdk_window_show(widget->window);

	widget->style = gtk_style_attach (widget->style, widget->window);
	gtk_style_set_background (widget->style, widget->window, GTK_STATE_NORMAL);
}

static void mauku_widget_unrealize(GtkWidget* widget) {
	MaukuWidget* mauku_widget;
	
	mauku_widget = MAUKU_WIDGET(widget);

	if (mauku_widget->priv->deviation_x || mauku_widget->priv->deviation_y) {
		deviating_widgets = g_list_remove(deviating_widgets, widget);
	}

	if (GTK_WIDGET_CLASS(mauku_widget_parent_class)->unrealize) {
		GTK_WIDGET_CLASS(mauku_widget_parent_class)->unrealize(widget);
	}
}
static void mauku_widget_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
	MaukuWidget* mauku_widget;
	gboolean deviating;
	
	mauku_widget = MAUKU_WIDGET(widget);

	if(GTK_WIDGET_REALIZED(widget)) {
		deviating = mauku_widget->priv->deviation_x || mauku_widget->priv->deviation_y;
		
		mauku_widget->priv->deviation_x += mauku_widget->priv->pending_deviation_x;
		mauku_widget->priv->deviation_y += mauku_widget->priv->pending_deviation_y;
		mauku_widget->priv->pending_deviation_x = 0;
		mauku_widget->priv->pending_deviation_y = 0;
	
		if (!deviating && (mauku_widget->priv->deviation_x || mauku_widget->priv->deviation_y)) {
			deviating_widgets = g_list_prepend(deviating_widgets, widget);
			if (!deviation_timeout_id) {
				deviation_timeout_id = g_timeout_add(40, deviation_timeout, mauku_widget);
			}
			gdk_window_raise(widget->window);
		}

		gdk_window_move_resize(widget->window,
	                	       allocation->x + mauku_widget->priv->deviation_x,
				       allocation->y + mauku_widget->priv->deviation_y,
				       allocation->width,
				       allocation->height);
		
		mauku_widget->priv->shown = TRUE;
	}
	widget->allocation = *allocation;
}

static gboolean deviation_timeout(gpointer data) {
	MaukuWidget* mauku_widget;
	GList* current;
	GList* next;
	GtkWidget* widget;
	gboolean again;
	
	current = deviating_widgets;
	while (current) {
		next = current->next;
		
		mauku_widget = MAUKU_WIDGET(current->data);
		widget = GTK_WIDGET(mauku_widget);

		if (mauku_widget->priv->deviation_x < 0) {
			if (mauku_widget->priv->deviation_x < -250) {
				mauku_widget->priv->deviation_x += 50;
			} else {
				mauku_widget->priv->deviation_x += -mauku_widget->priv->deviation_x / 5 + 1;
			}
			if (mauku_widget->priv->deviation_x > 0) {
				mauku_widget->priv->deviation_x = 0;
			}
		} else if (mauku_widget->priv->deviation_x > 0) {
			if (mauku_widget->priv->deviation_x > 250) {
				mauku_widget->priv->deviation_x -= 50;
			} else {
				mauku_widget->priv->deviation_x -= mauku_widget->priv->deviation_x / 5 + 1;
			}
			if (mauku_widget->priv->deviation_x < 0) {
				mauku_widget->priv->deviation_x = 0;
			}
		}
		if (mauku_widget->priv->deviation_y < 0) {
			if (mauku_widget->priv->deviation_y < -250) {
				mauku_widget->priv->deviation_y += 50;
			} else {
				mauku_widget->priv->deviation_y += -mauku_widget->priv->deviation_y / 5 + 1;
			}
			if (mauku_widget->priv->deviation_y > 0) {
				mauku_widget->priv->deviation_y = 0;
			}
		} else if (mauku_widget->priv->deviation_y > 0) {
			if (mauku_widget->priv->deviation_y > 250) {
				mauku_widget->priv->deviation_y -= 50;
			} else {
				mauku_widget->priv->deviation_y -= mauku_widget->priv->deviation_y / 5 + 1;
			}
			if (mauku_widget->priv->deviation_y < 0) {
				mauku_widget->priv->deviation_y = 0;
			}
		}
		gdk_window_move(widget->window,
	                        widget->allocation.x + mauku_widget->priv->deviation_x,
		                widget->allocation.y + mauku_widget->priv->deviation_y);
		if (!mauku_widget->priv->deviation_x && ! mauku_widget->priv->deviation_y) {
			deviating_widgets = g_list_delete_link(deviating_widgets, current);
		}
		
		current = next;
	}

	if (deviating_widgets) {
		again = TRUE;
	} else {
		again = FALSE;
		deviation_timeout_id = 0;
	}

	return again;
}
