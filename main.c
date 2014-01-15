
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#include <gtk/gtk.h>
#include <hildon/hildon.h>
#include <microfeed-subscriber/microfeedsubscriber.h>
#include <microfeed-common/microfeedprotocol.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-lowlevel.h>
#include "mauku.h"
#include "mauku-view.h"
#include "mauku-contacts.h"
#include "mauku-write.h"

static void image_stored(MicrofeedSubscriber* subscriber, const char* publisher, const char* url, const char* path, void* user_data);

DBusConnection* dbus_connection;
MicrofeedSubscriber* subscriber;

static MaukuView* overview_view = NULL;
static GHashTable* image_caches = NULL;

GdkPixbuf* image_cache_get_image(const gchar* publisher, const gchar* uid) {
	GdkPixbuf* image = NULL;
	GHashTable* image_cache;
	
	if ((image_cache = (GHashTable*)g_hash_table_lookup(image_caches, publisher))) {
		image = (GdkPixbuf*)g_hash_table_lookup(image_cache, uid);
	}
	
	return image;
}

static void image_stored(MicrofeedSubscriber* subscriber, const char* publisher, const char* url, const char* path, void* user_data) {
	GdkPixbuf* image = NULL;
	GHashTable* image_cache;

	if (image_caches && (image_cache = (GHashTable*)g_hash_table_lookup(image_caches, publisher))) {
		if ((image = gdk_pixbuf_new_from_file_at_size(path, 51, 51, NULL))) {
			g_hash_table_replace(image_cache, g_strdup(url), image);
		}
	}
}

static void configured_subscribe(MicrofeedSubscriber* subscriber, const char* publisher, void* user_data) {
	GHashTable* image_cache;

	if (overview_view) {
		mauku_view_add_feed(overview_view, publisher, MICROFEED_FEED_URI_OVERVIEW);
	}
	if (!image_caches) {
		image_caches = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_destroy);
	}
	image_cache = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
	g_hash_table_replace(image_caches, g_strdup(publisher), image_cache);
	microfeed_subscriber_add_data_stored_callback(subscriber, publisher, image_stored, NULL);
}

static void configured_unsubscribe(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, MicrofeedSubscriberCallbacks* callbacks, void* callbacks_user_data, void* user_data) {
	GHashTable* image_cache;

	if (overview_view) {
		mauku_view_remove_feed(overview_view, publisher, MICROFEED_FEED_URI_OVERVIEW);
	}
	if (image_caches && (image_cache = (GHashTable*)g_hash_table_lookup(image_caches, publisher))) {
		g_hash_table_destroy(image_cache);
		microfeed_subscriber_remove_data_stored_callback(subscriber, publisher, image_stored, NULL);
	}	
}

static void add_write_feed(gpointer key, gpointer value, gpointer user_data) {
	MaukuWrite* write;
	const gchar* publisher;

	write = (MaukuWrite*)user_data;
	publisher = (const char*)key;

	mauku_write_add_feed(write, publisher, MICROFEED_FEED_URI_OVERVIEW);
}

void add_current_publishers_to_write(MaukuWrite* write) {
	microfeed_subscriber_handle_configured_subscriptions(subscriber, configured_subscribe, configured_unsubscribe, NULL);
	g_hash_table_foreach(image_caches, add_write_feed, write);
}

static void on_write_button_clicked(gpointer user_data) {
	MaukuWrite* write;

	write = mauku_write_new("New message", NULL, NULL, NULL);
	add_current_publishers_to_write(write);
	mauku_write_show(write);
}

static void add_view_feed(gpointer key, gpointer value, gpointer user_data) {
	MaukuView* view;
	const gchar* publisher;

	view = (MaukuView*)user_data;
	publisher = (const char*)key;

	mauku_view_add_feed(view, publisher, MICROFEED_FEED_URI_OVERVIEW);
}

static void open_overview(void) {
	microfeed_subscriber_handle_configured_subscriptions(subscriber, configured_subscribe, configured_unsubscribe, NULL);
	if (!overview_view) {
		overview_view = mauku_view_new("Overview", TRUE);
		g_hash_table_foreach(image_caches, add_view_feed, overview_view);
	}
	mauku_view_show(overview_view);
}

static gboolean on_write_button_pressed(GtkWidget* button, GdkEventButton* event, gpointer user_data) {
	gtk_image_set_from_file(GTK_IMAGE(user_data), IMAGE_DIR "/mauku_new_message_selected.png");
	
	return FALSE;
}

static gboolean on_write_button_released(GtkWidget* button, GdkEventButton* event, gpointer user_data) {
	gtk_image_set_from_file(GTK_IMAGE(user_data), IMAGE_DIR "/mauku_new_message.png");
	
	
	return FALSE;
}

static gboolean on_overview_button_pressed(GtkWidget* button, GdkEventButton* event, gpointer user_data) {
	gtk_image_set_from_file(GTK_IMAGE(user_data), IMAGE_DIR "/mauku_overview_selected.png");
	
	return FALSE;
}

static gboolean on_overview_button_released(GtkWidget* button, GdkEventButton* event, gpointer user_data) {
	gtk_image_set_from_file(GTK_IMAGE(user_data), IMAGE_DIR "/mauku_overview.png");
	
	
	return FALSE;
}

static gboolean on_contact_button_pressed(GtkWidget* button, GdkEventButton* event, gpointer user_data) {
	gtk_image_set_from_file(GTK_IMAGE(user_data), IMAGE_DIR "/mauku_contacts_selected.png");
	
	return FALSE;
}

static gboolean on_contact_button_released(GtkWidget* button, GdkEventButton* event, gpointer user_data) {
	gtk_image_set_from_file(GTK_IMAGE(user_data), IMAGE_DIR "/mauku_contacts.png");
	
	
	return FALSE;
}

static gboolean on_overview_button_clicked(gpointer user_data) {
	open_overview();
	
	return FALSE;
}

static void add_contacts_publisher(gpointer key, gpointer value, gpointer user_data) {
	MaukuContacts* contacts;
	const gchar* publisher;

	contacts = (MaukuContacts*)user_data;
	publisher = (const char*)key;

	mauku_contacts_add_publisher(contacts, publisher);
}

static void on_contact_button_clicked(gpointer user_data) {
	GtkWindow* window;
	MaukuContacts* contacts;
	
	window = GTK_WINDOW(user_data);

	contacts = mauku_contacts_new(window);
	microfeed_subscriber_handle_configured_subscriptions(subscriber, configured_subscribe, configured_unsubscribe, NULL);
	g_hash_table_foreach(image_caches, add_contacts_publisher, contacts);

	mauku_contacts_show(contacts);
}

static void on_about_button_clicked(gpointer user_data) {
	GtkWindow* window;
	GdkPixbuf* pixbuf;

	window = GTK_WINDOW(user_data);

	pixbuf = gdk_pixbuf_new_from_file(IMAGE_DIR "/mauku_logo_191x64.png", NULL);
	gtk_show_about_dialog(window, "name", "Mauku", "logo", pixbuf, "version", VERSION, 
	                      "copyright", "Henrik Hedberg <hhedberg@innologies.fi>", 
			      "website", "http://mauku.henrikhedberg.com/", NULL);
}

static void on_child_exit(GPid pid, gint status, gpointer user_data) {
	microfeed_subscriber_handle_configured_subscriptions(subscriber, configured_subscribe, configured_unsubscribe, NULL);

	if (overview_view) {
		mauku_view_show(overview_view);
		mauku_view_update(overview_view);	
	} else {
		open_overview();
	}

	g_spawn_close_pid(pid);
}

static void execute_configuration(void) {
	gchar* args[3];
	GPid pid;

	args[0] = "microfeed-configuration";
	args[1] = SUBSCRIBER_IDENTIFIER;
	args[2] = NULL;
	if (g_spawn_async(NULL, args, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, NULL)) {
		g_child_watch_add(pid, on_child_exit, NULL);
	} else {
		hildon_banner_show_information(NULL, NULL, "Could not launch microfeed-configuration.");
	}
}

static void on_publishers_button_clicked(GtkButton* button, gpointer user_data) {
	execute_configuration();
}

static void on_special_feeds_button_clicked(GtkButton* button, gpointer user_data) {
	static MaukuContacts* contacts = NULL;
	GtkWindow* window;
	GtkWidget* touch_selector;
	GtkWidget* picker_dialog;
	
	window = GTK_WINDOW(user_data);

	hildon_banner_show_information(NULL, NULL, "Not implemented yet.");
}

static HildonAppMenu* create_menu() {
	HildonAppMenu* app_menu;
	GtkWidget* button;
	
	app_menu = HILDON_APP_MENU(hildon_app_menu_new());

	button = gtk_button_new_with_label("Publishers");
	g_signal_connect_after(button, "clicked", G_CALLBACK(on_publishers_button_clicked), NULL);
	hildon_app_menu_append(app_menu, GTK_BUTTON(button));

	button = gtk_button_new_with_label("About");
	g_signal_connect_after(button, "clicked", G_CALLBACK(on_about_button_clicked), NULL);
	hildon_app_menu_append(app_menu, GTK_BUTTON(button));

	gtk_widget_show_all(GTK_WIDGET(app_menu));
	
	return app_menu;
}
static void set_background(GtkWidget* widget, const char* filename) {
	GError* error = NULL;
	GdkPixbuf* pixbuf;
	GdkPixmap* pixmap;
	GtkStyle* style;

	pixbuf = gdk_pixbuf_new_from_file(filename, &error);
	if (!error) {
		gdk_pixbuf_render_pixmap_and_mask (pixbuf, &pixmap, NULL, 0); 
		style = gtk_style_new();
		style->bg_pixmap[0] = pixmap;
		gtk_widget_set_style(widget, style);
	}
}

static void create_main_window() {
	GtkWidget* window;
	GtkWidget* table;
	GtkWidget* box;
	GtkWidget* widget;
	GtkWidget* button;

	window = hildon_stackable_window_new();
	gtk_window_set_title(GTK_WINDOW(window), "Mauku " VERSION);
	g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	set_background(window, IMAGE_DIR "/background_main.png");
	hildon_window_set_app_menu(HILDON_WINDOW(window), create_menu());
	hildon_program_add_window(hildon_program_get_instance(), HILDON_WINDOW(window));

	table = gtk_table_new(1, 3, TRUE);
	gtk_container_set_border_width(GTK_CONTAINER(table), 48);
	gtk_container_add(GTK_CONTAINER(window), table);

	button = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(button), FALSE);
	widget = gtk_image_new_from_file(IMAGE_DIR "/mauku_new_message.png");
	g_signal_connect(button, "button-press-event", G_CALLBACK(on_write_button_pressed), widget);
	g_signal_connect(button, "button-release-event", G_CALLBACK(on_write_button_released), widget);
	g_signal_connect_swapped(button, "button-release-event", G_CALLBACK(on_write_button_clicked), window);
	gtk_table_attach(GTK_TABLE(table), button, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 8, 8);	
	box = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 1.0);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);
	widget = gtk_label_new("New message");
	gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);

	button = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(button), FALSE);
	widget = gtk_image_new_from_file(IMAGE_DIR "/mauku_overview.png");
	g_signal_connect(button, "button-press-event", G_CALLBACK(on_overview_button_pressed), widget);
	g_signal_connect(button, "button-release-event", G_CALLBACK(on_overview_button_released), widget);
	g_signal_connect_swapped(button, "button-release-event", G_CALLBACK(on_overview_button_clicked), window);
	gtk_table_attach(GTK_TABLE(table), button, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 8, 8);	
	box = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 1.0);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);
	widget = gtk_label_new("Overview");
	gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);

	button = gtk_event_box_new();
	gtk_event_box_set_visible_window(GTK_EVENT_BOX(button), FALSE);
	widget = gtk_image_new_from_file(IMAGE_DIR "/mauku_contacts.png");
	g_signal_connect(button, "button-press-event", G_CALLBACK(on_contact_button_pressed), widget);
	g_signal_connect(button, "button-release-event", G_CALLBACK(on_contact_button_released), widget);
	g_signal_connect_swapped(button, "button-release-event", G_CALLBACK(on_contact_button_clicked), window);
	gtk_table_attach(GTK_TABLE(table), button, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 8, 8);	
	box = gtk_vbox_new(FALSE, 8);
	gtk_container_add(GTK_CONTAINER(button), box);
	gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 1.0);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);
	widget = gtk_label_new("Contacts");
	gtk_misc_set_alignment(GTK_MISC(widget), 0.5, 0.0);
	gtk_box_pack_start(GTK_BOX(box), widget, TRUE, TRUE, 0);

	gtk_widget_show_all(window);
}

static void check_subscriptions(void) {
	MicrofeedConfiguration* configuration;
	const char** subscriptions;
	GtkWidget* dialog;
	
	configuration = microfeed_configuration_new();
	if (!(subscriptions = microfeed_configuration_get_subscriptions(configuration, SUBSCRIBER_IDENTIFIER)) || !*subscriptions) {
		dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_INFO, GTK_BUTTONS_NONE,
		                                "It seems that you do not have any subscriptions yet. You should configure at least one publisher before using Mauku.");
		gtk_window_set_title(GTK_WINDOW(dialog), "Welcome to Mauku");
		gtk_dialog_add_buttons(GTK_DIALOG(dialog), "Configure...", 1, NULL);
		if (gtk_dialog_run(GTK_DIALOG(dialog)) == 1) {
			execute_configuration();
		}
		gtk_widget_destroy(dialog);
	}

	microfeed_configuration_free(configuration);
}

static void unregister_function(DBusConnection* connection, void* user_data) {
}

static DBusHandlerResult message_function(DBusConnection* connection, DBusMessage* message, void* user_data) {
	DBusHandlerResult result = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	DBusMessage* reply = NULL;
	DBusError error;
	char* publisher;
	char* uri;
	char* uid;
	
	if (dbus_message_is_method_call(message, "com.innologies.Mauku", "OpenItem")) {
		dbus_error_init(&error);
		if (dbus_message_get_args(message, &error, DBUS_TYPE_STRING, &publisher, DBUS_TYPE_STRING, &uri, DBUS_TYPE_STRING, &uid, DBUS_TYPE_INVALID)) {
			open_overview();
			if (strcmp(uri, MICROFEED_FEED_URI_OVERVIEW)) {
				reply = dbus_message_new_error(message, "com.innologies.Mauku.Error.InvalidArguments", "Only overview feed is supported.");		
			} else if (!g_hash_table_lookup(image_caches, publisher)) {
				reply = dbus_message_new_error(message, "com.innologies.Mauku.Error.InvalidArguments", "Publisher is not subcribed.");
			} else if (!mauku_view_scroll_to_item(overview_view, publisher, uri, uid)) {
				reply = dbus_message_new_error(message, "com.innologies.Mauku.Error.InvalidArguments", "No such item.");
			} else {
				reply = dbus_message_new_method_return(message);
			}
		} else {
			reply = dbus_message_new_error(message, "com.innologies.Mauku.Error.InvalidArguments", "Wrong arguments in method call.");
		}		
	}
	
	if (reply) {
		dbus_connection_send(connection, reply, NULL);
		dbus_message_unref(reply);
		result = DBUS_HANDLER_RESULT_HANDLED;
	}
	
	return result;
}

static DBusObjectPathVTable vtable = {
	unregister_function,
	message_function
};

int main(int argc, char** argv) {
	DBusError error;
	
	hildon_gtk_init(&argc, &argv);
	g_set_application_name("Mauku");

	dbus_error_init(&error);
	dbus_connection = dbus_bus_get(DBUS_BUS_SESSION, &error);
	dbus_connection_setup_with_g_main(dbus_connection, NULL);
	
	if (dbus_bus_request_name(dbus_connection, "com.innologies.mauku", 0, &error) == -1) {
		fprintf(stderr, "Could not get D-BUS name %s.\n," "com.innologies.mauku");	
	} else {
		dbus_connection_register_object_path(dbus_connection, "/com/innologies/mauku", &vtable, NULL);
	}

	subscriber = microfeed_subscriber_new(SUBSCRIBER_IDENTIFIER, dbus_connection);

	create_main_window();
	check_subscriptions();

	gtk_main();
	
	return 0;
}
