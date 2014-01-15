
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef __MAUKU_H__
#define __MAUKU_H__

#include <dbus/dbus.h>
#include <gtk/gtk.h>
#include <microfeed-subscriber/microfeedsubscriber.h>
#include "mauku-write.h"


#define SUBSCRIBER_IDENTIFIER "com.innologies.mauku"
#define IMAGE_DIR "/usr/share/mauku/images"
#define VERSION "2.0 beta 4"

extern DBusConnection* dbus_connection;
extern MicrofeedSubscriber* subscriber;

GdkPixbuf* image_cache_get_image(const gchar* publisher, const gchar* uid);
void add_current_publishers_to_write(MaukuWrite* write);

#endif
