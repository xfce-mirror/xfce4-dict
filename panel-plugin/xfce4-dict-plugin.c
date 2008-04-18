/*  $Id$
 *
 *  Copyright 2006-2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-convenience.h>

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


typedef struct
{
	DictData *dd;
	XfcePanelPlugin *plugin;

	GtkTooltips *tooltips;

	GtkWidget *panel_button;
	GtkWidget *panel_button_image;
} DictPanelData;


static GdkPixbuf *dict_plugin_load_and_scale(const guint8 *data, gint dstw, gint dsth)
{
	GdkPixbuf *pb, *pb_scaled;
	gint pb_w, pb_h;

	pb = gdk_pixbuf_new_from_inline(-1, data, FALSE, NULL);
	pb_w = gdk_pixbuf_get_width(pb);
	pb_h = gdk_pixbuf_get_height(pb);

	if (dstw == pb_w && dsth == pb_h)
		return(pb);
	else if (dstw < 0)
		dstw = (dsth * pb_w) / pb_h;
	else if (dsth < 0)
		dsth = (dstw * pb_h) / pb_w;

	pb_scaled = gdk_pixbuf_scale_simple(pb, dstw, dsth, GDK_INTERP_HYPER);
	g_object_unref(G_OBJECT(pb));

	return pb_scaled;
}


static gboolean dict_plugin_panel_set_size(XfcePanelPlugin *plugin, gint wsize, DictPanelData *dpd)
{
	gint width;
	gint height = wsize;
	gint size = wsize - 2 - (2 * MAX(dpd->panel_button->style->xthickness,
									 dpd->panel_button->style->ythickness));

	dpd->dd->icon = dict_plugin_load_and_scale(dict_gui_get_icon_data(), size, -1);

	gtk_image_set_from_pixbuf(GTK_IMAGE(dpd->panel_button_image), dpd->dd->icon);

	if (dpd->dd->show_panel_entry &&
		xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_HORIZONTAL)
	{
		width = size + dpd->dd->panel_entry_size;
		gtk_widget_set_size_request(dpd->dd->panel_entry, dpd->dd->panel_entry_size, -1);
	}
	else
		width = size;

	if (xfce_panel_plugin_get_orientation(plugin) == GTK_ORIENTATION_VERTICAL)
		height -= 4; /* reduce some of the height because it creates too much space otherwise */

	gtk_widget_set_size_request(dpd->panel_button, wsize, height);

	return TRUE;
}


/* TODO remove me, unused
static void dict_toggle_main_window(GtkWidget *button, DictData *dd)
{
	if (GTK_WIDGET_VISIBLE(dd->window))
		gtk_widget_hide(dd->window);
	else
	{
		const gchar *panel_text = "";

		if (dd->panel_entry != NULL)
			panel_text = gtk_entry_get_text(GTK_ENTRY(dd->panel_entry));

		dict_show_main_window(dd);
		if (NZV(panel_text))
		{
			dict_search_word(dd, panel_text);
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), panel_text);
		}
		gtk_widget_grab_focus(dd->main_entry);
	}
}
*/


static void dict_plugin_panel_button_clicked(GtkWidget *button, DictPanelData *dpd)
{
	if (GTK_WIDGET_VISIBLE(dpd->dd->window))
		gtk_widget_hide(dpd->dd->window);
	else
	{
		const gchar *panel_text = gtk_entry_get_text(GTK_ENTRY(dpd->dd->panel_entry));

		dict_gui_show_main_window(dpd->dd);
		if (NZV(panel_text))
		{
			dict_search_word(dpd->dd, panel_text);
			gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), panel_text);
		}
		gtk_widget_grab_focus(dpd->dd->main_entry);
	}
}


/* Handle user messages (xfce4-dict) */
static gboolean dict_plugin_message_received(GtkWidget *w, GdkEventClient *ev, DictPanelData *dpd)
{
	if (ev->data_format == 8 && strncmp(ev->data.b, "xfdict", 6) == 0)
	{
		gchar flags = ev->data.b[6];
		gchar *tts = ev->data.b + 7;

		dpd->dd->mode_in_use = dict_set_search_mode_from_flags(dpd->dd->mode_in_use, flags);

		if (NZV(tts))
		{
			gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), tts);
			dict_search_word(dpd->dd, tts);
		}
		else if (flags & DICT_FLAGS_FOCUS_PANEL_ENTRY && dpd->dd->show_panel_entry)
		{
			xfce_panel_plugin_focus_widget(dpd->plugin, dpd->dd->panel_entry);
		}
		else
		{
			dict_plugin_panel_button_clicked(NULL, dpd);
		}

		return TRUE;
	}

	return FALSE;
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
	xwin = GDK_WINDOW_XID(GTK_WIDGET(win)->window);

	gscreen = gtk_widget_get_screen(win);
	g_snprintf(selection_name, sizeof (selection_name),
		XFCE_DICT_SELECTION"%d", gdk_screen_get_number(gscreen));
	selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

	if (XGetSelectionOwner(GDK_DISPLAY(), selection_atom))
	{
		gtk_widget_destroy(win);
		return FALSE;
	}

	XSelectInput(GDK_DISPLAY(), xwin, PropertyChangeMask);
	XSetSelectionOwner(GDK_DISPLAY(), selection_atom, xwin, GDK_CURRENT_TIME);

	g_signal_connect(G_OBJECT(win), "client-event", G_CALLBACK(dict_plugin_message_received), dpd);

	return TRUE;
}


void dict_plugin_close_button_clicked(GtkWidget *button, DictPanelData *dpd)
{
	gtk_widget_hide(dpd->dd->window);
}


static void dict_plugin_free_data(XfcePanelPlugin *plugin, DictPanelData *dpd)
{
	/* Destroy the setting dialog, if this open */
	GtkWidget *dialog = g_object_get_data(G_OBJECT(dpd->plugin), "dialog");

	if (dialog != NULL)
		gtk_widget_destroy(dialog);

	gtk_object_sink(GTK_OBJECT(dpd->tooltips));

	dict_free_data(dpd->dd);
	g_free(dpd);
}


static void dict_plugin_panel_change_orientation(XfcePanelPlugin *plugin,
												 GtkOrientation orientation, DictPanelData *dpd)
{
	if (! dpd->dd->show_panel_entry || orientation == GTK_ORIENTATION_VERTICAL)
		gtk_widget_hide(dpd->dd->panel_entry);
	else
		gtk_widget_show(dpd->dd->panel_entry);
}


static void dict_plugin_style_set(XfcePanelPlugin *plugin, gpointer unused, DictPanelData *dpd)
{
	dict_plugin_panel_set_size(plugin, xfce_panel_plugin_get_size(plugin), dpd);
}


static gboolean dict_plugin_panel_entry_buttonpress_cb(GtkWidget *entry, GdkEventButton *event, DictPanelData *dpd)
{
	GtkWidget *toplevel;

	/* Determine toplevel parent widget */
	toplevel = gtk_widget_get_toplevel(entry);

	/* Grab entry focus if possible */
	if (event->button != 3 && toplevel && toplevel->window)
		xfce_panel_plugin_focus_widget(dpd->plugin, entry);

	return FALSE;
}


static void dict_plugin_write_rc_file(XfcePanelPlugin *plugin, DictPanelData *dpd)
{
	dict_write_rc_file(dpd->dd);
}


static void dict_plugin_panel_save_settings(DictPanelData *dpd)
{
	if (dpd->dd->show_panel_entry &&
		xfce_panel_plugin_get_orientation(dpd->plugin) == GTK_ORIENTATION_HORIZONTAL)
	{
		gtk_widget_show(dpd->dd->panel_entry);
	}
	else
		gtk_widget_hide(dpd->dd->panel_entry);

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


static void dict_plugin_properties_dialog(XfcePanelPlugin *plugin, DictPanelData *dpd)
{
	GtkWidget *dlg;

	xfce_panel_plugin_block_menu(plugin);

	dlg = dict_prefs_dialog_show(gtk_widget_get_toplevel(GTK_WIDGET(plugin)), dpd->dd);

	g_object_set_data(G_OBJECT(dpd->plugin), "dialog", dlg);

	g_signal_connect(dlg, "response", G_CALLBACK(dict_plugin_properties_dialog_response), dpd);

	gtk_widget_show(dlg);
}


static void dict_plugin_panel_entry_activate_cb(GtkEntry *entry, DictPanelData *dpd)
{
	const gchar *entered_text = gtk_entry_get_text(GTK_ENTRY(dpd->dd->panel_entry));

	gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), entered_text);

	dict_search_word(dpd->dd, entered_text);
}


static void dict_plugin_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
		gint x, gint y, GtkSelectionData *data, guint info, guint ltime, DictPanelData *dpd)
{
	if ((data != NULL) && (data->length >= 0) && (data->format == 8))
	{
		if (widget == dpd->panel_button || widget == dpd->dd->panel_entry)
		{
			gtk_entry_set_text(GTK_ENTRY(dpd->dd->main_entry), (const gchar*) data->data);
		}

		dict_drag_data_received(widget, drag_context, x, y, data, info, ltime, dpd->dd);
	}
}


static void dict_plugin_construct(XfcePanelPlugin *plugin)
{
	DictPanelData *dpd = g_new0(DictPanelData, 1);
	GtkWidget *hbox;

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	g_thread_init(NULL);

	dpd->dd = dict_create_dictdata();
	dpd->dd->is_plugin = TRUE;
	dpd->plugin = plugin;

	dict_read_rc_file(dpd->dd);

	dpd->panel_button = xfce_create_panel_button();

	dpd->tooltips = gtk_tooltips_new();
	gtk_tooltips_set_tip(dpd->tooltips, dpd->panel_button, _("Look up a word"), NULL);

	dpd->panel_button_image = gtk_image_new();
	gtk_container_add(GTK_CONTAINER(dpd->panel_button), GTK_WIDGET(dpd->panel_button_image));

	gtk_widget_show_all(dpd->panel_button);

	g_signal_connect(dpd->panel_button, "clicked", G_CALLBACK(dict_plugin_panel_button_clicked), dpd);

	dict_gui_create_main_window(dpd->dd);

	g_signal_connect(dpd->dd->window, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(dpd->dd->close_button, "clicked", G_CALLBACK(dict_plugin_close_button_clicked), dpd);
	g_signal_connect(plugin, "free-data", G_CALLBACK(dict_plugin_free_data), dpd);
	g_signal_connect(plugin, "size-changed", G_CALLBACK(dict_plugin_panel_set_size), dpd);
	g_signal_connect(plugin, "orientation-changed", G_CALLBACK(dict_plugin_panel_change_orientation), dpd);
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
	dpd->dd->panel_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(dpd->dd->panel_entry), 15);
	g_signal_connect(dpd->dd->panel_entry, "activate",
		G_CALLBACK(dict_plugin_panel_entry_activate_cb), dpd);
	g_signal_connect(dpd->dd->panel_entry, "button-press-event",
		G_CALLBACK(dict_plugin_panel_entry_buttonpress_cb), dpd);

	if (dpd->dd->show_panel_entry &&
		xfce_panel_plugin_get_orientation(dpd->plugin) == GTK_ORIENTATION_HORIZONTAL)
	{
		gtk_widget_show(dpd->dd->panel_entry);
	}

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);

	gtk_box_pack_start(GTK_BOX(hbox), dpd->panel_button, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox), dpd->dd->panel_entry, FALSE, FALSE, 0);
	gtk_container_add(GTK_CONTAINER(plugin), hbox);

	xfce_panel_plugin_add_action_widget(plugin, dpd->panel_button);
	dict_plugin_set_selection(dpd);

	/* DnD stuff */
	gtk_drag_dest_set(GTK_WIDGET(dpd->panel_button), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_text_targets(GTK_WIDGET(dpd->panel_button));
	g_signal_connect(dpd->panel_button, "drag-data-received", G_CALLBACK(dict_plugin_drag_data_received), dpd);
	g_signal_connect(dpd->dd->panel_entry, "drag-data-received", G_CALLBACK(dict_plugin_drag_data_received), dpd);

	dict_gui_status_add(dpd->dd, _("Ready."));
}
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(dict_plugin_construct);
