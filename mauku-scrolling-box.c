
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
 
#include <glib.h>
#include <gtk/gtk.h>

#include "mauku-scrolling-box.h"
#include "miaouwmarshalers.h"
#include "mauku-widget.h"

G_DEFINE_TYPE(MaukuScrollingBox, mauku_scrolling_box, GTK_TYPE_CONTAINER);

enum {
	SIGNAL_CHILD_ADDED,
	SIGNAL_CHILD_REMOVED,
	SIGNAL_COUNT
};

static void mauku_scrolling_box_destroy(GtkObject* object);
static void mauku_scrolling_box_finalize(GObject* object);
static gboolean mauku_scrolling_box_expose_event(GtkWidget* widget, GdkEventExpose* event);
static void mauku_scrolling_box_realize(GtkWidget* widget);
static void mauku_scrolling_box_unrealize(GtkWidget* widget);
static void mauku_scrolling_box_add(GtkContainer* container, GtkWidget* child);
static void mauku_scrolling_box_remove(GtkContainer* container, GtkWidget* child);
static GType mauku_scrolling_box_child_type(GtkContainer* container);
static void mauku_scrolling_box_forall(GtkContainer* container, gboolean include_internals, GtkCallback callback, gpointer callback_data);
static void mauku_scrolling_box_style_set(GtkWidget* widget, GtkStyle* previous_style);
static void mauku_scrolling_box_size_allocate(GtkWidget* widget, GtkAllocation* allocation);
static void mauku_scrolling_box_size_request(GtkWidget* widget, GtkRequisition* requisition);
static void mauku_scrolling_box_set_scroll_adjustments(MaukuScrollingBox* scrolling_box, GtkAdjustment* hadjustment, GtkAdjustment* vadjustment);

static void set_adjustment_values(GtkAdjustment* adjustment, gint total_size, gint viewable_size);
static void connect_adjustment(MaukuScrollingBox* scrolling_box, GtkAdjustment* adjustment);
static void disconnect_adjustment(MaukuScrollingBox* scrolling_box, GtkAdjustment* adjustment);
static GdkWindow* create_window(GtkWidget* widget, GdkWindow* parent, gint x, gint y, gint width, gint height);
static gboolean is_visible(gint widget_start, gint widget_end, gint page_start, gint page_end);

static guint signals[SIGNAL_COUNT];

GtkWidget* mauku_scrolling_box_new_horizontal(GtkAdjustment* hadjustment, GtkAdjustment* vadjustment) {
	GtkWidget* scrolling_box;
	
	scrolling_box = gtk_widget_new(MAUKU_TYPE_SCROLLING_BOX, NULL);
	MAUKU_SCROLLING_BOX(scrolling_box)->horizontal = TRUE;
	mauku_scrolling_box_set_scroll_adjustments(MAUKU_SCROLLING_BOX(scrolling_box), hadjustment, vadjustment);
	
	return scrolling_box;
}

GtkWidget* mauku_scrolling_box_new_vertical(GtkAdjustment* hadjustment, GtkAdjustment* vadjustment) {
	GtkWidget* scrolling_box;
	
	scrolling_box = gtk_widget_new(MAUKU_TYPE_SCROLLING_BOX, NULL);
	mauku_scrolling_box_set_scroll_adjustments(MAUKU_SCROLLING_BOX(scrolling_box), hadjustment, vadjustment);

	return scrolling_box;
}

void mauku_scrolling_box_add_before(MaukuScrollingBox* scrolling_box, GtkWidget* child, GtkWidget* existing_child) {
	GList* existing_child_in_list;

	g_return_if_fail(MAUKU_IS_SCROLLING_BOX(scrolling_box));
	g_return_if_fail(GTK_IS_WIDGET(child));
	g_return_if_fail(child->parent == NULL);
	g_return_if_fail(existing_child == NULL || GTK_IS_WIDGET(existing_child));
	g_return_if_fail(existing_child == NULL || existing_child->parent == (GtkWidget*)scrolling_box);
	
	if (existing_child) {
		existing_child_in_list = g_list_find(scrolling_box->children, existing_child);
		g_return_if_fail(existing_child_in_list != NULL);

		scrolling_box->children = g_list_insert_before(scrolling_box->children, existing_child_in_list, child);
	} else {
		scrolling_box->children = g_list_prepend(scrolling_box->children, child);
	}
	if (scrolling_box->scrolling_window) {
		gtk_widget_set_parent_window(child, scrolling_box->scrolling_window);
	}
	gtk_widget_set_parent(child, GTK_WIDGET(scrolling_box));
	
	if (GTK_WIDGET_VISIBLE(child) && GTK_WIDGET_VISIBLE(scrolling_box)) {
		gtk_widget_queue_resize(child);
	}

	g_signal_emit(scrolling_box, signals[SIGNAL_CHILD_ADDED], 0, child);
}

void mauku_scrolling_box_add_after(MaukuScrollingBox* scrolling_box, GtkWidget* child, GtkWidget* existing_child) {
	GList* existing_child_in_list;

	g_return_if_fail(MAUKU_IS_SCROLLING_BOX(scrolling_box));
	g_return_if_fail(GTK_IS_WIDGET(child));
	g_return_if_fail(child->parent == NULL);
	g_return_if_fail(existing_child == NULL || GTK_IS_WIDGET(existing_child));
	g_return_if_fail(existing_child == NULL || existing_child->parent == (GtkWidget*)scrolling_box);

	if (existing_child) {
		existing_child_in_list = g_list_find(scrolling_box->children, existing_child);
		g_return_if_fail(existing_child_in_list != NULL);

		if ((existing_child_in_list = g_list_next(existing_child_in_list))) {
			scrolling_box->children = g_list_insert_before(scrolling_box->children, existing_child_in_list, child);
		} else {
			scrolling_box->children = g_list_append(scrolling_box->children, child);
		}
	} else {
		scrolling_box->children = g_list_append(scrolling_box->children, child);
	}
	if (scrolling_box->scrolling_window) {
		gtk_widget_set_parent_window(child, scrolling_box->scrolling_window);
	}
	gtk_widget_set_parent(child, GTK_WIDGET(scrolling_box));

	if (GTK_WIDGET_VISIBLE(child) && GTK_WIDGET_VISIBLE(scrolling_box)) {
		gtk_widget_queue_resize(child);
	}

	g_signal_emit(scrolling_box, signals[SIGNAL_CHILD_ADDED], 0, child);
}

static void mauku_scrolling_box_class_init(MaukuScrollingBoxClass* scrolling_box_class) {
	GtkContainerClass *container_class;
	GtkWidgetClass *widget_class;
	GtkObjectClass* gtk_object_class;
	GObjectClass* object_class;
	
	scrolling_box_class->set_scroll_adjustments = mauku_scrolling_box_set_scroll_adjustments;

	container_class = GTK_CONTAINER_CLASS(scrolling_box_class);
	container_class->add = mauku_scrolling_box_add;
	container_class->remove = mauku_scrolling_box_remove;
	container_class->forall = mauku_scrolling_box_forall;
	container_class->child_type = mauku_scrolling_box_child_type;

	widget_class = GTK_WIDGET_CLASS(scrolling_box_class);
	widget_class->size_request = mauku_scrolling_box_size_request;
	widget_class->size_allocate = mauku_scrolling_box_size_allocate;
	widget_class->realize = mauku_scrolling_box_realize;
	widget_class->unrealize = mauku_scrolling_box_unrealize;
	widget_class->expose_event = mauku_scrolling_box_expose_event;
	widget_class->style_set = mauku_scrolling_box_style_set;

	gtk_object_class = GTK_OBJECT_CLASS(scrolling_box_class);
	gtk_object_class->destroy = mauku_scrolling_box_destroy;

	object_class = G_OBJECT_CLASS(scrolling_box_class);
	object_class->finalize = mauku_scrolling_box_finalize;

	widget_class->set_scroll_adjustments_signal = g_signal_new("set_scroll_adjustments", G_OBJECT_CLASS_TYPE(object_class),
	 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION, G_STRUCT_OFFSET(MaukuScrollingBoxClass, set_scroll_adjustments),
	 NULL, NULL, miaouw_marshal_VOID__OBJECT_OBJECT, G_TYPE_NONE, 2, GTK_TYPE_ADJUSTMENT, GTK_TYPE_ADJUSTMENT);

	signals[SIGNAL_CHILD_ADDED] = g_signal_new("child-added", MAUKU_TYPE_SCROLLING_BOX, 0, 0, 
	                                           NULL, NULL,
	                                           g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
					           GTK_TYPE_WIDGET);

	signals[SIGNAL_CHILD_REMOVED] = g_signal_new("child-removed", MAUKU_TYPE_SCROLLING_BOX, 0, 0, 
	                                             NULL, NULL,
	                                             g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE, 1,
				  	             GTK_TYPE_WIDGET);
}

static void mauku_scrolling_box_init(MaukuScrollingBox* scrolling_box) {
	GTK_WIDGET_UNSET_FLAGS(scrolling_box, GTK_NO_WINDOW);
	gtk_widget_set_redraw_on_allocate(GTK_WIDGET(scrolling_box), FALSE);
	gtk_widget_set_double_buffered(GTK_WIDGET(scrolling_box), FALSE);
	gtk_container_set_reallocate_redraws(GTK_CONTAINER(scrolling_box), FALSE);
	gtk_container_set_resize_mode(GTK_CONTAINER(scrolling_box), GTK_RESIZE_QUEUE);
}

static void mauku_scrolling_box_destroy(GtkObject* object) {
	MaukuScrollingBox* scrolling_box;
	
	scrolling_box = MAUKU_SCROLLING_BOX(object);
	if (scrolling_box->hadjustment) {
		disconnect_adjustment(scrolling_box, scrolling_box->hadjustment);
		scrolling_box->hadjustment = NULL;
	}
	if (scrolling_box->vadjustment) {
		disconnect_adjustment(scrolling_box, scrolling_box->vadjustment);
		scrolling_box->vadjustment = NULL;
	}

	GTK_OBJECT_CLASS(g_type_class_peek_parent(g_type_class_peek(MAUKU_TYPE_SCROLLING_BOX)))->destroy(object);
}

static void mauku_scrolling_box_finalize(GObject* object) {
	MaukuScrollingBox* scrolling_box;
	
	scrolling_box = MAUKU_SCROLLING_BOX(object);
	if (scrolling_box->hadjustment) {
		disconnect_adjustment(scrolling_box, scrolling_box->hadjustment);
		scrolling_box->hadjustment = NULL;
	}
	if (scrolling_box->vadjustment) {
		disconnect_adjustment(scrolling_box, scrolling_box->vadjustment);
		scrolling_box->vadjustment = NULL;
	}

	G_OBJECT_CLASS(g_type_class_peek_parent(g_type_class_peek(MAUKU_TYPE_SCROLLING_BOX)))->finalize(object);
}

static void mauku_scrolling_box_realize(GtkWidget* widget) {
	MaukuScrollingBox* scrolling_box;
	GtkAllocation allocation;
	GdkWindowAttr attributes;
	gint attributes_mask;

	GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);

	scrolling_box = MAUKU_SCROLLING_BOX(widget);
	
	attributes.x = widget->allocation.x + GTK_CONTAINER(widget)->border_width;
	attributes.y = widget->allocation.y + GTK_CONTAINER(widget)->border_width;
	attributes.width = widget->allocation.width - GTK_CONTAINER(widget)->border_width * 2;
	attributes.height = widget->allocation.height - GTK_CONTAINER(widget)->border_width * 2;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;

	widget->window = gdk_window_new(gtk_widget_get_parent_window(widget), &attributes, attributes_mask);
	gdk_window_set_user_data(widget->window, scrolling_box);

	attributes.x = -scrolling_box->hadjustment->value;
	attributes.y = -scrolling_box->vadjustment->value;
	attributes.width = scrolling_box->hadjustment->upper;
	attributes.height = scrolling_box->vadjustment->upper;

	scrolling_box->scrolling_window = gdk_window_new(widget->window, &attributes, attributes_mask);
	gdk_window_set_user_data(scrolling_box->scrolling_window, scrolling_box);
	
	g_list_foreach(scrolling_box->children, (GFunc)gtk_widget_set_parent_window, scrolling_box->scrolling_window);

	widget->style = gtk_style_attach(widget->style, widget->window);
	gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);
	gtk_style_set_background(widget->style, scrolling_box->scrolling_window, GTK_STATE_NORMAL);

	gtk_paint_flat_box(widget->style, scrolling_box->scrolling_window, GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL,
	                   widget, "maukuscrollingboxwindow", 0, 0, -1, -1);

	gdk_window_show(scrolling_box->scrolling_window);
}

static void mauku_scrolling_box_unrealize(GtkWidget* widget) {
	MaukuScrollingBox* scrolling_box;
	GtkWidgetClass* widget_class;
	
	scrolling_box = MAUKU_SCROLLING_BOX(widget);
	
	gdk_window_set_user_data(scrolling_box->scrolling_window, NULL);
	gdk_window_destroy(scrolling_box->scrolling_window);
	scrolling_box->scrolling_window = NULL;

	widget_class = GTK_WIDGET_CLASS(g_type_class_peek_parent(g_type_class_peek(MAUKU_TYPE_SCROLLING_BOX)));
	if (widget_class->unrealize) {
		widget_class->unrealize(widget);
	}
}

static void mauku_scrolling_box_add(GtkContainer* container, GtkWidget* child) {
	mauku_scrolling_box_add_after(MAUKU_SCROLLING_BOX(container), child, NULL);
}

static void mauku_scrolling_box_remove(GtkContainer* container, GtkWidget* child) {
	MaukuScrollingBox* scrolling_box;
	GList* child_in_list;
	gboolean was_visible;
	
	scrolling_box = MAUKU_SCROLLING_BOX(container);
	g_return_if_fail(GTK_IS_WIDGET(child));
	g_return_if_fail(child->parent == (GtkWidget*)scrolling_box);
	
	was_visible = GTK_WIDGET_VISIBLE(child);

	child_in_list = g_list_find(scrolling_box->children, child);
	g_return_if_fail(child_in_list != NULL);
	scrolling_box->children = g_list_delete_link(scrolling_box->children, child_in_list);
	gtk_widget_unparent(child);
	gtk_widget_set_parent_window(child, NULL);

	if (was_visible) {
		gtk_widget_queue_resize(GTK_WIDGET(container));
	}

	g_signal_emit(scrolling_box, signals[SIGNAL_CHILD_REMOVED], 0, child);
}

static GType mauku_scrolling_box_child_type(GtkContainer* container) {
	return GTK_TYPE_WIDGET;
}

static void mauku_scrolling_box_forall(GtkContainer* container, gboolean include_internals, GtkCallback callback, gpointer callback_data) {
	MaukuScrollingBox* scrolling_box;
	GList* children;
	
	scrolling_box = MAUKU_SCROLLING_BOX(container);
	g_return_if_fail(callback != NULL);
	
	g_list_foreach(scrolling_box->children, (GFunc)callback, callback_data);
}

static void mauku_scrolling_box_style_set(GtkWidget* widget, GtkStyle* previous_style) {
	MaukuScrollingBox *scrolling_box;

	if (GTK_WIDGET_REALIZED(widget) && !GTK_WIDGET_NO_WINDOW(widget)) {
		scrolling_box = MAUKU_SCROLLING_BOX(widget);
		gtk_style_set_background (widget->style, scrolling_box->scrolling_window, GTK_STATE_NORMAL);
		gtk_style_set_background (widget->style, widget->window, widget->state);
	}
}

static void mauku_scrolling_box_size_allocate(GtkWidget* widget, GtkAllocation* allocation) {
	MaukuScrollingBox* scrolling_box;
	GList* children;
	GtkWidget* child;
	GtkAllocation child_allocation;
	GtkRequisition child_requisition;
	gint difference;
	
	scrolling_box = MAUKU_SCROLLING_BOX(widget);

	if (scrolling_box->horizontal) {
		scrolling_box->hadjustment->lower = 0.0;
		scrolling_box->hadjustment->upper = widget->requisition.width - 2 * GTK_CONTAINER(scrolling_box)->border_width;
		scrolling_box->hadjustment->page_size = allocation->width - 2 * GTK_CONTAINER(scrolling_box)->border_width;
		scrolling_box->hadjustment->step_increment = 0.1 * scrolling_box->hadjustment->page_size;
		scrolling_box->hadjustment->page_increment = 0.9 * scrolling_box->hadjustment->page_size;
		set_adjustment_values(scrolling_box->vadjustment, allocation->height - 2 * GTK_CONTAINER(scrolling_box)->border_width, GTK_WIDGET(scrolling_box)->allocation.height - 2 * GTK_CONTAINER(scrolling_box)->border_width);
	} else {
		set_adjustment_values(scrolling_box->hadjustment, allocation->width - 2 * GTK_CONTAINER(scrolling_box)->border_width, GTK_WIDGET(scrolling_box)->allocation.width - 2 * GTK_CONTAINER(scrolling_box)->border_width);
		scrolling_box->vadjustment->lower = 0.0;
		scrolling_box->vadjustment->upper = widget->requisition.height - 2 * GTK_CONTAINER(scrolling_box)->border_width;
		scrolling_box->vadjustment->page_size = allocation->height - 2 * GTK_CONTAINER(scrolling_box)->border_width;
		scrolling_box->vadjustment->step_increment = 0.1 * scrolling_box->vadjustment->page_size;
		scrolling_box->vadjustment->page_increment = 0.9 * scrolling_box->vadjustment->page_size;
	}

	child_allocation.x = 0;
	child_allocation.y = 0;
	if (scrolling_box->horizontal) {
		child_allocation.height = MAX(1, (gint)allocation->height - (gint)GTK_CONTAINER(widget)->border_width * 2);
	} else {
		child_allocation.width = MAX(1, (gint)allocation->width - (gint)GTK_CONTAINER(widget)->border_width * 2);
	}

	difference = 0;
	for (children = scrolling_box->children; children; children = children->next) {
		child = GTK_WIDGET(children->data);
		if (GTK_WIDGET_VISIBLE(child)) {
			gtk_widget_get_child_requisition(child, &child_requisition);
			if (MAUKU_IS_WIDGET(child)) {		
				if (mauku_widget_is_shown(MAUKU_WIDGET(child))) {
					if (scrolling_box->horizontal) {
						if (child_allocation.x - child->allocation.x != difference &&
						    (is_visible(child->allocation.x,
						        	child->allocation.x + child_requisition.width,
								scrolling_box->hadjustment->value,
								scrolling_box->hadjustment->value + scrolling_box->hadjustment->page_size) ||
						     is_visible(child_allocation.x,
						        	child_allocation.x + child_requisition.width,
								scrolling_box->hadjustment->value,
								scrolling_box->hadjustment->value + scrolling_box->hadjustment->page_size))) {
							mauku_widget_add_deviation(MAUKU_WIDGET(child), child_allocation.x - child->allocation.x, 0);
						}
					} else {
						if (child_allocation.y - child->allocation.y != difference &&
						    (is_visible(child->allocation.y,
						        	child->allocation.y + child_requisition.height,
								scrolling_box->vadjustment->value,
								scrolling_box->vadjustment->value + scrolling_box->vadjustment->page_size) ||
						     is_visible(child_allocation.y,
						        	child_allocation.y + child_requisition.height,
								scrolling_box->vadjustment->value,
								scrolling_box->vadjustment->value + scrolling_box->vadjustment->page_size))) {
							mauku_widget_add_deviation(MAUKU_WIDGET(child), 0, child->allocation.y - child_allocation.y);
						}
					}
				} else {
					if (scrolling_box->horizontal) {
						mauku_widget_add_deviation(MAUKU_WIDGET(child), -scrolling_box->hadjustment->page_size, 0);
					} else {
						mauku_widget_add_deviation(MAUKU_WIDGET(child), 0, -scrolling_box->vadjustment->page_size);
					}
				}
			}
			
			if (scrolling_box->horizontal) {
				child_allocation.width = child_requisition.width;
			} else {
				child_allocation.height = child_requisition.height;
				if (child_allocation.y <= scrolling_box->vadjustment->value) {
					difference = child_allocation.y - child->allocation.y;
				}
			}
			gtk_widget_size_allocate(child, &child_allocation);

			if (scrolling_box->horizontal) {
				child_allocation.x += child_requisition.width;
				if (child_allocation.x - difference <= scrolling_box->hadjustment->value &&
				    scrolling_box->hadjustment->value + child_requisition.width < scrolling_box->hadjustment->upper - scrolling_box->hadjustment->page_size) {
					scrolling_box->hadjustment->value += child_requisition.width;
					difference += child_requisition.width;
				}
			} else {
				child_allocation.y += child_requisition.height;
			}
		}
	}
					
	widget->allocation = *allocation;
	if (difference) {
	printf("%d diff\n", difference);
		if (scrolling_box->horizontal) {
			scrolling_box->hadjustment->value += difference;
			gtk_adjustment_changed(scrolling_box->hadjustment);
		} else {
			scrolling_box->vadjustment->value += difference;
			gtk_adjustment_changed(scrolling_box->vadjustment);
		}
	}
	gtk_adjustment_value_changed(scrolling_box->horizontal ? scrolling_box->hadjustment : scrolling_box->vadjustment);

	if (GTK_WIDGET_REALIZED(widget)) {
		gdk_window_move_resize(widget->window,
		                       allocation->x + GTK_CONTAINER(widget)->border_width,
		                       allocation->y + GTK_CONTAINER(widget)->border_width,
		                       allocation->width - 2 * GTK_CONTAINER(widget)->border_width,
				       allocation->height - 2 * GTK_CONTAINER(widget)->border_width);	
	}
		gdk_window_process_updates(scrolling_box->scrolling_window, TRUE);

}

static void mauku_scrolling_box_size_request(GtkWidget* widget, GtkRequisition* requisition) {
	MaukuScrollingBox* scrolling_box;
	GList* children;
	GtkRequisition child_requisition;
	GtkWidget* child;
	gint size_ratio;
	
	scrolling_box = MAUKU_SCROLLING_BOX(widget);
	
	requisition->width = requisition->height = 0;
	for (children = scrolling_box->children; children; children = children->next) {
		child = GTK_WIDGET(children->data);
		if (GTK_WIDGET_VISIBLE(child)) {
			gtk_widget_size_request(child, &child_requisition);
			if (scrolling_box->horizontal) {
				requisition->width += child_requisition.width;
				requisition->height = MAX(requisition->height, child_requisition.height);
			} else {
				requisition->height += child_requisition.height;
				requisition->width = MAX(requisition->width, child_requisition.width);			
			}
		}
	}
	requisition->width += GTK_CONTAINER(widget)->border_width * 2;
	requisition->height += GTK_CONTAINER(widget)->border_width * 2;
}

static gboolean mauku_scrolling_box_expose_event(GtkWidget* widget, GdkEventExpose* event) {
 	MaukuScrollingBox* scrolling_box;
	GtkWidgetClass* widget_class;

	if (GTK_WIDGET_DRAWABLE(widget)) {
		scrolling_box = MAUKU_SCROLLING_BOX(widget);
		if (event->window == scrolling_box->scrolling_window) {
			gtk_paint_flat_box(widget->style, scrolling_box->scrolling_window, GTK_STATE_NORMAL, GTK_SHADOW_NONE, &event->area,
			                   widget, "maukuscrollingboxwindow", 0, 0, -1, -1);
			widget_class = GTK_WIDGET_CLASS(g_type_class_peek_parent(g_type_class_peek(MAUKU_TYPE_SCROLLING_BOX)));
			if (widget_class->expose_event) {
				widget_class->expose_event(widget, event);
			}
		}
	}

	return FALSE;
}

static void mauku_scrolling_box_set_scroll_adjustments(MaukuScrollingBox* scrolling_box, GtkAdjustment* hadjustment, GtkAdjustment* vadjustment) {
	gint size;
	GList* children;
	GtkWidget* child;

	disconnect_adjustment(scrolling_box, scrolling_box->hadjustment);
	disconnect_adjustment(scrolling_box, scrolling_box->vadjustment);

	if (!hadjustment) {
		hadjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0.0, 0.0, 0.0, 0.0, 0.0));
	}
	if (!vadjustment) {
		vadjustment = GTK_ADJUSTMENT(gtk_adjustment_new(0, 0.0, 0.0, 0.0, 0.0, 0.0));
	}
	scrolling_box->hadjustment = hadjustment;
	scrolling_box->vadjustment = vadjustment;

	scrolling_box->hadjustment->value = 0.0;
	scrolling_box->vadjustment->value = 0.0;

	if (scrolling_box->horizontal) {
		size = 0;
		for (children = scrolling_box->children; children; children = children->next) {
			child = GTK_WIDGET(children->data);
			if (GTK_WIDGET_VISIBLE(child)) {
				size += child->allocation.width;
			}
		}
		set_adjustment_values(scrolling_box->hadjustment, size, GTK_WIDGET(scrolling_box)->allocation.width - 2 * GTK_CONTAINER(scrolling_box)->border_width);
		set_adjustment_values(scrolling_box->vadjustment, GTK_WIDGET(scrolling_box)->allocation.height - 2 * GTK_CONTAINER(scrolling_box)->border_width, GTK_WIDGET(scrolling_box)->allocation.height - 2 * GTK_CONTAINER(scrolling_box)->border_width);
	} else {
		size = 0;
		for (children = scrolling_box->children; children; children = children->next) {
			child = GTK_WIDGET(children->data);
			if (GTK_WIDGET_VISIBLE(child)) {
				size += child->allocation.height;
			}
		}
		set_adjustment_values(scrolling_box->hadjustment, GTK_WIDGET(scrolling_box)->allocation.width - 2 * GTK_CONTAINER(scrolling_box)->border_width, GTK_WIDGET(scrolling_box)->allocation.width - 2 * GTK_CONTAINER(scrolling_box)->border_width);
		set_adjustment_values(scrolling_box->vadjustment, size, GTK_WIDGET(scrolling_box)->allocation.height - 2 * GTK_CONTAINER(scrolling_box)->border_width);
	}
	connect_adjustment(scrolling_box, hadjustment);
	connect_adjustment(scrolling_box, vadjustment);
}

static void on_adjustment_value_changed(GtkAdjustment* adjustment, gpointer data) {
	MaukuScrollingBox* scrolling_box;
	GdkRectangle rectangle;

	g_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
	g_return_if_fail(MAUKU_IS_SCROLLING_BOX(data));

	scrolling_box = MAUKU_SCROLLING_BOX(data);
	if (GTK_WIDGET_REALIZED(GTK_WIDGET(scrolling_box))) {
		rectangle.x = 0;
		rectangle.y = 0;
		rectangle.width = scrolling_box->hadjustment->upper;
		rectangle.height = scrolling_box->vadjustment->upper;
		gdk_window_move_resize(scrolling_box->scrolling_window,
			               -scrolling_box->hadjustment->value, -scrolling_box->vadjustment->value,
		                       scrolling_box->hadjustment->upper, scrolling_box->vadjustment->upper);
		gdk_window_process_updates(scrolling_box->scrolling_window, TRUE);
	}
}

static void connect_adjustment(MaukuScrollingBox* scrolling_box, GtkAdjustment* adjustment) {
	g_signal_connect(adjustment, "value_changed", G_CALLBACK(on_adjustment_value_changed), scrolling_box);
	g_object_ref_sink(adjustment);
}

static void disconnect_adjustment(MaukuScrollingBox* scrolling_box, GtkAdjustment* adjustment) {
	if (adjustment) {
		g_signal_handlers_disconnect_by_func(adjustment, G_CALLBACK(on_adjustment_value_changed), scrolling_box);
		g_object_unref(adjustment);
	}
}

static void set_adjustment_values(GtkAdjustment* adjustment, gint total_size, gint viewable_size) {
	gdouble new_upper;
	gdouble new_page_size;
	
	
	new_page_size = (gdouble)MAX(0, viewable_size);
	new_upper = (gdouble)MAX(new_page_size, total_size);

	if (adjustment->lower != 0.0 || adjustment->upper != new_upper || adjustment->page_size != new_page_size) {
		adjustment->lower = 0.0;
		adjustment->step_increment = 0.1 * new_page_size;
		adjustment->page_increment = 0.9 * new_page_size;
		adjustment->page_size = new_page_size;
		adjustment->upper = new_upper;
		
		if (adjustment->value > new_upper - new_page_size) {
			adjustment->value = MAX(0.0, new_upper - new_page_size);
			gtk_adjustment_changed(adjustment);
			gtk_adjustment_value_changed(adjustment);
		} else if (adjustment->value < 0.0 ) {
			adjustment->value = 0.0;
			gtk_adjustment_changed(adjustment);
			gtk_adjustment_value_changed(adjustment);		
		} else {
			gtk_adjustment_changed(adjustment);
		}
	}
}

static GdkWindow* create_window(GtkWidget* widget, GdkWindow* parent, gint x, gint y, gint width, gint height) {
	GdkWindow* window;
	GdkWindowAttr attributes;
	gint attributes_mask;

	attributes.x = x;
	attributes.y = y;
	attributes.width = width;
	attributes.height = height;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_OUTPUT;
	attributes.visual = gtk_widget_get_visual(widget);
	attributes.colormap = gtk_widget_get_colormap(widget);
	attributes.event_mask = gtk_widget_get_events(widget) | GDK_EXPOSURE_MASK;
	attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
	window = gdk_window_new(parent, &attributes, attributes_mask);
	gdk_window_set_user_data(window, widget);

	widget->style = gtk_style_attach(widget->style, window);
	gtk_style_set_background(widget->style, window, GTK_STATE_NORMAL);

	gtk_paint_flat_box(widget->style, window, GTK_STATE_NORMAL, GTK_SHADOW_NONE, NULL,
	                   widget, "maukuscrollingboxwindow", 0, 0, -1, -1);
	gdk_window_show(window);

	return window;
}

static gboolean is_visible(gint widget_start, gint widget_end, gint page_start, gint page_end) {

	return (widget_start >= page_start && widget_start <= page_end) ||
	       (widget_end >= page_end && widget_end <= page_end) ||
	       (widget_start < page_start && widget_end > page_end);
}
