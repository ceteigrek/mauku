
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#include "mauku.h"
#include "mauku-view.h"
#include "mauku-item.h"
#include "mauku-write.h"
#include "mauku-scrolling-box.h"
#include <microfeed-common/microfeedmisc.h>
#include <microfeed-common/microfeedprotocol.h>
#include <hildon/hildon.h>
#include <string.h>

typedef struct {
	MaukuView* view;
	gchar* publisher;
	gchar* uri;
} Subscription;

struct _MaukuView {
	GtkWidget* window;
	HildonAppMenu* menu;
	GtkWidget* pannable_area;
	GtkWidget* container;
	GList* subscriptions;
	gint updating;
	gint republishing;
	gchar* jump_to_publisher;
	gchar* jump_to_uri;
	gchar* jump_to_uid;
};

static gboolean on_delete_event(MaukuView* view);
static void feed_subscribed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data);
static HildonAppMenu* create_menu();
static void error_occured(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data);
static void feed_update_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void feed_update_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void feed_republishing_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void feed_republishing_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void item_added(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, MicrofeedItem* item, void* user_data);
static void item_status_changed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, MicrofeedItemStatus status, void* user_data);
static gboolean get_item(MaukuView* view, const gchar* publisher, const gchar* uri, const gchar* uid, time_t timestamp, MaukuItem** item_return);
static void set_progress_indicator(MaukuView* view);
static void on_pannable_area_realize(GtkWidget* widget, gpointer data);
static void on_is_topmost_notify(gpointer user_data);
static void mark_all_read(MaukuView* view);


static MicrofeedSubscriberCallbacks callbacks = {
	error_occured,
	feed_update_started,
	feed_update_ended,
	feed_republishing_started,
	feed_republishing_ended,
	item_added,
	item_added,
	item_added, /* item_republished */
	NULL, /* item_removed */
	item_status_changed,
};

MaukuView* mauku_view_new(const gchar* title, gboolean permanent) {
	MaukuView* view;
	GtkWidget* item;
	GtkWidget* menu;
	GtkWidget* menu_item;
	GtkWidget* event_box;
	int i;
	gchar* s;
	time_t t;
	
	view = microfeed_memory_allocate(MaukuView);
	view->window = hildon_stackable_window_new();
	if (permanent) {
		g_signal_connect(view->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	} else {
		g_signal_connect_swapped(view->window, "delete-event", G_CALLBACK(on_delete_event), view);
	}
	g_signal_connect_swapped(view->window, "notify::is-topmost", G_CALLBACK(on_is_topmost_notify), view);	

	view->menu = create_menu(view);
	hildon_window_set_app_menu(HILDON_WINDOW(view->window), view->menu);		

	gtk_window_set_title(GTK_WINDOW(view->window), title);
	view->pannable_area = hildon_pannable_area_new();
	g_object_set(view->pannable_area, "hscrollbar-policy", GTK_POLICY_NEVER, "vovershoot-max", 400, NULL);
	gtk_container_add(GTK_CONTAINER(view->window), view->pannable_area);

	view->container = mauku_scrolling_box_new_vertical(hildon_pannable_area_get_hadjustment(HILDON_PANNABLE_AREA(view->pannable_area)),
	                                                   hildon_pannable_area_get_vadjustment(HILDON_PANNABLE_AREA(view->pannable_area)));
	gtk_container_add(GTK_CONTAINER(view->pannable_area), view->container);
	
	return view;
}

static gboolean on_delete_event(MaukuView* view) {
	GList* list;
	Subscription* subscription;
	
	for (list = view->subscriptions; list; list = list->next) {
		subscription = (Subscription*)list->data;
		microfeed_subscriber_unsubscribe_feed(subscriber, subscription->publisher, subscription->uri, &callbacks, subscription, NULL, NULL);
		g_free(subscription);
	}
	/* TODO: Free also other stuff. */

	return FALSE;
}

void mauku_view_show(MaukuView* view) {
	gtk_widget_show_all(view->window);
}

void mauku_view_add_feed(MaukuView* view, const gchar* publisher, const gchar* uri) {
	Subscription* subscription;

	printf("mauku_view_add_feed: %s %s\n", publisher, uri);
	
	subscription = g_new0(Subscription, 1);
	subscription->view = view;
	subscription->publisher = g_strdup(publisher);
	subscription->uri = g_strdup(uri);
	view->subscriptions = g_list_prepend(view->subscriptions, subscription);

	microfeed_subscriber_subscribe_feed(subscriber, publisher, uri, &callbacks, subscription, feed_subscribed, subscription);
}

void mauku_view_remove_feed(MaukuView* view, const gchar* publisher, const gchar* uri) {
	GList* list;
	Subscription* subscription;
	GList* child;
	MaukuItem* item;

	printf("mauku_view_remove_feed: %s %s\n", publisher, uri);

	for (list = view->subscriptions; list; list = list->next) {
		subscription = (Subscription*)list->data;
		if (!strcmp(subscription->uri, uri) && !strcmp(subscription->publisher, publisher)) {
			view->subscriptions = g_list_delete_link(view->subscriptions, list);
			g_free(subscription->publisher);
			g_free(subscription->uri);
			g_free(subscription);
			break;
		}
	}
	list = gtk_container_get_children(GTK_CONTAINER(view->container));
	for (child = list; child; child = child->next) {
		item = MAUKU_ITEM(child->data);
		if (!strcmp(mauku_item_get_uri(item), uri) && !strcmp(mauku_item_get_publisher(item), publisher)) {
			gtk_widget_destroy(GTK_WIDGET(item));
		}
	}
	g_list_free(list);
}

static void scroll_to(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	MaukuView* view;
	MaukuItem* item;
	
	view = (MaukuView*)user_data;
	if (!error_name && get_item(view, publisher, uri, uid, 0, &item)) {
		hildon_pannable_area_scroll_to_child(HILDON_PANNABLE_AREA(view->pannable_area), GTK_WIDGET(item));
	}
}

gboolean mauku_view_scroll_to_item(MaukuView* view, const gchar* publisher, const gchar* uri, const gchar* uid) {
	gboolean retvalue = TRUE; /* Always TRUE, since we do not really know yet... */
	MaukuItem* item;
	
	if (get_item(view, publisher, uri, uid, 0, &item)) {
		hildon_pannable_area_scroll_to_child(HILDON_PANNABLE_AREA(view->pannable_area), GTK_WIDGET(item));
		retvalue = TRUE;
	} else {
		microfeed_subscriber_republish_items(subscriber, publisher, uri, uid, uid, 1, scroll_to, view);
	}

	return retvalue;
}

void mauku_view_update(MaukuView* view) {
	GList* list;
	Subscription* subscription;
	
	for (list = view->subscriptions; list; list = list->next) {
		subscription = (Subscription*)list->data;
		microfeed_subscriber_update_feed(subscriber, subscription->publisher, subscription->uri, NULL, NULL);
	}	
}

static void on_update_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuView* view;
	
	view = (MaukuView*)user_data;
	mauku_view_update(view);
}

static void on_jump_to_top_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuView* view;
	
	view = (MaukuView*)user_data;
	mark_all_read(view);
	hildon_pannable_area_scroll_to(HILDON_PANNABLE_AREA(view->pannable_area), 0, 0);
}

static HildonAppMenu* create_menu(MaukuView* view) {
	HildonAppMenu* app_menu;
	GtkWidget* button;
	
	app_menu = HILDON_APP_MENU(hildon_app_menu_new());

/*	button = gtk_radio_button_new_with_label(NULL, "Show only new messages");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
	hildon_app_menu_add_filter(app_menu, GTK_BUTTON(button));
	
	button = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(button), "Show all messages");
	gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
	hildon_app_menu_add_filter(app_menu, GTK_BUTTON(button));
*/

	button = gtk_button_new_with_label("Update");
	g_signal_connect_after(button, "clicked", G_CALLBACK(on_update_button_clicked), view);
	hildon_app_menu_append(app_menu, GTK_BUTTON(button));

	button = gtk_button_new_with_label("Jump to top");
	g_signal_connect_after(button, "clicked", G_CALLBACK(on_jump_to_top_button_clicked), view);
	hildon_app_menu_append(app_menu, GTK_BUTTON(button));

	gtk_widget_show_all(GTK_WIDGET(app_menu));
	
	return app_menu;
}

static void feed_subscribed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	printf("MaukuView::feed_subscribed: %s %s %s %s\n", publisher, uri, error_name, error_message);

	microfeed_subscriber_republish_items(subscriber, publisher, uri, NULL, NULL, 50, NULL, NULL);
}

static gint press_x, press_y;
static gboolean on_button_release_event(GtkWidget* widget, GdkEventButton* event, gpointer user_data);

static gboolean on_button_press_event(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
	MaukuView* view;
	GtkWidget* item;
	GList* children;
	
	view = (MaukuView*)user_data;
	
	gtk_widget_translate_coordinates(widget, view->window, event->x, event->y, &press_x, &press_y);

/*		item = mauku_item_new("publisher", "uri", "uid", NULL, NULL,
	                        	"Testi\ndsafjh sdflkaj afdsfjkk iii abcdefghijklmn",
	                        	MICROFEED_ITEM_PROPERTY_NAME_USER_NICK,
	                        	time(NULL) - 55,
	                        	0, FALSE);
		mauku_scrolling_box_add_before(MAUKU_SCROLLING_BOX(view->container), item, widget);
		gtk_widget_show_all(item);
		g_signal_connect(item, "button-press-event", G_CALLBACK(on_button_press_event), view);
		g_signal_connect(item, "button-release-event", G_CALLBACK(on_button_release_event), view);
*/
/*	children = gtk_container_get_children(GTK_CONTAINER(view->container));
	gtk_container_remove(GTK_CONTAINER(view->container), children->data);
*/
}

static void on_reply_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;
	gchar* s;
	MaukuWrite* write;
	
	item = MAUKU_ITEM(user_data);
	
	s = g_strconcat("@", mauku_item_get_sender(item), " ", NULL);
	write = mauku_write_new("Reply", MAUKU_ITEM(mauku_item_new_from_item(item)), s, NULL);
	g_free(s);
	mauku_write_add_feed(write, mauku_item_get_publisher(item), mauku_item_get_uri(item));
	mauku_write_show(write);
	
	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void on_forward_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;
	gchar* s;
	MaukuWrite* write;
	
	item = MAUKU_ITEM(user_data);
	
	s = g_strconcat("RT @", mauku_item_get_sender(item), ": ", mauku_item_get_text(item), NULL);
	write = mauku_write_new("Forward", NULL, NULL, s);
	g_free(s);
	add_current_publishers_to_write(write);
	mauku_write_show(write);
	
	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void on_contact_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;
	MaukuView* view;

	item = MAUKU_ITEM(user_data);
	
	view = mauku_view_new(mauku_item_get_sender(item), FALSE);
	mauku_view_add_feed(view, mauku_item_get_publisher(item), mauku_item_get_sender_uri(item));
	mauku_view_show(view);

	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void on_referred_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;
	MaukuView* view;

	item = MAUKU_ITEM(user_data);
	
	view = mauku_view_new("Comments", FALSE);
	mauku_view_add_feed(view, mauku_item_get_publisher(item), mauku_item_get_referred_uri(item));
	mauku_view_show(view);

	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void on_comments_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;
	MaukuView* view;

	item = MAUKU_ITEM(user_data);
	
	view = mauku_view_new("Comments", FALSE);
	mauku_view_add_feed(view, mauku_item_get_publisher(item), mauku_item_get_comments_uri(item));
	mauku_view_show(view);

	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void append_to_table(GtkTable* table, GtkWidget* widget, gint cols, gint* col_pointer, gint* row_pointer) {
	gtk_table_attach_defaults(table, widget, *col_pointer, *col_pointer + 1, *row_pointer, *row_pointer + 1);
	(*col_pointer)++;
	if (*col_pointer >= cols) {
		(*row_pointer)++;
		*col_pointer = 0;
	}
}

static void on_open_link_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
	HildonTouchSelector* touch_selector;
	gchar* url;
	DBusMessage* message;

	touch_selector = HILDON_TOUCH_SELECTOR(user_data);
	if (response_id == GTK_RESPONSE_OK &&
	    (url = hildon_touch_selector_get_current_text(touch_selector))) {
printf("%s\n", url);
		message = dbus_message_new_method_call("com.nokia.osso_browser", "/com/nokia/osso_browser", "com.nokia.osso_browser", "open_new_window");
		dbus_message_append_args(message,DBUS_TYPE_STRING, &url, DBUS_TYPE_INVALID);
		dbus_connection_send(dbus_connection, message, NULL);
		dbus_message_unref(message);

		g_free(url);
	}
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void on_open_link_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;
	GtkWidget* dialog;
	GtkWidget* touch_selector;
	const gchar* start;
	const gchar* end;
	const gchar* temp;
	GtkWidget* link_button;
	gchar* s;
	const gchar* cs;
	gint col = 0, row = 0;

	item = MAUKU_ITEM(user_data);
		
	dialog = hildon_picker_dialog_new(NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), "Open link");
	
	touch_selector = hildon_touch_selector_new_text();
	hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog), HILDON_TOUCH_SELECTOR(touch_selector));

	if ((cs = mauku_item_get_link(item))) {
		hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(touch_selector), cs);
	}
	
	end = mauku_item_get_text(item);
	while (((start = strstr(end, "http://")) && start[7] && !g_ascii_isspace(start[7])) ||
	       ((start = strstr(end, "https://")) && start[8] && !g_ascii_isspace(start[8])) ||
	       ((start = strstr(end, "www.")) && start[4] && !g_ascii_isspace(start[4]))) {
		if (*start == 'h' && (temp = strstr(end, "www.")) && temp[4] && !g_ascii_isspace(temp[4]) && temp < start) {
			start = temp;
		} else if (*start == 'w' && (temp = strstr(end, "http://")) && temp[7] && !g_ascii_isspace(temp[7]) && temp < start) {
			start = temp;
		}
		for (end = start; *end; end++) {
			if (g_ascii_isspace(*end) || *end == ')') {
				break;
			}
		}
		while (end > start && *(end - 1) == '.') {
			end--;
		}
		s = g_strndup(start, end - start);
		hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(touch_selector), s);
		g_free(s);
	}

	g_signal_connect(dialog, "response", G_CALLBACK(on_open_link_dialog_response), touch_selector);

	gtk_widget_show_all(dialog);
	gtk_widget_hide(GTK_DIALOG(dialog)->action_area);

	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void on_mark_button_clicked(GtkButton* button, gpointer user_data) {
	MaukuItem* item;

	item = MAUKU_ITEM(user_data);

	if (mauku_item_get_marked(item)) {
		microfeed_subscriber_unmark_item(subscriber, mauku_item_get_publisher(item), mauku_item_get_uri(item), mauku_item_get_uid(item), error_occured, NULL);
	} else {
		microfeed_subscriber_mark_item(subscriber, mauku_item_get_publisher(item), mauku_item_get_uri(item), mauku_item_get_uid(item), error_occured, NULL);
	}

	gtk_widget_destroy(gtk_widget_get_toplevel(GTK_WIDGET(button)));
}

static void show_item_dialog(MaukuView* view, MaukuItem* item) {
	GtkWidget* dialog;
	gchar* title;
	GtkWidget* table;
	GtkWidget* button;
	gchar* s;
	gint col = 0, row = 0;
	
	dialog = gtk_dialog_new();
	if (strlen(mauku_item_get_text(item)) > 60) {
		title = g_strdup_printf("%.60s...", mauku_item_get_text(item));
		gtk_window_set_title(GTK_WINDOW(dialog), title);
		g_free(title);
	} else {
		gtk_window_set_title(GTK_WINDOW(dialog), mauku_item_get_text(item));
	}
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	
	table = gtk_table_new(3, 2, TRUE);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), table, FALSE, FALSE, 0);	
	
	button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Reply", NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(on_reply_button_clicked), item);
	append_to_table(GTK_TABLE(table), button, 2, &col, &row);

	button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Forward", NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(on_forward_button_clicked), item);
	append_to_table(GTK_TABLE(table), button, 2, &col, &row);

	if (mauku_item_get_sender_uri(item)) {
		button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Open sender", NULL);
		g_signal_connect(button, "clicked", G_CALLBACK(on_contact_button_clicked), item);	
		append_to_table(GTK_TABLE(table), button, 2, &col, &row);
	}
	
	if (mauku_item_get_referred_uri(item)) {
		button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Open referred", NULL);
		g_signal_connect(button, "clicked", G_CALLBACK(on_referred_button_clicked), item);	
		append_to_table(GTK_TABLE(table), button, 2, &col, &row);
	}
	
	if (mauku_item_get_comments_uri(item)) {
		button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Open comments", NULL);
		g_signal_connect(button, "clicked", G_CALLBACK(on_comments_button_clicked), item);	
		append_to_table(GTK_TABLE(table), button, 2, &col, &row);
	}		

	button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, "Open link", NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(on_open_link_button_clicked), item);
	append_to_table(GTK_TABLE(table), button, 2, &col, &row);

	button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL, (mauku_item_get_marked(item) ? "Unmark" : "Mark"), NULL);
	g_signal_connect(button, "clicked", G_CALLBACK(on_mark_button_clicked), item);
	append_to_table(GTK_TABLE(table), button, 2, &col, &row);

	gtk_widget_show_all(dialog);
	gtk_widget_hide(GTK_DIALOG(dialog)->action_area);
}

static gboolean on_button_release_event(GtkWidget* widget, GdkEventButton* event, gpointer user_data) {
	gboolean handled = FALSE;
	MaukuView* view;
	gint x, y;
	
	view = (MaukuView*)user_data;
	
	gtk_widget_translate_coordinates(widget, view->window, event->x, event->y, &x, &y);		
	if (!gtk_drag_check_threshold(widget, press_x, press_y, x, y)) {
		show_item_dialog(view, MAUKU_ITEM(widget));
		handled = TRUE;
	}
		
	return handled;
}

static void error_occured(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	if (error_name) {
		hildon_banner_show_information(NULL, NULL, (error_message ? error_message : error_name));
	}
}

static void feed_update_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuView* view;

	subscription = (Subscription*)user_data;
	view = subscription->view;
	view->updating++;
	set_progress_indicator(view);
}	

static void feed_update_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuView* view;

	subscription = (Subscription*)user_data;
	view = subscription->view;
	view->updating--;
	set_progress_indicator(view);
}

static void feed_republishing_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuView* view;

	subscription = (Subscription*)user_data;
	view = subscription->view;
	view->republishing++;
	set_progress_indicator(view);
}	

static void feed_republishing_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuView* view;

	subscription = (Subscription*)user_data;
	view = subscription->view;
	view->republishing--;
	set_progress_indicator(view);
}	

static void image_stored(MicrofeedSubscriber* subscriber, const char* publisher, const char* url, const char* not_used, const char* error_name, const char* error_message, void* user_data) {
	MaukuItem* item;
	GdkPixbuf* avatar;
	
	item = MAUKU_ITEM(user_data);

	if (!error_name && (avatar = image_cache_get_image(publisher, url))) {
		mauku_item_set_avatar(item, avatar);
	}
}

static void item_added(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, MicrofeedItem* item, void* user_data) {
	Subscription* subscription;
	MaukuView* view;
	MaukuItem* existing_item;
	const char* s;
	guint comments = 0;
	const char* uid;
	const void* data;
	size_t length;
	GdkPixbuf* avatar = NULL;
	GdkPixbuf* icon = NULL;
	GtkWidget* widget;

	printf("MaukuView::item_added: %s %s %s\n", publisher, uri, microfeed_item_get_uid(item));
	
	subscription = (Subscription*)user_data;
	view = subscription->view;

	if (!strcmp(microfeed_item_get_uid(item), MICROFEED_ITEM_UID_FEED_METADATA)) {
	
	} else if (get_item(view, publisher, uri, microfeed_item_get_uid(item), microfeed_item_get_timestamp(item), &existing_item)) {
	
	} else {
		if ((s = microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_COMMENTS_COUNT))) {
			comments = atoi(s);
		}


		if ((uid = microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_IMAGE))) {
			avatar = image_cache_get_image(publisher, uid);
		}
		
		widget = mauku_item_new(publisher, uri, microfeed_item_get_uid(item), avatar, icon,
	                        	microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_CONTENT_TEXT),
	                        	microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_NICK),
					microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_FEED),
	                        	microfeed_item_get_timestamp(item),
	                        	comments,
					microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_COMMENTS_FEED),
					microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_REFERRED_ITEM),
					microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_REFERRED_FEED),
					microfeed_item_get_status(item) & MICROFEED_ITEM_STATUS_MARKED,
					microfeed_item_get_status(item) & MICROFEED_ITEM_STATUS_UNREAD,
					microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_URL));
		if (uid && !avatar) {
			microfeed_subscriber_store_data(subscriber, publisher, uid, image_stored, widget);
		}
		if (existing_item) {
			mauku_scrolling_box_add_before(MAUKU_SCROLLING_BOX(view->container), widget, GTK_WIDGET(existing_item));
		} else {
			mauku_scrolling_box_add_after(MAUKU_SCROLLING_BOX(view->container), widget, NULL);
		}
		gtk_widget_show_all(widget);
		g_signal_connect(widget, "button-press-event", G_CALLBACK(on_button_press_event), view);
		g_signal_connect(widget, "button-release-event", G_CALLBACK(on_button_release_event), view);
	}
}

static void item_status_changed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, MicrofeedItemStatus status, void* user_data) {
	Subscription* subscription;
	MaukuView* view;
	MaukuItem* item;
	
	printf("MaukuView::item_status_changed: %s %s %s\n", publisher, uri, uid);
	
	subscription = (Subscription*)user_data;
	view = subscription->view;

	if (get_item(view, publisher, uri, uid, 0, &item)) {
		mauku_item_set_marked(item, status & MICROFEED_ITEM_STATUS_MARKED);
		mauku_item_set_unread(item, status & MICROFEED_ITEM_STATUS_UNREAD);
	}
}

static gboolean get_item(MaukuView* view, const gchar* publisher, const gchar* uri, const gchar* uid, time_t timestamp, MaukuItem** item_return) {
	gboolean found = FALSE;
	MaukuItem* item;
	GList* list;
	GList* child;
	
	*item_return = NULL;
	list = gtk_container_get_children(GTK_CONTAINER(view->container));
	for (child = list; child; child = child->next) {
		item = MAUKU_ITEM(child->data);
		if (timestamp && mauku_item_get_timestamp(item) < timestamp) {
			*item_return = item;
			break;		
		} else if ((!timestamp || mauku_item_get_timestamp(item) == timestamp) &&
		           !strcmp(mauku_item_get_uid(item), uid) &&
		           !strcmp(mauku_item_get_uri(item), uri) &&
		           !strcmp(mauku_item_get_publisher(item), publisher)) {
			*item_return = item;
			found = TRUE;
			break;
		}
	}
	g_list_free(list);
	
	return found;
}

static void set_progress_indicator(MaukuView* view) {
	hildon_gtk_window_set_progress_indicator(GTK_WINDOW(view->window), (view->updating || view->republishing ? TRUE : FALSE));
}

static void mark_all_read(MaukuView* view) {
	GList* list;
	GList* child;
	MaukuItem* item;

	list = gtk_container_get_children(GTK_CONTAINER(view->container));
	for (child = list; child; child = child->next) {
		item = MAUKU_ITEM(child->data);
		if (mauku_item_get_unread(item)) {
			microfeed_subscriber_read_item(subscriber, mauku_item_get_publisher(item), mauku_item_get_uri(item), mauku_item_get_uid(item), NULL, NULL);
		}
	}
	g_list_free(list);
}


static void on_is_topmost_notify(gpointer user_data)  {
	MaukuView* view;
	
	view = (MaukuView*)user_data;
	
	if (!hildon_window_get_is_topmost(HILDON_WINDOW(view->window))) {
		mark_all_read(view);
	}
}
