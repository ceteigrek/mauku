
/* Mauku 2.0 (c) Henrik Hedberg <hhedberg@innologies.fi> 
   You are NOT allowed to modify or redistribute the source code. */

#ifndef __MAUKU_CONTACTS_H__
#define __MAUKU_CONTACTS_H__

#include <gtk/gtk.h>


typedef struct _MaukuContacts MaukuContacts;

MaukuContacts* mauku_contacts_new(GtkWindow* parent_window);
void mauku_contacts_show(MaukuContacts* contacts);
void mauku_contacts_add_publisher(MaukuContacts* contacts, const gchar* publisher); // MicrofeedItemDataCache* item_data_cache);

#endif
