
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#include "mauku.h"
#include "mauku-write.h"
#include <microfeed-common/microfeedmisc.h>
#include <microfeed-common/microfeedprotocol.h>
#include <hildon/hildon.h>
#include <string.h>

typedef struct {
	gchar* name;
	gchar* publisher;
	gchar* uri;
	gboolean active;
} Subscription;

struct _MaukuWrite {
	MaukuItem* item;
	gchar* prefix;
	gchar* text;
	
	GtkWidget* window;
	GtkWidget* pannable_area;
	GtkWidget* container;
	GtkWidget* to_button;
	GtkTextBuffer* text_buffer;
	GtkTextMark* start_mark;
	GtkTextMark* end_mark;
	GtkWidget* chars_label;
	GtkWidget* send_button;
	GArray* subscriptions;
};

void mauku_write_free(MaukuWrite* write);
static void set_progress_indicator(MaukuWrite* write);

static void update_labels(MaukuWrite* write) {
	gint count;
	gchar* s;
	GtkTextIter start_iter;
	GtkTextIter end_iter;
	gchar* prefix;
	
	count = gtk_text_buffer_get_char_count(write->text_buffer);
	if (write->chars_label) {
		s = g_strdup_printf("%d characters left", 140 - count);
		gtk_label_set_text(GTK_LABEL(write->chars_label), s);
		g_free(s);
		gtk_widget_set_sensitive(write->send_button, count > 0 && count <= 140);
	} else {
		gtk_widget_set_sensitive(write->send_button, count > 0);
	}
	
	if (write->prefix) {
		gtk_text_buffer_get_iter_at_mark(write->text_buffer, &start_iter, write->start_mark);
		gtk_text_buffer_get_iter_at_mark(write->text_buffer, &end_iter, write->end_mark);
		if (strcmp(gtk_text_buffer_get_text(write->text_buffer, &start_iter, &end_iter, FALSE), write->prefix)) {
			prefix = write->prefix;
			write->prefix = NULL;
			gtk_text_buffer_delete(write->text_buffer, &start_iter, &end_iter);
			gtk_text_buffer_get_iter_at_mark(write->text_buffer, &start_iter, write->start_mark);
			gtk_text_buffer_get_iter_at_mark(write->text_buffer, &end_iter, write->end_mark);
			gtk_text_buffer_insert(write->text_buffer, &start_iter, prefix, -1);
			gtk_text_buffer_delete_mark(write->text_buffer, write->end_mark);
			write->end_mark = gtk_text_buffer_create_mark(write->text_buffer, NULL, &start_iter, TRUE);
			write->prefix = prefix;
		}
	}
}

static void item_added(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	MaukuWrite* write;
	
	write = (MaukuWrite*)user_data;
	if (error_name) {
	
	} else {
		mauku_write_free(write);
	}
}

static void on_send_button_clicked(MaukuWrite* write) {
	GtkTextIter start_iter;
	GtkTextIter end_iter;
	const gchar* text;
	MicrofeedItem* item;
	gint i;
	Subscription subscription;

	gtk_text_buffer_get_start_iter(write->text_buffer, &start_iter);
	gtk_text_buffer_get_end_iter(write->text_buffer, &end_iter);
	text = gtk_text_buffer_get_text(write->text_buffer, &start_iter, &end_iter, FALSE);
	
	gtk_widget_hide(write->window);
	
	item = microfeed_item_new_temporary();
	microfeed_item_set_property(item, MICROFEED_ITEM_PROPERTY_NAME_CONTENT_TEXT, text);
	microfeed_item_set_property(item, ".twitter.source", "mauku");
	if (write->item) {
		microfeed_item_set_property(item, MICROFEED_ITEM_PROPERTY_NAME_REFERRED_ITEM, mauku_item_get_uid(write->item));

	}
	
	for (i = 0; i< write->subscriptions->len; i++) {
		subscription = g_array_index(write->subscriptions, Subscription, i);
		if (subscription.active) {
			microfeed_subscriber_add_item(subscriber, subscription.publisher, subscription.uri, item, item_added, write);
		}
	}
}

void update_to_button(MaukuWrite* write) {
	gint i;
	Subscription subscription;
	GString* s;

	s = g_string_new("");
	for (i = 0; i< write->subscriptions->len; i++) {
		subscription = g_array_index(write->subscriptions, Subscription, i);
		if (subscription.active) {
			g_string_append(s, subscription.name);
			g_string_append(s, ", ");
		}
	}
	if (s->len > 2) {
		g_string_set_size(s, s->len - 2);
	} else {
		g_string_append(s, "<select at least one>");
	}
	hildon_button_set_value(HILDON_BUTTON(write->to_button), s->str);
	g_string_free(s, TRUE);
}

static void on_to_button_clicked(MaukuWrite* write) {
	GtkWidget* picker_dialog;
	GtkWidget* touch_selector;
	GtkListStore* list_store;
	GtkTreeIter iter;
	gint i;
	Subscription subscription;
	GArray* subscriptions;
	GList* selected;
	GList* list_item;
	gchar* s;

	picker_dialog = hildon_picker_dialog_new(GTK_WINDOW(write->window));
	gtk_window_set_title(GTK_WINDOW(picker_dialog), "Select publishers");
	touch_selector = hildon_touch_selector_new();
	list_store = gtk_list_store_new(1, G_TYPE_STRING);
	hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(touch_selector), GTK_TREE_MODEL(list_store), TRUE);
	hildon_touch_selector_set_column_selection_mode(HILDON_TOUCH_SELECTOR(touch_selector), HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE);
	for (i = 0; i < write->subscriptions->len; i++) {
		subscription = g_array_index(write->subscriptions, Subscription, i);
		gtk_list_store_append(list_store, &iter);
		gtk_list_store_set(list_store, &iter, 0, subscription.name, -1);
		if (subscription.active) {
			hildon_touch_selector_select_iter(HILDON_TOUCH_SELECTOR(touch_selector), 0, &iter, FALSE);
		}
	}
	hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(picker_dialog), HILDON_TOUCH_SELECTOR(touch_selector));
	if (gtk_dialog_run(GTK_DIALOG(picker_dialog)) == GTK_RESPONSE_OK) {		
		subscriptions = g_array_new(FALSE, FALSE, sizeof(Subscription));
		selected = hildon_touch_selector_get_selected_rows(HILDON_TOUCH_SELECTOR(touch_selector), 0);
		for (i = 0; i < write->subscriptions->len; i++) {
			subscription = g_array_index(write->subscriptions, Subscription, i);
			subscription.active = FALSE;
			for (list_item = selected; list_item; list_item = list_item->next) {
				gtk_tree_model_get_iter(GTK_TREE_MODEL(list_store), &iter, (GtkTreePath*)list_item->data);
				gtk_tree_model_get(GTK_TREE_MODEL(list_store), &iter, 0, &s, -1);
				if (!strcmp(subscription.name, s)) {
					subscription.active = TRUE;
					break;
				}
				g_free(s);
			}
			g_array_append_val(subscriptions, subscription);
		}
		g_list_foreach(selected, (GFunc)gtk_tree_path_free, NULL);
		g_list_free(selected);

		g_array_free(write->subscriptions, TRUE);
		write->subscriptions = subscriptions;
		update_to_button(write);
	}
	
	gtk_widget_destroy(picker_dialog);
}

MaukuWrite* mauku_write_new(const gchar* title, MaukuItem* item, const gchar* prefix, const gchar* text) {
	MaukuWrite* write;
	GtkWidget* table;
	GtkWidget* text_view;
	GtkTextIter start_iter;
	GtkTextIter end_iter;
	gchar* s;
	gint count;
	
	write = microfeed_memory_allocate(MaukuWrite);
	write->item = item;
	write->prefix = g_strdup(prefix);
	write->text = g_strdup(text);
	write->subscriptions = g_array_new(FALSE, FALSE, sizeof(Subscription));
	
	write->window = hildon_stackable_window_new();
	g_signal_connect(write->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	gtk_window_set_title(GTK_WINDOW(write->window), title);

	table = gtk_table_new(2, 2, FALSE);
	gtk_container_add(GTK_CONTAINER(write->window), table);

	write->pannable_area = hildon_pannable_area_new();
	g_object_set(write->pannable_area, "hscrollbar-policy", GTK_POLICY_NEVER, "vovershoot-max", 400, NULL);
	gtk_table_attach(GTK_TABLE(table), write->pannable_area, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL | GTK_SHRINK, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0);

	write->container = gtk_table_new(2, 1, FALSE);
	hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(write->pannable_area), write->container);

	if (write->item) {
		gtk_table_attach(GTK_TABLE(write->container), GTK_WIDGET(write->item), 0, 1, 0, 1, GTK_EXPAND |GTK_SHRINK | GTK_FILL, 0, 4, 0);
	} else {
		write->to_button = hildon_button_new_with_text(HILDON_SIZE_AUTO_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL, "To", "");
		g_signal_connect_swapped(write->to_button, "clicked", G_CALLBACK(on_to_button_clicked), write);
		gtk_table_attach(GTK_TABLE(write->container), write->to_button, 0, 1, 0, 1, GTK_EXPAND |GTK_SHRINK | GTK_FILL, 0, 0, 0);
	}
	
	text_view = hildon_text_view_new();
	write->text_buffer = hildon_text_view_get_buffer(HILDON_TEXT_VIEW(text_view));
	hildon_text_view_set_placeholder(HILDON_TEXT_VIEW(text_view), "Type your message here");
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
	gtk_table_attach(GTK_TABLE(write->container), text_view, 0, 1, 1, 2, GTK_EXPAND |GTK_SHRINK | GTK_FILL, GTK_EXPAND |GTK_SHRINK | GTK_FILL, 0, 0);

	if (write->prefix) {
		gtk_text_buffer_set_text(write->text_buffer, write->prefix, -1);
		gtk_text_buffer_get_bounds(write->text_buffer, &start_iter, &end_iter);
		write->start_mark = gtk_text_buffer_create_mark(write->text_buffer, NULL, &start_iter, TRUE);
		write->end_mark = gtk_text_buffer_create_mark(write->text_buffer, NULL, &end_iter, TRUE);
	}
	if (write->text) {
		gtk_text_buffer_get_bounds(write->text_buffer, &start_iter, &end_iter);
		gtk_text_buffer_insert(write->text_buffer, &end_iter, write->text, -1);
	}

	write->chars_label = gtk_label_new("140 characters left");
	gtk_table_attach(GTK_TABLE(table), write->chars_label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL | GTK_SHRINK, 0, 0, 0);

	write->send_button = hildon_button_new_with_text(HILDON_SIZE_HALFSCREEN_WIDTH | HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL, "Send", NULL);
	g_signal_connect_swapped(write->send_button, "clicked", G_CALLBACK(on_send_button_clicked), write);
	gtk_table_attach(GTK_TABLE(table), write->send_button, 1, 2, 1, 2, 0, 0, 0, 0);

	count = gtk_text_buffer_get_char_count(write->text_buffer);
	if (write->chars_label) {
		s = g_strdup_printf("%d characters left", 140 - count);
		gtk_label_set_text(GTK_LABEL(write->chars_label), s);
		g_free(s);
		gtk_widget_set_sensitive(write->send_button, count > 0 && count <= 140);
	} else {
		gtk_widget_set_sensitive(write->send_button, count > 0);
	}
	
	g_signal_connect_swapped(write->text_buffer, "changed", G_CALLBACK(update_labels), write);
		
	return write;
}

void mauku_write_free(MaukuWrite* write) {
	g_free(write->prefix);
	g_free(write->text);
	gtk_widget_destroy(write->window);
	g_array_free(write->subscriptions, TRUE);
	microfeed_memory_free(write);
}

void mauku_write_show(MaukuWrite* write) {
	gtk_widget_show_all(write->window);
}

void mauku_write_add_feed(MaukuWrite* write, const gchar* publisher, const gchar* uri) {
	Subscription subscription;
	gchar* s;

	printf("mauku_write_add_feed: %s\n", publisher);
	
	if ((s = strchr(publisher, MICROFEED_PUBLISHER_IDENTIFIER_SEPARATOR_CHAR))) {
		subscription.name = g_strndup(publisher, s - publisher);
	} else {
		subscription.name = g_strdup(publisher);
	}
	subscription.publisher = g_strdup(publisher);
	subscription.uri = g_strdup(uri);
	subscription.active = TRUE;
	g_array_append_val(write->subscriptions, subscription);
	
	update_to_button(write);
}

static void feed_subscribed(MicrofeedSubscriber* subscriber, const char* publisher, const char* uri, const char* uid, const char* error_name, const char* error_message, void* user_data) {
	printf("MaukuWrite::feed_subscribed: %s %s %s %s\n", publisher, uri, error_name, error_message);
}
