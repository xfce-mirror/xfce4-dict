/*  $Id$
 *
 *  Copyright 2008-2011 Enrico Tröger <enrico(at)xfce(dot)org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */


/* Subclass of a GtkComboBoxEntry which embeds a SexyIconEntry instead of
 * a plain GtkEntry. */


#include <gtk/gtk.h>
#include <string.h>
#include "searchentry.h"
#include "sexy-icon-entry.h"


enum
{
    ACTIVE_CHANGED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];


G_DEFINE_TYPE(XfdSearchEntry, xfd_search_entry, GTK_TYPE_COMBO_BOX_ENTRY)


static void xfd_search_entry_class_init(XfdSearchEntryClass *klass)
{
	signals[ACTIVE_CHANGED] = g_signal_new("active-changed",
											G_TYPE_FROM_CLASS (klass),
											(GSignalFlags) (G_SIGNAL_RUN_LAST),
											0,
											0,
											NULL,
											g_cclosure_marshal_VOID__VOID,
											G_TYPE_NONE, 0);
}


static void combo_changed_cb(GtkComboBox *combo_box, gpointer data)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo_box, &iter))
	{
		g_signal_emit(XFD_SEARCH_ENTRY(combo_box), signals[ACTIVE_CHANGED], 0);
	}
}


static void xfd_search_entry_init(XfdSearchEntry *self)
{
	GtkWidget *entry;
	GtkCellRenderer *text_renderer;

    /* we want the widget to have appears-as-list applied
	 * (code and idea stolen from Midori, thanks Christian) */
    gtk_rc_parse_string("style \"xfd-search-entry-style\" {\n"
						"  GtkComboBox::appears-as-list = 1\n }\n"
						"widget_class \"*XfdSearchEntry\" "
						"style \"xfd-search-entry-style\"\n");

	entry = sexy_icon_entry_new_full("gtk-find", "gtk-clear");

    gtk_widget_show(entry);
    gtk_container_add(GTK_CONTAINER(self), entry);

	text_renderer = gtk_cell_renderer_text_new();
	gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(self), text_renderer, TRUE);

	gtk_combo_box_set_active(GTK_COMBO_BOX(self), -1);

	g_signal_connect(self, "changed", G_CALLBACK(combo_changed_cb), NULL);
}


GtkWidget *xfd_search_entry_new(const gchar *text)
{
	GtkWidget *widget;
	GtkListStore *store;

	store = gtk_list_store_new(1, G_TYPE_STRING);

	widget = g_object_new(XFD_SEARCH_ENTRY_TYPE, "model", store, "text-column", 0, NULL);

	gtk_entry_set_text(GTK_ENTRY(gtk_bin_get_child(GTK_BIN(widget))), text);

	g_object_unref(store);

	return widget;
}


void xfd_search_entry_prepend_text(XfdSearchEntry *search_entry, const gchar *text)
{
	GtkTreeIter iter;
	GtkListStore *store;
	GtkTreeModel *model;
	gint i, len;
	gchar *str;

	g_return_if_fail(GTK_IS_COMBO_BOX(search_entry));
	g_return_if_fail(text != NULL);

	model = gtk_combo_box_get_model(GTK_COMBO_BOX(search_entry));
	store = GTK_LIST_STORE(model);

	/* check whether 'text' is already in the model */
	len = gtk_tree_model_iter_n_children(model, NULL);
	for (i = 0; i < len; i++)
	{
		if (gtk_tree_model_iter_nth_child(model, &iter, NULL, i))
		{
			gtk_tree_model_get(model, &iter, 0, &str, -1);
			if (str != NULL && strcmp(str, text) == 0)
			{
				/* if we found 'text' in the model, move it to the top of the list and return */
				gtk_list_store_move_after(store, &iter, NULL);

				g_free(str);
				return;
			}
			g_free(str);
		}
	}

	/* add 'text' to the model/store */
	gtk_list_store_prepend(store, &iter);
	gtk_list_store_set(store, &iter, 0, text, -1);
}

