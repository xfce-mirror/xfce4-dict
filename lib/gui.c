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


/* Creation of main window and helper functions (GUI stuff). */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxfcegui4/libxfcegui4.h>

#include "common.h"
#include "gui.h"
#include "inline-icon.h"



void dict_gui_set_panel_entry_text(DictData *dd, const gchar *text)
{
	if (dd->panel_entry != NULL)
		gtk_entry_set_text(GTK_ENTRY(dd->panel_entry), text);
}


void dict_gui_status_add(DictData *dd, const gchar *format, ...)
{
	static gchar string[512];
	va_list args;

	string[0] = ' ';
	va_start(args, format);
	g_vsnprintf(string + 1, (sizeof string) - 1, format, args);
	va_end(args);

	gtk_statusbar_pop(GTK_STATUSBAR(dd->statusbar), 1);
	gtk_statusbar_push(GTK_STATUSBAR(dd->statusbar), 1, string);
}


void dict_gui_clear_text_buffer(DictData *dd)
{
	GtkTextIter end_iter;

	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &dd->textiter);
	gtk_text_buffer_get_end_iter(dd->main_textbuffer, &end_iter);
	gtk_text_buffer_delete(dd->main_textbuffer, &dd->textiter, &end_iter);

	gtk_widget_grab_focus(dd->main_entry);
}


static void entry_activate_cb(GtkEntry *entry, DictData *dd)
{
	const gchar *entered_text = gtk_entry_get_text(GTK_ENTRY(dd->main_entry));

	dict_search_word(dd, entered_text);
}


static void entry_button_clicked_cb(GtkButton *button, DictData *dd)
{
	entry_activate_cb(NULL, dd);
	gtk_widget_grab_focus(dd->main_entry);
}


static void clear_button_clicked_cb(GtkButton *button, DictData *dd)
{
	dict_gui_clear_text_buffer(dd);

	/* clear the entries */
	gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
	dict_gui_set_panel_entry_text(dd, "");

	dict_gui_status_add(dd, _("Ready."));
}


static void search_mode_dict_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode_in_use = DICTMODE_DICT;
		gtk_widget_grab_focus(dd->main_entry);
	}
}


static void search_mode_web_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode_in_use = DICTMODE_WEB;
		gtk_widget_grab_focus(dd->main_entry);
	}
}


static void search_mode_spell_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode_in_use = DICTMODE_SPELL;
		gtk_widget_grab_focus(dd->main_entry);
	}
}


const guint8 *dict_gui_get_icon_data(void)
{
	return dict_icon_data;
}


static GtkWidget *create_file_menu(DictData *dd)
{
	GtkWidget *menubar, *file, *file_menu, *help, *help_menu, *menu_item;
	GtkAccelGroup *accel_group;

	accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(dd->window), accel_group);

	menubar = gtk_menu_bar_new();

	file = gtk_menu_item_new_with_mnemonic(_("_File"));

	file_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), file_menu);

	dd->pref_menu_item = gtk_image_menu_item_new_from_stock("gtk-preferences", accel_group);
	gtk_widget_add_accelerator(dd->pref_menu_item, "activate", accel_group,
			GDK_p, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(file_menu), dd->pref_menu_item);

	gtk_container_add(GTK_CONTAINER(file_menu), gtk_separator_menu_item_new());

	dd->close_menu_item = gtk_image_menu_item_new_from_stock(
			(dd->is_plugin) ? "gtk-close" : "gtk-quit", accel_group);
	gtk_container_add(GTK_CONTAINER(file_menu), dd->close_menu_item);

	help = gtk_menu_item_new_with_mnemonic(_("_Help"));

	help_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), help_menu);

	menu_item = gtk_image_menu_item_new_from_stock("gtk-about", accel_group);
	gtk_container_add(GTK_CONTAINER(help_menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(dict_gui_about_dialog), dd);

	gtk_container_add(GTK_CONTAINER(menubar), file);
	gtk_container_add(GTK_CONTAINER(menubar), help);

	gtk_widget_show_all(menubar);

	return menubar;
}


void dict_gui_create_main_window(DictData *dd)
{
	GtkWidget *main_box;
	GtkWidget *entry_box, *label_box, *entry_label, *entry_button, *clear_button;
	GtkWidget *sep, *align, *scrolledwindow_results;
	GdkPixbuf *icon;
	GtkWidget *method_chooser, *radio, *label;

	dd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(dd->window), _("Xfce4 Dictionary"));
	gtk_window_set_default_size(GTK_WINDOW(dd->window), 500, 300);

	icon = gdk_pixbuf_new_from_inline(-1, dict_icon_data, FALSE, NULL);
	gtk_window_set_icon(GTK_WINDOW(dd->window), icon);
	g_object_unref(icon);

	main_box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(main_box);
	gtk_container_add(GTK_CONTAINER(dd->window), main_box);

	gtk_box_pack_start(GTK_BOX(main_box), create_file_menu(dd), FALSE, TRUE, 0);

	/* entry box (label, entry, buttons) */
	entry_box = gtk_hbox_new(FALSE, 10);
	gtk_widget_show(entry_box);
	gtk_container_set_border_width(GTK_CONTAINER(entry_box), 2);
	gtk_box_pack_start(GTK_BOX(main_box), entry_box, FALSE, TRUE, 5);

	label_box = gtk_hbox_new(FALSE, 5);
	gtk_widget_show(label_box);
	gtk_box_pack_start(GTK_BOX(entry_box), label_box, TRUE, TRUE, 0);

	entry_label = gtk_label_new(_("Text to search:"));
	gtk_widget_show(entry_label);
	gtk_misc_set_alignment(GTK_MISC(entry_label), 1, 0.5);

	align = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 5, 0);
	gtk_widget_show(align);
	gtk_container_add(GTK_CONTAINER(align), entry_label);
	gtk_box_pack_start(GTK_BOX(label_box), align, FALSE, FALSE, 2);

	dd->main_entry = gtk_entry_new();
	gtk_widget_show(dd->main_entry);
	gtk_box_pack_start(GTK_BOX(label_box), dd->main_entry, TRUE, TRUE, 0);
	g_signal_connect(dd->main_entry, "activate", G_CALLBACK(entry_activate_cb), dd);

	entry_button = gtk_button_new_from_stock("gtk-find");
	gtk_widget_show(entry_button);
	gtk_box_pack_start(GTK_BOX(entry_box), entry_button, FALSE, FALSE, 0);
	g_signal_connect(entry_button, "clicked", G_CALLBACK(entry_button_clicked_cb), dd);

	clear_button = gtk_button_new_from_stock("gtk-clear");
	gtk_widget_show(clear_button);
	gtk_box_pack_start(GTK_BOX(entry_box), clear_button, FALSE, FALSE, 0);
	g_signal_connect(clear_button, "clicked", G_CALLBACK(clear_button_clicked_cb), dd);

	/* just make some space */
	align = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 10, 0);
	gtk_widget_show(align);
	gtk_container_add(GTK_CONTAINER(align), gtk_label_new(""));
	gtk_box_pack_start(GTK_BOX(entry_box), align, FALSE, FALSE, 0);

	dd->close_button = gtk_button_new_from_stock((dd->is_plugin) ? "gtk-close" : "gtk-quit");
	gtk_widget_show(dd->close_button);
	gtk_box_pack_end(GTK_BOX(entry_box), dd->close_button, FALSE, FALSE, 2);

	/* insert it here and it will(hopefully) be placed before the Close button */
	sep = gtk_vseparator_new();
	gtk_widget_show(sep);
	gtk_box_pack_end(GTK_BOX(entry_box), sep, FALSE, FALSE, 5);

	/* search method chooser */
	method_chooser = gtk_hbox_new(FALSE, 0);
	gtk_widget_show(method_chooser);
	gtk_box_pack_start(GTK_BOX(main_box), method_chooser, FALSE, FALSE, 0);

	label = gtk_label_new(_("Search with:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(method_chooser), label, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_label(NULL, _("Dict"));
	gtk_widget_show(radio);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode_in_use == DICTMODE_DICT));
	g_signal_connect(radio, "toggled", G_CALLBACK(search_mode_dict_toggled), dd);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), _("Web"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode_in_use == DICTMODE_WEB));
	g_signal_connect(radio, "toggled", G_CALLBACK(search_mode_web_toggled), dd);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), _("Spellcheck"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode_in_use == DICTMODE_SPELL));
	g_signal_connect(radio, "toggled", G_CALLBACK(search_mode_spell_toggled), dd);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	/* results area */
	scrolledwindow_results = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow_results);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_results), 4);
	gtk_box_pack_start(GTK_BOX(main_box), scrolledwindow_results, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow_results),
								GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_results),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	/* searched words textview stuff */
	dd->main_textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(dd->main_textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(dd->main_textview), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(dd->main_textview), 5);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(dd->main_textview), GTK_WRAP_WORD);
	dd->main_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(dd->main_textview));
	gtk_text_buffer_create_tag(dd->main_textbuffer,
			"bold",
			"weight", PANGO_WEIGHT_BOLD,
			"style", PANGO_STYLE_ITALIC,
			"pixels-below-lines", 3, NULL);
	gtk_text_buffer_create_tag(dd->main_textbuffer,
			"indent", "indent", 10, NULL);

	gtk_widget_show(dd->main_textview);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_results), dd->main_textview);

	/* status bar */
	dd->statusbar = gtk_statusbar_new();
	gtk_widget_show(dd->statusbar);
	gtk_box_pack_end(GTK_BOX(main_box), dd->statusbar, FALSE, FALSE, 0);

	/* DnD */
	g_signal_connect(dd->main_entry, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);
	/* TODO: find a way to get this working, the treeview doesn't receive anything as long as it isn't
	 * editable. scrolledwindow_results and a surrounding event box as receivers also don't work. */
	g_signal_connect(dd->main_textview, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);

	/* use the saved window geometry */
	if (dd->geometry[0] != -1)
	{
		gtk_window_move(GTK_WINDOW(dd->window), dd->geometry[0], dd->geometry[1]);
		gtk_window_set_default_size(GTK_WINDOW(dd->window), dd->geometry[2], dd->geometry[3]);
		if (dd->geometry[4] == 1)
			gtk_window_maximize(GTK_WINDOW(dd->window));
	}
}


void dict_gui_show_main_window(DictData *dd)
{
	gtk_widget_show(dd->window);
	gtk_window_deiconify(GTK_WINDOW(dd->window));
	gtk_window_present(GTK_WINDOW(dd->window));
}


void dict_gui_about_dialog(GtkWidget *widget, DictData *dd)
{
	GtkWidget *dialog;
	XfceAboutInfo *info;

	info = xfce_about_info_new("xfce4-dict", VERSION,
							   _("A client program to query different dictionaries."),
							   XFCE_COPYRIGHT_TEXT("2006-2008", "Enrico Tröger"),
							   XFCE_LICENSE_GPL);

	xfce_about_info_add_credit(info, "Enrico Tröger", "enrico(dot)troeger(at)uvena(dot)de", _("Developer"));
	xfce_about_info_set_homepage(info, "http://goodies.xfce.org/projects/applications/xfce4-dict");

	dialog = xfce_about_dialog_new_with_values(
		GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(widget))), info, dd->icon);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(gtk_widget_destroy), NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), "Xfce4 Dictionary");
	gtk_dialog_run(GTK_DIALOG(dialog));

	xfce_about_info_free(info);
}


void dict_gui_query_geometry(DictData *dd)
{
	gtk_window_get_position(GTK_WINDOW(dd->window),	&dd->geometry[0], &dd->geometry[1]);
	gtk_window_get_size(GTK_WINDOW(dd->window),	&dd->geometry[2], &dd->geometry[3]);

	if (gdk_window_get_state(dd->window->window) & GDK_WINDOW_STATE_MAXIMIZED)
		dd->geometry[4] = 1;
	else
		dd->geometry[4] = 0;
}
