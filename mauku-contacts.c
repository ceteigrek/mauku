
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#include "mauku.h"
#include "mauku-contacts.h"
#include "mauku-view.h"
#include <microfeed-subscriber/microfeedsubscriber.h>
#include <microfeed-common/microfeedmisc.h>
#include <microfeed-common/microfeedprotocol.h>
#include <hildon/hildon.h>
#include <string.h>

typedef struct {
	MaukuContacts* contacts;
	gchar* publisher;
} Subscription;

struct _MaukuContacts {
	GtkWidget* dialog;
	GtkWidget* label;
	GtkWidget* touch_selector;
	GtkListStore* list_store;
	GList* subscriptions;
	gint updating;
	gint republishing;
};

typedef struct {
	GtkListStore* list_store;
	GtkTreeIter iter;
} ListStoreWithIter;	

static void feed_subscribed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data);
static void error_occured(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data);
static void feed_update_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void feed_update_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void feed_republishing_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void feed_republishing_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data);
static void item_added(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, MicrofeedItem* item, void* user_data);
static void item_removed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, void* user_data);
static void set_progress_indicator(MaukuContacts* contacts);

static MicrofeedSubscriberCallbacks callbacks = {
	error_occured,
	feed_update_started,
	feed_update_ended,
	feed_republishing_started,
	feed_republishing_ended,
	item_added,
	NULL, /* item_changed */
	item_added, /* item_republished */
	item_removed,
	NULL, /* item_status_changed */
};

static void on_dialog_response(GtkDialog* dialog, gint response_id, gpointer user_data) {
	MaukuContacts* contacts;
	GtkTreeIter iter;
	MaukuView* view;
	const gchar* nick;
	const gchar* publisher;
	const gchar* uri;
	GList* list;
	Subscription* subscription;
	
	contacts = (MaukuContacts*)user_data;
	
	if (response_id == GTK_RESPONSE_OK &&
	    hildon_touch_selector_get_selected(HILDON_TOUCH_SELECTOR(contacts->touch_selector), 0, &iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(contacts->list_store), &iter,  3, &nick, 0, &publisher, 1, &uri, -1);
		view = mauku_view_new(nick, FALSE);
		mauku_view_add_feed(view, publisher, uri);
		mauku_view_show(view);
	}
	
	for (list = contacts->subscriptions; list; list = list->next) {
		subscription = (Subscription*)list->data;
		microfeed_subscriber_unsubscribe_feed(subscriber, subscription->publisher, MICROFEED_FEED_URI_CONTACTS, &callbacks, subscription, NULL, NULL);
		g_free(subscription);
	}
	/* TODO: Free other stuff also... */
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

static void touch_selector_changed(HildonTouchSelector* selector, gint column, gpointer user_data) {
	MaukuContacts* contacts;
	
	contacts = (MaukuContacts*)user_data;
	gtk_dialog_response(GTK_DIALOG(contacts->dialog), GTK_RESPONSE_OK);
}

MaukuContacts* mauku_contacts_new(GtkWindow* parent_window) {
	MaukuContacts* contacts;
	
	contacts = microfeed_memory_allocate(MaukuContacts);
	contacts->dialog = gtk_dialog_new();
	gtk_window_set_transient_for(GTK_WINDOW(contacts->dialog), parent_window);
	gtk_window_set_title(GTK_WINDOW(contacts->dialog), "Select the contact");
	gtk_widget_hide(GTK_DIALOG(contacts->dialog)->action_area);
	gtk_dialog_set_has_separator(GTK_DIALOG(contacts->dialog), FALSE);
	g_signal_connect_after(contacts->dialog, "realize", G_CALLBACK(hildon_gtk_window_set_progress_indicator), GINT_TO_POINTER(TRUE));
	gtk_widget_set_size_request(contacts->dialog, 800, 358);

	contacts->label = gtk_label_new("Wait while loading contacts...");
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(contacts->dialog)->vbox), contacts->label, TRUE, TRUE, 0);	
	
	contacts->list_store = gtk_list_store_new(6, G_TYPE_STRING, G_TYPE_STRING, GDK_TYPE_PIXBUF, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

	g_signal_connect(contacts->dialog, "response", G_CALLBACK(on_dialog_response), contacts);

	return contacts;
}

static void connect_changed(HildonTouchSelector* widget, gint column, gpointer user_data) {
printf("\n\n\n\n\n\nHEEEE\n");
	g_signal_connect(widget, "changed", G_CALLBACK(touch_selector_changed), user_data);
}

static void show_selector(MaukuContacts* contacts) {
	HildonTouchSelectorColumn* column;
	GtkCellRenderer* renderer;

	contacts->touch_selector = hildon_touch_selector_new();
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(contacts->dialog)->vbox), contacts->touch_selector, TRUE, TRUE, 0);

	column = hildon_touch_selector_append_column(HILDON_TOUCH_SELECTOR(contacts->touch_selector), GTK_TREE_MODEL(contacts->list_store), NULL, NULL);

	renderer = gtk_cell_renderer_pixbuf_new();
	g_object_set(renderer, "xpad", 8, NULL);
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, FALSE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(column), renderer, "pixbuf", 2);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(column), renderer, "text", 3);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(column), renderer, "text", 4);

	renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(column), renderer, TRUE);
	gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(column), renderer, "text", 5);

	g_signal_connect_after(contacts->touch_selector, "changed", G_CALLBACK(connect_changed), contacts);
	gtk_widget_show_all(contacts->touch_selector);
}

void mauku_contacts_show(MaukuContacts* contacts) {
	gtk_widget_show_all(contacts->dialog);
}

void mauku_contacts_add_publisher(MaukuContacts* contacts, const gchar* publisher) { 
	Subscription* subscription;

	printf("mauku_contacts_add_publisher: %s\n", publisher);
	
	subscription = g_new0(Subscription, 1);
	subscription->contacts = contacts;
	subscription->publisher = g_strdup(publisher);
	contacts->subscriptions = g_list_prepend(contacts->subscriptions, subscription);
	
	microfeed_subscriber_subscribe_feed(subscriber, publisher, MICROFEED_FEED_URI_CONTACTS, &callbacks, subscription, feed_subscribed, subscription);
}

static void feed_subscribed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	printf("MaukuContacts::feed_subscribed: %s %s %s %s\n", publisher, uri, error_name, error_message);

	microfeed_subscriber_republish_items(subscriber, publisher, uri, NULL, NULL, 1000, NULL, NULL);
}

static void error_occured(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	printf("MaukuContacts::error_occured: %s %s %s %s\n", publisher, uri, error_name, error_message);
}

static void feed_update_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuContacts* contacts;
	
	subscription = (Subscription*)user_data;
	contacts = subscription->contacts;
	contacts->updating++;
	set_progress_indicator(contacts);
}	

static void feed_update_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuContacts* contacts;
	
	subscription = (Subscription*)user_data;
	contacts = subscription->contacts;
	contacts->updating--;
	set_progress_indicator(contacts);
}

static void feed_republishing_started(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuContacts* contacts;
	
	subscription = (Subscription*)user_data;
	contacts = subscription->contacts;
	contacts->republishing++;
	set_progress_indicator(contacts);
}	

static void feed_republishing_ended(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, void* user_data) {
	Subscription* subscription;
	MaukuContacts* contacts;
	
	subscription = (Subscription*)user_data;
	contacts = subscription->contacts;
	contacts->republishing--;
	set_progress_indicator(contacts);
}	

static void image_stored(MicrofeedSubscriber* subscriber, const char* publisher, const char* url, const char* not_used, const char* error_name, const char* error_message, void* user_data) {
	ListStoreWithIter* lswi;
	GdkPixbuf* avatar;
	
	lswi = (ListStoreWithIter*)user_data;
	
	if ((avatar = image_cache_get_image(publisher, url))) {
		gtk_list_store_set(lswi->list_store, &lswi->iter, 2, avatar, -1);
	}
	microfeed_memory_free(lswi);
}


static void item_added(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, MicrofeedItem* item, void* user_data) {
	Subscription* subscription;
	MaukuContacts* contacts;
	const char* nick;
	GtkTreeIter eiter;
	char* enick;
	gboolean found;
	GtkTreeIter iter;
	const gchar* uid;
	const void* data;
	size_t length;
	GdkPixbuf* avatar;
	ListStoreWithIter* lswi;
	gchar* s;

	printf("MaukuContacts::item_added: %s %s %s\n", publisher, uri, microfeed_item_get_uid(item));
	
	subscription = (Subscription*)user_data;
	contacts = subscription->contacts;

	if (!strcmp(microfeed_item_get_uid(item), MICROFEED_ITEM_UID_FEED_METADATA)) {
/*	
	} else if (get_item(contacts, publisher, uri, microfeed_item_get_uid(item), microfeed_item_get_timestamp(item), &existing_item)) {
*/
	} else {
		nick = microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_NICK);
		for (found = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(contacts->list_store), &eiter);
		     found;
		     found = gtk_tree_model_iter_next(GTK_TREE_MODEL(contacts->list_store), &eiter)) {
			  gtk_tree_model_get(GTK_TREE_MODEL(contacts->list_store), &eiter, 3, &enick, -1);
			if (strcasecmp(nick, enick) < 0) {
			  	break;
			}
			g_free(enick);
		}
		if (found) {
			gtk_list_store_insert_before(contacts->list_store, &iter, &eiter);
		} else {
			gtk_list_store_append(contacts->list_store, &iter);
		}
		if ((uid = microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_IMAGE))) {
			if ((avatar = image_cache_get_image(publisher, uid))) {
				gtk_list_store_set(contacts->list_store, &iter, 2, avatar, -1);
			} else {
				lswi = microfeed_memory_allocate(ListStoreWithIter);
				lswi->list_store = contacts->list_store;
				lswi->iter = iter;
				microfeed_subscriber_store_data(subscriber, publisher, uid, image_stored, lswi);
			}
		}
		if ((s = strchr(publisher, MICROFEED_PUBLISHER_IDENTIFIER_SEPARATOR_CHAR))) {
			s = g_strndup(publisher, s - publisher);
		} else {
			s = g_strdup(publisher);
		}
		
		gtk_list_store_set(contacts->list_store, &iter,
		                   0, publisher,
				   1, microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_FEED),
		                   3, microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_NICK),
				   4, microfeed_item_get_property(item, MICROFEED_ITEM_PROPERTY_NAME_USER_NAME),
				   5, s,
				   -1);
		g_free(s);

		if (contacts->label) {
			gtk_widget_destroy(contacts->label);
			contacts->label = NULL;
			show_selector(contacts);
		}
	}
}

static void item_removed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, void* user_data) {

}

static void set_progress_indicator(MaukuContacts* contacts) {
	hildon_gtk_window_set_progress_indicator(GTK_WINDOW(contacts->dialog), (contacts->updating || contacts->republishing ? TRUE : FALSE));
}
