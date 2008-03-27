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
#include <signal.h>
#include <string.h>

#include "common.h"
#include "popup_def.h"



static GdkPixbuf *dict_load_and_scale(const guint8 *data, gint dstw, gint dsth)
{
	GdkPixbuf *pb, *pb_scaled;
	int pb_w, pb_h;

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

	return (pb_scaled);
}


static gboolean dict_plugin_panel_set_size(XfcePanelPlugin *plugin, gint wsize, DictData *dd)
{
	gint width;
	gint size = wsize - 2 - (2 * MAX(dd->panel_button->style->xthickness,
									 dd->panel_button->style->ythickness));

	/*g_object_unref(G_OBJECT(dd->icon));*/
	dd->icon = dict_load_and_scale(dict_get_icon_data(), size, -1);

	gtk_image_set_from_pixbuf(GTK_IMAGE(dd->panel_button_image), dd->icon);

	if (dd->show_panel_entry && xfce_panel_plugin_get_orientation(dd->plugin) == GTK_ORIENTATION_HORIZONTAL)
	{
		width = size + dd->panel_entry_size;
		gtk_widget_set_size_request(dd->panel_entry, dd->panel_entry_size, -1);
	}
	else
		width = size;

	gtk_widget_set_size_request(GTK_WIDGET(plugin), width, size);

	return TRUE;
}


/* unused
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


static void dict_plugin_panel_button_clicked(GtkWidget *button, DictData *dd)
{
	if (GTK_WIDGET_VISIBLE(dd->window))
		gtk_widget_hide(dd->window);
	else
	{
		const gchar *panel_text = gtk_entry_get_text(GTK_ENTRY(dd->panel_entry));

		dict_show_main_window(dd);
		if (NZV(panel_text))
		{
			dict_search_word(dd, panel_text);
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), panel_text);
		}
		gtk_widget_grab_focus(dd->main_entry);
	}
}


/* Handle user messages (xfce4-popup-dict) */
static gboolean dict_plugin_message_received(GtkWidget *w, GdkEventClient *ev, gpointer user_data)
{
	DictData *dd = user_data;

	if (ev->data_format == 8 && *(ev->data.b) != '\0')
	{
		if (strcmp(XFCE_DICT_WINDOW_MESSAGE, ev->data.b) == 0)
		{	/* open the main window */
			dict_plugin_panel_button_clicked(NULL, dd);
			return TRUE;
		}

		if (strcmp(XFCE_DICT_TEXTFIELD_MESSAGE, ev->data.b) == 0)
		{	/* put the focus onto the panel entry */
			if (dd->show_panel_entry)
				xfce_panel_plugin_focus_widget(dd->plugin, dd->panel_entry);
		}
	}

	return FALSE;
}


static gboolean dict_plugin_set_selection(DictData *dd)
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

	g_signal_connect(G_OBJECT(win), "client-event", G_CALLBACK(dict_plugin_message_received), dd);

	return TRUE;
}


void dict_plugin_close_button_clicked(GtkWidget *button, DictData *dd)
{
	gtk_widget_hide(dd->window);
}


static void dict_plugin_free_data(XfcePanelPlugin *plugin, DictData *dd)
{
	dict_free_data(dd);
}


static void dict_plugin_panel_change_orientation(
		XfcePanelPlugin *plugin, GtkOrientation orientation, DictData *dd)
{
	if (! dd->show_panel_entry || orientation == GTK_ORIENTATION_VERTICAL)
		gtk_widget_hide(dd->panel_entry);
	else
		gtk_widget_show(dd->panel_entry);
}


static void dict_plugin_style_set(XfcePanelPlugin *plugin, gpointer unused, DictData *dd)
{
	dict_plugin_panel_set_size(plugin, xfce_panel_plugin_get_size(plugin), dd);
}


static gboolean dict_panel_entry_buttonpress_cb(GtkWidget *entry, GdkEventButton *event, DictData *dd)
{
	GtkWidget *toplevel;

	/* Determine toplevel parent widget */
	toplevel = gtk_widget_get_toplevel(entry);

	/* Grab entry focus if possible */
	if (event->button != 3 && toplevel && toplevel->window)
		xfce_panel_plugin_focus_widget(dd->plugin, entry);

	return FALSE;
}


static void dict_plugin_write_rc_file(XfcePanelPlugin *plugin, DictData *dd)
{
	dict_write_rc_file(dd);
}


static void dict_plugin_properties_dialog(XfcePanelPlugin *plugin, DictData *dd)
{
	xfce_panel_plugin_block_menu(plugin);

	dict_properties_dialog(dd);
}


/* the following extern declared functions are called in libdict when panel plugin is in use */
extern void dict_panel_plugin_save_settings(DictData *dd)
{
	if (dd->show_panel_entry &&
		xfce_panel_plugin_get_orientation(dd->plugin) == GTK_ORIENTATION_HORIZONTAL)
	{
		gtk_widget_show(dd->panel_entry);
	}
	else
		gtk_widget_hide(dd->panel_entry);

	dict_plugin_panel_set_size(dd->plugin, xfce_panel_plugin_get_size(dd->plugin), dd);
}


extern void dict_panel_plugin_unblock(DictData *dd)
{
	xfce_panel_plugin_unblock_menu(dd->plugin);
}


static void dict_construct(XfcePanelPlugin *plugin)
{
	DictData *dd = g_new0(DictData, 1);
	GtkWidget *hbox;

	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

	g_thread_init(NULL);

	dd->plugin = plugin;
	dd->searched_word = NULL;
	dd->query_is_running = FALSE;
	dd->query_status = NO_ERROR;

	dict_read_rc_file(dd);

	dd->panel_button = xfce_create_panel_button();

	dd->tooltips = gtk_tooltips_new();
	gtk_tooltips_set_tip(dd->tooltips, dd->panel_button, _("Look up a word"), NULL);

	dd->panel_button_image = gtk_image_new();
	gtk_container_add(GTK_CONTAINER(dd->panel_button), GTK_WIDGET(dd->panel_button_image));

	gtk_widget_show_all(dd->panel_button);

	g_signal_connect(dd->panel_button, "clicked", G_CALLBACK(dict_plugin_panel_button_clicked), dd);

	dict_create_main_window(dd);

	g_signal_connect(dd->window, "delete_event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
	g_signal_connect(dd->close_button, "clicked", G_CALLBACK(dict_plugin_close_button_clicked), dd);
	g_signal_connect(plugin, "free-data", G_CALLBACK(dict_plugin_free_data), dd);
	g_signal_connect(plugin, "size-changed", G_CALLBACK(dict_plugin_panel_set_size), dd);
	g_signal_connect(plugin, "orientation-changed", G_CALLBACK(dict_plugin_panel_change_orientation), dd);
	g_signal_connect(plugin, "style-set", G_CALLBACK(dict_plugin_style_set), dd);
	g_signal_connect(plugin, "save", G_CALLBACK(dict_plugin_write_rc_file), dd);
	g_signal_connect(plugin, "configure-plugin", G_CALLBACK(dict_plugin_properties_dialog), dd);
	g_signal_connect(plugin, "about", G_CALLBACK(dict_about_dialog), dd);

	xfce_panel_plugin_menu_show_configure(plugin);
	xfce_panel_plugin_menu_show_about(plugin);

	/* panel entry */
	dd->panel_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(dd->panel_entry), 15);
	g_signal_connect(dd->panel_entry, "activate", G_CALLBACK(dict_entry_activate_cb), dd);
	g_signal_connect(dd->panel_entry, "button-press-event", G_CALLBACK(dict_panel_entry_buttonpress_cb), dd);

	if (dd->show_panel_entry && xfce_panel_plugin_get_orientation(dd->plugin) == GTK_ORIENTATION_HORIZONTAL)
		gtk_widget_show(dd->panel_entry);

	hbox = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(hbox);

	gtk_container_add(GTK_CONTAINER(hbox), dd->panel_button);
	gtk_container_add(GTK_CONTAINER(hbox), dd->panel_entry);
	gtk_container_add(GTK_CONTAINER(plugin), hbox);

	xfce_panel_plugin_add_action_widget(plugin, dd->panel_button);
	dict_plugin_set_selection(dd);

	/* DnD stuff */
	gtk_drag_dest_set(GTK_WIDGET(dd->panel_button), GTK_DEST_DEFAULT_ALL,
		NULL, 0, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	gtk_drag_dest_add_text_targets(GTK_WIDGET(dd->panel_button));
	g_signal_connect(dd->panel_button, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);
	g_signal_connect(dd->main_entry, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);
	g_signal_connect(dd->panel_entry, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);

	dict_status_add(dd, _("Ready."));

	siginterrupt(SIGALRM, 1);
	signal(SIGALRM, dict_signal_cb);
}
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(dict_construct);
