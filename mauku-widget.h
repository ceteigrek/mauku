
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef __MAUKU_WIDGET_H__
#define __MAUKU_WIDGET_H__

#include <gtk/gtk.h>

#define MAUKU_TYPE_WIDGET (mauku_widget_get_type ())
#define MAUKU_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), MAUKU_TYPE_WIDGET, MaukuWidget))
#define MAUKU_IS_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), MAUKU_TYPE_WIDGET))
#define MAUKU_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), MAUKU_WIDGET, MaukuWidgetClass))
#define MAUKU_IS_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), MAUKU_TYPE_WIDGET))
#define MAUKU_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), MAUKU_TYPE_WIDGET, MaukuWidgetClass))

typedef struct _MaukuWidgetPrivate MaukuWidgetPrivate;

typedef struct _MaukuWidget
{
	GtkWidget parent_instance;
	MaukuWidgetPrivate* priv;
} MaukuWidget;

typedef struct _MaukuWidgetClass
{
	GtkWidgetClass parent_class;
} MaukuWidgetClass;

void mauku_widget_add_deviation(MaukuWidget* mauku_widget, gint x, gint y);
gboolean mauku_widget_is_shown(MaukuWidget* mauku_widget);

#endif
