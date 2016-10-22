/*  Copyright 2006-2012 Enrico Tröger <enrico(at)xfce(dot)org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4panel/xfce-panel-plugin.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "libdict.h"
#include "resources.h"


typedef struct
{
	DictData *dd;
	XfcePanelPlugin *plugin;

	GtkWidget *panel_button;
	GtkWidget *panel_button_image;
	GtkWidget *box;
} DictPanelData;


static gboolean entry_is_dirty = FALSE;


static gboolean dict_plugin_panel_set_size(XfcePanelPlugin *plugin, gint wsize, DictPanelData *dpd)
{
  GtkBorder border;
	GtkStyleContext *context;
	gint size;
	gint bsize = wsize;
	bsize /= xfce_panel_plugin_get_nrows (plugin);

	context = gtk_widget_get_style_context (GTK_WIDGET (dpd->panel_button));
	gtk_style_context_get_border (context, gtk_widget_get_state_flags (GTK_WIDGET (dpd->panel_button)), &border);
	size = bsize - 2 * MAX (border.left + border.right, border.top + border.bottom);

	dpd->dd->icon = gdk_pixbuf_new_from_resource_at_scale("/org/xfce/dict/icon",
										size, -1, TRUE, NULL);

	gtk_image_set_from_pixbuf(GTK_IMAGE(dpd->panel_button_image), dpd->dd->icon);

	if (dpd->dd->show_panel_entry &&
		xfce_panel_plugin_get_mode(dpd->plugin) != XFCE_PANEL_PLUGIN_MODE_VERTICAL)
	{
		xfce_panel_plugin_set_small(plugin, FALSE);
		if (xfce_panel_plugin_get_mode(dpd->plugin) == XFCE_PANEL_PLUGIN_MODE_HORIZONTAL)
			gtk_widget_set_size_request(dpd->dd->panel_entry, dpd->dd->panel_entry_size, -1);
		else
			gtk_widget_set_size_request(dpd->dd->panel_entry, -1, -1);
		gtk_orientable_set_orientation(GTK_ORIENTABLE(dpd->box), xfce_panel_plugin_get_orientation(dpd->plugin));
		gtk_widget_show(dpd->dd->panel_entry);
	}
	else
	{
		gtk_widget_hide(dpd->dd->panel_entry);
		xfce_panel_plugin_set_small(plugin, TRUE);
	}

	gtk_widget_set_size_request(dpd->panel_button, bsize, bsize);

	return TRUE;
}


static void dict_plugin_panel_button_clicked(GtkWidget *button, DictPanelData *dpd)
{
	if (gtk_widget_get_visible(GTK_WIDGET(dpd->dd->window)))
	{
		/* we must query geometry settings here because position and maximized state
		 * doesn't work when the window is hidden */
		dict_gui_query_geometry(dpd->dd);

		gtk_widget_hide(GTK_WIDGET(dpd->dd->window));
	}
	else
	{
		dict_gui_show_main_window(dpd->dd);
		/* Do not search the panel text if it is still the default text */
		if (dpd->dd->show_panel_entry &&
			xfce_panel_plugin_get_orientation(dpd->plugin) == GTK_ORIENTATION_HORIZONTAL &&
			entry_is_dirty)
		{
			const gchar *panel_text = gtk_entry_get_text(GTK_ENTRY(dpd->dd->panel_entry));

			if (NZV(panel_text))
			{
				dict_search_word(dpd->dd, panel_text);
				gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), panel_text);
			}
		}
		gtk_widget_grab_focus(dpd->dd->main_entry);
	}
}


static gboolean dict_plugin_set_selection(DictPanelData *dpd)
{
	GdkScreen *gscreen;
	gchar      selection_name[32];
	Atom       selection_atom;
	GtkWidget *win;
	Window     xwin;

	win = gtk_invisible_new();
	gtk_widget_realize(win);
	xwin = GDK_WINDOW_XID(gtk_widget_get_window (GTK_WIDGET(win)));

	gscreen = gtk_widget_get_screen(win);
	g_snprintf(selection_name, sizeof (selection_name),
		XFCE_DICT_SELECTION"%d", gdk_screen_get_number(gscreen));
	selection_atom = XInternAtom(gdk_x11_display_get_xdisplay(gdk_display_get_default()), selection_name, False);

	if (XGetSelectionOwner(gdk_x11_display_get_xdisplay(gdk_display_get_default()), selection_atom))
	{
		gtk_widget_destroy(win);
		return FALSE;
	}

	XSelectInput(gdk_x11_display_get_xdisplay(gdk_display_get_default()), xwin, PropertyChangeMask);
	XSetSelectionOwner(gdk_x11_display_get_xdisplay(gdk_display_get_default()), selection_atom, xwin, GDK_CURRENT_TIME);

	return TRUE;
}


static void dict_plugin_close_button_clicked(GtkWidget *button, DictPanelData *dpd)
{
	gtk_widget_hide(dpd->dd->window);
}


static void dict_plugin_free_data(XfcePanelPlugin *plugin, DictPanelData *dpd)
{
	/* Destroy the setting dialog, if this open */
	GtkWidget *dialog = g_object_get_data(G_OBJECT(dpd->plugin), "dialog");

	/* if the main window is visible, query geometry as usual, if it is hidden the geometry
	 * was queried when it was hidden */
	if (gtk_widget_get_visible(GTK_WIDGET(dpd->dd->window)))
	{
		dict_gui_query_geometry(dpd->dd);
	}

	if (dialog != NULL)
		gtk_widget_destroy(dialog);

	dict_free_data(dpd->dd);
	g_free(dpd);
}


static void dict_plugin_panel_change_mode(XfcePanelPlugin *plugin,
												 XfcePanelPluginMode mode, DictPanelData *dpd)
{
	dict_plugin_panel_set_size(plugin, xfce_panel_plugin_get_size(plugin), dpd);
}


static void dict_plugin_style_set(XfcePanelPlugin *plugin, gpointer unused, DictPanelData *dpd)
{
	dict_plugin_panel_set_size(plugin, xfce_panel_plugin_get_size(plugin), dpd);
}


static void dict_plugin_write_rc_file(XfcePanelPlugin *plugin, DictPanelData *dpd)
{
	dict_write_rc_file(dpd->dd);
}


static void dict_plugin_panel_save_settings(DictPanelData *dpd)
{
	dict_plugin_panel_set_size(dpd->plugin, xfce_panel_plugin_get_size(dpd->plugin), dpd);
}


static void dict_plugin_properties_dialog_response(GtkWidget *dlg, gint response, DictPanelData *dpd)
{
	/* first run the real response handler which reads the settings from the dialog */
	dict_prefs_dialog_response(dlg, response, dpd->dd);

	dict_plugin_panel_save_settings(dpd);

	g_object_set_data(G_OBJECT(dpd->plugin), "dialog", NULL);
	xfce_panel_plugin_unblock_menu(dpd->plugin);
}


static void dict_plugin_properties_dialog(GtkWidget *widget, DictPanelData *dpd)
{
	GtkWidget *dlg;
	XfcePanelPlugin *plugin = dpd->plugin;

	xfce_panel_plugin_block_menu(plugin);

	dlg = dict_prefs_dialog_show(gtk_widget_get_toplevel(GTK_WIDGET(plugin)), dpd->dd);

	g_object_set_data(G_OBJECT(dpd->plugin), "dialog", dlg);

	g_signal_connect(dlg, "response", G_CALLBACK(dict_plugin_properties_dialog_response), dpd);

	gtk_widget_show(dlg);
}


static void entry_activate_cb(GtkEntry *entry, DictPanelData *dpd)
{
	const gchar *entered_text = gtk_entry_get_text(GTK_ENTRY(dpd->dd->panel_entry));

	gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), entered_text);

	dict_search_word(dpd->dd, entered_text);
}


static void entry_icon_release_cb(GtkEntry *entry, GtkEntryIconPosition icon_pos,
		GdkEventButton *event, DictPanelData *dpd)
{
	if (event->button != 1)
		return;

	if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
	{
		entry_activate_cb(NULL, dpd);
		gtk_widget_grab_focus(dpd->dd->main_entry);
	}
	else if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
	{
		dict_gui_clear_text_buffer(dpd->dd);
		gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), "");
		dict_gui_set_panel_entry_text(dpd->dd, "");
		dict_gui_status_add(dpd->dd, _("Ready"));
	}
}


static gboolean entry_buttonpress_cb(GtkWidget *entry, GdkEventButton *event, DictPanelData *dpd)
{
	GtkWidget *toplevel;

	if (! entry_is_dirty)
	{
		entry_is_dirty = TRUE;
		if (event->button == 1)
			gtk_entry_set_text(GTK_ENTRY(entry), "");
	}

	/* Determine toplevel parent widget */
	toplevel = gtk_widget_get_toplevel(entry);

	/* Grab entry focus if possible */
	if (event->button != 3 && toplevel && gtk_widget_get_window (toplevel))
		xfce_panel_plugin_focus_widget(dpd->plugin, entry);

	return FALSE;
}


static void entry_changed_cb(GtkEditable *editable, DictPanelData *dpd)
{
	entry_is_dirty = TRUE;
}


static void dict_plugin_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, GtkSelectionData *data, guint info, guint ltime, DictPanelData *dpd)
{
	if ((data != NULL) && (gtk_selection_data_get_length(data) >= 0) && (gtk_selection_data_get_format(data) == 8))
	{
		if (widget == dpd->panel_button || widget == dpd->dd->panel_entry)
		{
			gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), (const gchar*) gtk_selection_data_get_data(data));
		}

		dict_drag_data_received(widget, drag_context, x, y, data, info, ltime, dpd->dd);
	}
}


static void dict_plugin_construct(XfcePanelPlugin *plugin)
{
	GtkCssProvider *css_provider;
	DictPanelData *dpd = g_new0(DictPanelData, 1);

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	dpd->dd = dict_create_dictdata();
	dpd->dd->is_plugin = TRUE;
	dpd->plugin = plugin;

	dict_read_rc_file(dpd->dd);

	dpd->panel_button = xfce_create_panel_button();
	gtk_widget_set_tooltip_text (dpd->panel_button, _("Look up a word"));

	dpd->panel_button_image = gtk_image_new();
	gtk_container_add(GTK_CONTAINER(dpd->panel_button), GTK_WIDGET(dpd->panel_button_image));

	/* Setup Gtk style */
	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (css_provider, "button { padding: 1px; border-width: 1px;}", -1, NULL);
	gtk_style_context_add_provider (GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (dpd->panel_button))),
																	GTK_STYLE_PROVIDER (css_provider),
																	GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_widget_show_all(dpd->panel_button);

	g_signal_connect(dpd->panel_button, "clicked", G_CALLBACK(dict_plugin_panel_button_clicked), dpd);

	dict_gui_create_main_window(dpd->dd);

	g_signal_connect(dpd->dd->window, "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(dpd->dd->close_button, "clicked", G_CALLBACK(dict_plugin_close_button_clicked), dpd);
	g_signal_connect(plugin, "free-data", G_CALLBACK(dict_plugin_free_data), dpd);
	g_signal_connect(plugin, "size-changed", G_CALLBACK(dict_plugin_panel_set_size), dpd);
	g_signal_connect(plugin, "mode-changed", G_CALLBACK(dict_plugin_panel_change_mode), dpd);
	g_signal_connect(plugin, "style-set", G_CALLBACK(dict_plugin_style_set), dpd);
	g_signal_connect(plugin, "save", G_CALLBACK(dict_plugin_write_rc_file), dpd);
	g_signal_connect(plugin, "configure-plugin", G_CALLBACK(dict_plugin_properties_dialog), dpd);
	g_signal_connect(plugin, "about", G_CALLBACK(dict_gui_about_dialog), dpd->dd);

	xfce_panel_plugin_menu_show_configure(plugin);
	xfce_panel_plugin_menu_show_about(plugin);

	/* file menu */
	g_signal_connect(dpd->dd->close_menu_item, "activate", G_CALLBACK(dict_plugin_close_button_clicked), dpd);
	g_signal_connect(dpd->dd->pref_menu_item, "activate", G_CALLBACK(dict_plugin_properties_dialog), dpd);

	/* panel entry */
	dpd->dd->panel_entry = gtk_search_entry_new();
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(dpd->dd->panel_entry), GTK_ENTRY_ICON_SECONDARY, "gtk-clear");
	gtk_entry_set_width_chars(GTK_ENTRY(dpd->dd->panel_entry), 25);
	gtk_entry_set_placeholder_text(GTK_ENTRY(dpd->dd->panel_entry), _("Search term"));
	g_signal_connect(dpd->dd->panel_entry, "icon-release", G_CALLBACK(entry_icon_release_cb), dpd);
	g_signal_connect(dpd->dd->panel_entry, "activate", G_CALLBACK(entry_activate_cb), dpd);
	g_signal_connect(dpd->dd->panel_entry, "button-press-event", G_CALLBACK(entry_buttonpress_cb), dpd);
	g_signal_connect(dpd->dd->panel_entry, "changed", G_CALLBACK(entry_changed_cb), dpd);

	dpd->box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 3);
	gtk_widget_show(dpd->box);

	gtk_box_pack_start(GTK_BOX(dpd->box), dpd->panel_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(dpd->box), dpd->dd->panel_entry, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(plugin), dpd->box);

	dict_plugin_panel_set_size(dpd->plugin, xfce_panel_plugin_get_size(dpd->plugin), dpd);

	xfce_panel_plugin_add_action_widget(plugin, dpd->panel_button);
	dict_plugin_set_selection(dpd);

	/* DnD stuff */
	gtk_drag_dest_set(GTK_WIDGET(dpd->panel_button), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_text_targets(GTK_WIDGET(dpd->panel_button));
	g_signal_connect(dpd->panel_button, "drag-data-received", G_CALLBACK(dict_plugin_drag_data_received), dpd);
	g_signal_connect(dpd->dd->panel_entry, "drag-data-received", G_CALLBACK(dict_plugin_drag_data_received), dpd);

	dict_acquire_dbus_name(dpd->dd);

	dict_gui_status_add(dpd->dd, _("Ready"));
}
XFCE_PANEL_PLUGIN_REGISTER(dict_plugin_construct);
