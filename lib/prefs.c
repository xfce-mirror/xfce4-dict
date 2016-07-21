/*  Copyright 2006-2011 Enrico Tröger <enrico(at)xfce(dot)org>
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


/* Preferences dialog and helper functions. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

#include "common.h"
#include "prefs.h"
#include "dictd.h"
#include "spell.h"
#include "wraplabel.h"


typedef struct
{
	gchar *label;
	gchar *url;
} web_dict_t;

enum
{
	NOTEBOOK_PAGE_GENERAL = 0,
	NOTEBOOK_PAGE_DICTD,
	NOTEBOOK_PAGE_WEB,
	NOTEBOOK_PAGE_SPELL
};

static const web_dict_t web_dicts[] =
{
	{ N_("dict.leo.org - German <-> English"), "http://dict.leo.org/ende?search={word}" },
	{ N_("dict.leo.org - German <-> French"), "http://dict.leo.org/frde?search={word}" },
	{ N_("dict.leo.org - German <-> Spanish"), "http://dict.leo.org/esde?search={word}" },
	{ N_("dict.leo.org - German <-> Italian"), "http://dict.leo.org/itde?search={word}" },
	{ N_("dict.leo.org - German <-> Chinese"), "http://dict.leo.org/chde?search={word}" },
	{ N_("dist.cc - Dictionary"), "http://www.dict.cc/?s={word}" },
	{ N_("Dictionary.com"), "http://dictionary.reference.com/search?db=dictionary&q={word}" },
	{ N_("TheFreeDictionary.com"), "http://www.thefreedictionary.com/_/partner.aspx?Word={word}&Set=www&mode=w" },
	{ N_("Wikipedia, the free encyclopedia (EN)"), "http://en.wikipedia.org/wiki/{word}" },
	{ N_("Wiktionary, the free dictionary (EN)"), "http://en.wiktionary.org/wiki/{word}" },
	{ N_("Merriam-Webster Online Dictionary"), "http://www.merriam-webster.com/dictionary/{word}" },
	{ N_("Clear"), "" },
	{ NULL, NULL }
};

static void show_panel_entry_toggled(GtkToggleButton *tb, DictData *dd)
{
	if (dd->is_plugin)
	{
		gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(tb), "spinner"),
			gtk_toggle_button_get_active(tb));
		gtk_widget_set_sensitive(g_object_get_data(G_OBJECT(tb), "label"),
			gtk_toggle_button_get_active(tb));
	}
}


static void search_method_changed(GtkRadioButton *radiobutton, DictData *dd)
{
	if (! gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radiobutton)))
		return; /* ignore the toggled event when a button is deselected */

	dd->mode_default = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(radiobutton), "type"));
}


void dict_prefs_dialog_response(GtkWidget *dlg, gint response, DictData *dd)
{
	gchar *dictionary;

	/* check some values before actually saving the settings in case we need to return to
	 * the dialog */
	dictionary = gtk_combo_box_get_active_text(
		GTK_COMBO_BOX(g_object_get_data(G_OBJECT(dlg), "dict_combo")));
	if (! NZV(dictionary) || dictionary[0] == '-')
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("You have chosen an invalid dictionary."));
		g_free(dictionary);
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(g_object_get_data(G_OBJECT(dlg), "notebook")), NOTEBOOK_PAGE_DICTD);
		gtk_widget_grab_focus(GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "dict_combo")));
		return;
	}

	/* MODE DICT */
	dd->port = gtk_spin_button_get_value_as_int(
		GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dlg), "port_spinner")));

	g_free(dd->server);
	dd->server = g_strdup(gtk_entry_get_text(
		GTK_ENTRY(g_object_get_data(G_OBJECT(dlg), "server_entry"))));

	g_free(dd->dictionary);
	dd->dictionary = dictionary;

	/* MODE WEB */
	g_free(dd->web_url);
	dd->web_url = g_strdup(gtk_entry_get_text(
			GTK_ENTRY(g_object_get_data(G_OBJECT(dlg), "web_entry"))));
	gtk_widget_set_sensitive(dd->radio_button_web, NZV(dd->web_url));

	/* MODE SPELL */
	dictionary = gtk_combo_box_get_active_text(
			GTK_COMBO_BOX(g_object_get_data(G_OBJECT(dlg), "spell_combo")));
	if (NZV(dictionary))
	{
		g_free(dd->spell_dictionary);
		dd->spell_dictionary = dictionary;
	}

	g_free(dd->spell_bin);
	dd->spell_bin = g_strdup(gtk_entry_get_text(
			GTK_ENTRY(g_object_get_data(G_OBJECT(dlg), "spell_entry"))));

	/* general settings */
	if (dd->is_plugin)
	{
		dd->show_panel_entry = gtk_toggle_button_get_active(
					GTK_TOGGLE_BUTTON(g_object_get_data(G_OBJECT(dlg), "check_panel_entry")));
		dd->panel_entry_size = gtk_spin_button_get_value_as_int(
					GTK_SPIN_BUTTON(g_object_get_data(G_OBJECT(dlg), "panel_entry_size_spinner")));
	}
	g_object_set(G_OBJECT(dd->link_tag), "foreground-gdk", dd->color_link, NULL);
	g_object_set(G_OBJECT(dd->phon_tag), "foreground-gdk", dd->color_phonetic, NULL);
	g_object_set(G_OBJECT(dd->error_tag), "foreground-gdk", dd->color_incorrect, NULL);
	g_object_set(G_OBJECT(dd->success_tag), "foreground-gdk", dd->color_correct, NULL);

	/* save settings */
	dict_write_rc_file(dd);

	gtk_widget_destroy(dlg);
}


static void web_dict_button_clicked(GtkButton *button, gpointer user_data)
{
	const gchar *url = user_data;
	GtkEntry *entry = g_object_get_data(G_OBJECT(button), "web_entry");

	if (entry != NULL && url != NULL)
		gtk_entry_set_text(entry, url);
}


static GtkWidget *create_web_dicts_table(GtkWidget *entry)
{
	gint i;
	gint offset;
	GtkWidget *grid, *button;

	grid = gtk_grid_new();
	gtk_grid_set_row_spacing(GTK_GRID(grid), 4);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 4);

	for (i = 0; web_dicts[i].label != NULL; i++)
	{
		offset = i % 2;
		button = gtk_button_new_with_label(_(web_dicts[i].label));
		g_signal_connect(button, "clicked", G_CALLBACK(web_dict_button_clicked), web_dicts[i].url);
		g_object_set_data(G_OBJECT(button), "web_entry", entry);
		gtk_widget_show(button);

		gtk_grid_attach(GTK_GRID(grid), button, offset, i - offset, 1, 1);
		gtk_widget_set_hexpand(button, TRUE);
	}

	return grid;
}


const gchar *dict_prefs_get_web_url_label(DictData *dd)
{
	gint i;

	for (i = 0; web_dicts[i].label != NULL; i++)
	{
		if (strcmp(web_dicts[i].url, dd->web_url) == 0)
			return web_dicts[i].label;
	}
	return dd->web_url;
}


static void color_set_cb(GtkColorChooser *widget, GdkRGBA *color)
{
	gtk_color_chooser_get_rgba(widget, color);
}


static void button_dict_refresh_cb(GtkWidget *button, DictData *dd)
{
	GtkWidget *combo = g_object_get_data(G_OBJECT(button), "spell_combo");

	dict_spell_get_dictionaries(dd, combo);
}


static gboolean spell_entry_focus_cb(GtkEntry *entry, GdkEventFocus *ev, GtkWidget *icon)
{
	gchar *path = g_find_program_in_path(gtk_entry_get_text(entry));

	if (path != NULL)
	{
		gtk_image_set_from_icon_name(GTK_IMAGE(icon), "gtk-yes", GTK_ICON_SIZE_BUTTON);
		g_free(path);
	}
	else
		gtk_image_set_from_icon_name(GTK_IMAGE(icon), "gtk-stop", GTK_ICON_SIZE_BUTTON);

	return FALSE;
}


static void spell_entry_activate_cb(GtkEntry *entry, DictData *dd)
{
	GtkWidget *combo = g_object_get_data(G_OBJECT(entry), "spell_combo");
	GtkWidget *icon = g_object_get_data(G_OBJECT(entry), "icon");

	spell_entry_focus_cb(entry, NULL, icon);
	dict_spell_get_dictionaries(dd, combo);
}


static void spell_combo_changed_cb(GtkComboBox *widget, DictData *dd)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(widget, &iter))
	{
		gchar *text;
		gtk_tree_model_get(gtk_combo_box_get_model(widget), &iter, 0, &text, -1);
		g_free(dd->spell_dictionary);
		dd->spell_dictionary = text;
	}
}


GtkWidget *dict_prefs_dialog_show(GtkWidget *parent, DictData *dd)
{
	GtkWidget *dialog, *inner_vbox, *notebook, *notebook_vbox;
	GtkWidget *label1, *label2, *label3;

	dialog = xfce_titled_dialog_new_with_buttons(
		_("Dictionary"), GTK_WINDOW(parent),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		"gtk-close", GTK_RESPONSE_OK,
		NULL);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-dict");
	if (! dd->is_plugin) /* the response callback is run by the plugin's callback */
		g_signal_connect(dialog, "response", G_CALLBACK(dict_prefs_dialog_response), dd);

	notebook = gtk_notebook_new();
	GTK_WIDGET_UNSET_FLAGS(notebook, GTK_CAN_FOCUS);
	gtk_widget_show(notebook);
	g_object_set_data(G_OBJECT(dialog), "notebook", notebook);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), notebook, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);

	/*
	 * Page: general
	 */
#define PAGE_GENERAL /* only for navigation in Geany's symbol list ;-) */
	{
		GtkWidget *radio_button, *label, *grid, *label4;
		GtkWidget *color_link, *color_phon, *color_success, *color_error;
		GSList *search_method;

		notebook_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("General")), NOTEBOOK_PAGE_GENERAL);

		label = gtk_label_new(_("<b>Default search method:</b>"));
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		radio_button = gtk_radio_button_new_with_label(NULL, _("Dictionary Server"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_DICT)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_DICT));
		g_signal_connect(radio_button, "toggled", G_CALLBACK(search_method_changed), dd);

		radio_button = gtk_radio_button_new_with_label(search_method, _("Web Service"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_WEB)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_WEB));
		g_signal_connect(radio_button, "toggled", G_CALLBACK(search_method_changed), dd);

		radio_button = gtk_radio_button_new_with_label(search_method, _("Spell Checker"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_SPELL)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_SPELL));
		g_signal_connect(radio_button, "toggled", G_CALLBACK(search_method_changed), dd);

		radio_button = gtk_radio_button_new_with_label(search_method, _("Last used method"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_LAST_USED)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_LAST_USED));
		g_signal_connect(radio_button, "toggled", G_CALLBACK(search_method_changed), dd);

		label = gtk_label_new(_("<b>Colors:</b>"));
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_widget_set_valign(label, GTK_ALIGN_END);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 5);

		label1 = gtk_label_new(_("Links:"));
		label2 = gtk_label_new(_("Phonetics:"));
		label3 = gtk_label_new(_("Spelled correctly:"));
		label4 = gtk_label_new(_("Spelled incorrectly:"));
		color_link = gtk_color_button_new_with_rgba(dd->color_link);
		color_phon = gtk_color_button_new_with_rgba(dd->color_phonetic);
		color_error = gtk_color_button_new_with_rgba(dd->color_incorrect);
		color_success = gtk_color_button_new_with_rgba(dd->color_correct);
		g_signal_connect(color_link, "color-set", G_CALLBACK(color_set_cb), dd->color_link);
		g_signal_connect(color_phon, "color-set", G_CALLBACK(color_set_cb), dd->color_phonetic);
		g_signal_connect(color_error, "color-set", G_CALLBACK(color_set_cb), dd->color_incorrect);
		g_signal_connect(color_success, "color-set", G_CALLBACK(color_set_cb), dd->color_correct);

		grid = gtk_grid_new();
		gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
		gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

		gtk_grid_attach(GTK_GRID(grid), label1, 0, 0, 1, 1);
		gtk_widget_set_valign (label1, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label1, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), color_link, 1, 0, 1, 1);
		gtk_widget_set_hexpand(color_link, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label2, 0, 1, 1, 1);
		gtk_widget_set_valign (label2, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label2, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), color_phon, 1, 1, 1, 1);
		gtk_widget_set_hexpand(color_phon, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label3, 2, 0, 1, 1);
		gtk_widget_set_valign (label3, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label3, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), color_success, 3, 0, 1, 1);
		gtk_widget_set_hexpand(color_success, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label4, 2, 1, 1, 1);
		gtk_widget_set_valign (label4, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label4, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), color_error, 3, 1, 1, 1);
		gtk_widget_set_hexpand(color_error, TRUE);

		gtk_widget_show_all(grid);
		gtk_box_pack_start(GTK_BOX(inner_vbox), grid, FALSE, FALSE, 0);


		/* show panel entry check box */
		if (dd->is_plugin)
		{
			GtkWidget *pe_hbox, *panel_entry_size_label, *panel_entry_size_spinner, *check_panel_entry;

			label = gtk_label_new(_("<b>Panel Text Field:</b>"));
			gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
			gtk_widget_set_valign(label, GTK_ALIGN_END);
			gtk_widget_show(label);
			gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 5);

			check_panel_entry = gtk_check_button_new_with_label(_("Show text field in the panel"));
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check_panel_entry), dd->show_panel_entry);
			gtk_widget_show(check_panel_entry);
			g_signal_connect(G_OBJECT(check_panel_entry), "toggled",
														G_CALLBACK(show_panel_entry_toggled), dd);

			/* panel entry size */
			panel_entry_size_label = gtk_label_new_with_mnemonic(_("Text field size:"));
			gtk_widget_show(panel_entry_size_label);
			panel_entry_size_spinner = gtk_spin_button_new_with_range(0.0, 500.0, 1.0);
			gtk_spin_button_set_value(GTK_SPIN_BUTTON(panel_entry_size_spinner),
																			dd->panel_entry_size);

			g_object_set_data(G_OBJECT(dialog), "check_panel_entry", check_panel_entry);
			g_object_set_data(G_OBJECT(dialog), "panel_entry_size_spinner", panel_entry_size_spinner);
			g_object_set_data(G_OBJECT(check_panel_entry), "spinner", panel_entry_size_spinner);
			g_object_set_data(G_OBJECT(check_panel_entry), "label", panel_entry_size_label);

			gtk_widget_show(panel_entry_size_spinner);

			pe_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
			gtk_widget_show(pe_hbox);

			gtk_box_pack_start(GTK_BOX(pe_hbox), panel_entry_size_label, FALSE, FALSE, 10);
			gtk_box_pack_start(GTK_BOX(pe_hbox), panel_entry_size_spinner, TRUE, TRUE, 0);

			gtk_box_pack_start(GTK_BOX(inner_vbox), check_panel_entry, FALSE, FALSE, 0);
			gtk_box_pack_start(GTK_BOX(inner_vbox), pe_hbox, FALSE, FALSE, 0);

			/* init the sensitive widgets */
			show_panel_entry_toggled(GTK_TOGGLE_BUTTON(check_panel_entry), dd);
		}
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);

	}

	/*
	 * Page: DICTD
	 */
#define PAGE_DICTD /* only for navigation in Geany's symbol list ;-) */
	 {
		GtkWidget *grid, *button_get_list, *button_get_info;
		GtkWidget *server_entry, *port_spinner, *dict_combo;

		notebook_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Dictionary Server")), NOTEBOOK_PAGE_DICTD);

		/* server address */
		label1 = gtk_label_new_with_mnemonic(_("Server:"));

		server_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(server_entry), 256);
		if (dd->server != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(server_entry), dd->server);
		}

		/* server port */
		label2 = gtk_label_new_with_mnemonic(_("Server Port:"));

		port_spinner = gtk_spin_button_new_with_range(0.0, 65536.0, 1.0);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spinner), dd->port);

		/* dictionary */
		label3 = gtk_label_new_with_mnemonic(_("Dictionary:"));

		dict_combo = gtk_combo_box_new_text();
		gtk_combo_box_append_text(GTK_COMBO_BOX(dict_combo), _("* (use all)"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(dict_combo),
											_("! (use all, stop after first match)"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(dict_combo), "----------------");
		if (dd->dictionary != NULL)
		{
			if (dd->dictionary[0] == '*')
				gtk_combo_box_set_active(GTK_COMBO_BOX(dict_combo), 0);
			else if (dd->dictionary[0] == '!')
				gtk_combo_box_set_active(GTK_COMBO_BOX(dict_combo), 1);
			else
			{
				gtk_combo_box_append_text(GTK_COMBO_BOX(dict_combo), dd->dictionary);
				gtk_combo_box_set_active(GTK_COMBO_BOX(dict_combo), 3);
			}
		}


		g_object_set_data(G_OBJECT(dialog), "server_entry", server_entry);
		g_object_set_data(G_OBJECT(dialog), "port_spinner", port_spinner);
		g_object_set_data(G_OBJECT(dialog), "dict_combo", dict_combo);

		button_get_list = gtk_button_new_from_icon_name("gtk-refresh", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show(button_get_list);
		g_signal_connect(button_get_list, "clicked", G_CALLBACK(dict_dictd_get_list), dd);
		g_object_set_data(G_OBJECT(button_get_list), "dict_combo", dict_combo);
		g_object_set_data(G_OBJECT(button_get_list), "port_spinner", port_spinner);
		g_object_set_data(G_OBJECT(button_get_list), "server_entry", server_entry);

		button_get_info = gtk_button_new_from_icon_name("gtk-info", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show(button_get_info);
		g_signal_connect(button_get_info, "clicked", G_CALLBACK(dict_dictd_get_information), dd);
		g_object_set_data(G_OBJECT(button_get_info), "port_spinner", port_spinner);
		g_object_set_data(G_OBJECT(button_get_info), "server_entry", server_entry);

		/* put it all together */
		grid = gtk_grid_new();
		gtk_grid_set_row_spacing (GTK_GRID(grid), 8);
		gtk_grid_set_column_spacing (GTK_GRID(grid), 8);

		gtk_grid_attach(GTK_GRID(grid), label1, 0, 0, 1, 1);
		gtk_widget_set_valign (label1, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label1, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), server_entry, 1, 0, 1, 1);
		gtk_widget_set_hexpand(server_entry, TRUE);

		gtk_grid_attach(GTK_GRID(grid), button_get_info, 2, 0, 1, 1);
		gtk_widget_set_hexpand(button_get_info, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label2, 0, 1, 1, 1);
		gtk_widget_set_valign (label2, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label2, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), port_spinner, 1, 1, 1, 1);
		gtk_widget_set_hexpand(port_spinner, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label3, 0, 2, 1, 1);
		gtk_widget_set_valign (label3, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label3, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), dict_combo, 1, 2, 1, 1);
		gtk_widget_set_hexpand(dict_combo, TRUE);

		gtk_grid_attach(GTK_GRID(grid), button_get_list, 2, 2, 1, 1);

		gtk_widget_show_all(grid);
		gtk_box_pack_start(GTK_BOX(inner_vbox), grid, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	/*
	 * Page: WEB
	 */
#define PAGE_WEB /* only for navigation in Geany's symbol list ;-) */
	{
		GtkWidget *label, *web_entry_label, *web_entry, *web_entry_box, *web_dicts_table;

		notebook_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Web Service")), NOTEBOOK_PAGE_WEB);

		label = gtk_label_new(_("<b>Web search URL:</b>"));
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		web_entry_label = gtk_label_new_with_mnemonic(_("URL:"));
		gtk_widget_show(web_entry_label);
		web_entry = gtk_entry_new();
		if (dd->web_url != NULL)
			gtk_entry_set_text(GTK_ENTRY(web_entry), dd->web_url);
		gtk_widget_show(web_entry);

		web_entry_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
		gtk_widget_show(web_entry_box);

		web_dicts_table = create_web_dicts_table(web_entry);
		gtk_widget_show(web_dicts_table);
		gtk_box_pack_start(GTK_BOX(inner_vbox), web_dicts_table, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(web_entry_box), web_entry_label, FALSE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(web_entry_box), web_entry, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(inner_vbox), web_entry_box, FALSE, FALSE, 0);

		g_object_set_data(G_OBJECT(dialog), "web_entry", web_entry);

		label1 = xfd_wrap_label_new(_("Enter the URL of a web site which offers translation or dictionary services. Use {word} as placeholder for the searched word."));
		gtk_widget_show(label1);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	/*
	 * Page: SPELL
	 */
#define PAGE_SPELL /* only for navigation in Geany's symbol list ;-) */
	{
		GtkWidget *grid, *label_help, *spell_entry, *spell_combo, *button_refresh, *image, *icon;
		GtkListStore *store;
		GtkCellRenderer *renderer;

		notebook_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Spell Checker")), NOTEBOOK_PAGE_SPELL);

		label1 = gtk_label_new_with_mnemonic(_("Spell Check Program:"));
		gtk_widget_show(label1);

		icon = gtk_image_new();
		gtk_widget_show(icon);

		spell_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(spell_entry), 256);
		if (dd->spell_bin != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(spell_entry), dd->spell_bin);
		}
		g_signal_connect(spell_entry, "focus-out-event", G_CALLBACK(spell_entry_focus_cb), icon);
		g_signal_connect(spell_entry, "activate", G_CALLBACK(spell_entry_activate_cb), dd);
		gtk_widget_show(spell_entry);

		label_help = xfd_wrap_label_new(_(
	"<i>The spell check program can be 'enchant', 'aspell', 'ispell' or any other spell check "
	"program which is compatible to the ispell command.\nThe icon shows whether the entered "
	"command exists.</i>"));
		gtk_label_set_use_markup(GTK_LABEL(label_help), TRUE);
		gtk_widget_show(label_help);

		label2 = gtk_label_new_with_mnemonic(_("Dictionary:"));
		gtk_widget_show(label2);

		store = gtk_list_store_new(1, G_TYPE_STRING);
		spell_combo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(store));
		g_object_set_data(G_OBJECT(spell_combo), "spell_entry", spell_entry);

		renderer = gtk_cell_renderer_text_new();
		gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(spell_combo), renderer, TRUE);
		gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(spell_combo), renderer, "text", 0);

		dict_spell_get_dictionaries(dd, spell_combo);
		g_signal_connect(spell_combo, "changed", G_CALLBACK(spell_combo_changed_cb), dd);
		gtk_widget_show(spell_combo);

		button_refresh = gtk_button_new_from_icon_name("gtk-info", GTK_ICON_SIZE_BUTTON);
		gtk_widget_show(button_refresh);
		g_object_set_data(G_OBJECT(button_refresh), "spell_combo", spell_combo);
		g_signal_connect(button_refresh, "clicked", G_CALLBACK(button_dict_refresh_cb), dd);

		g_object_set_data(G_OBJECT(spell_entry), "icon", icon);
		g_object_set_data(G_OBJECT(spell_entry), "spell_combo", spell_combo);
		g_object_set_data(G_OBJECT(dialog), "spell_combo", spell_combo);
		g_object_set_data(G_OBJECT(dialog), "spell_entry", spell_entry);
		g_object_unref(store);

		spell_entry_focus_cb(GTK_ENTRY(spell_entry), NULL, icon); /* initially set the icon */

		grid = gtk_grid_new();
		gtk_widget_show(grid);
		gtk_grid_set_row_spacing(GTK_GRID(grid), 8);
		gtk_grid_set_column_spacing(GTK_GRID(grid), 8);

		gtk_grid_attach(GTK_GRID(grid), label_help, 0, 0, 3, 1);
		gtk_widget_set_hexpand(label_help, TRUE);

		gtk_grid_attach(GTK_GRID(grid), label1, 0, 1, 1, 1);
		gtk_widget_set_valign (label1, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label1, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), spell_entry, 1, 1, 1, 1);
		gtk_widget_set_hexpand(spell_entry, TRUE);

		gtk_grid_attach(GTK_GRID(grid), icon, 2, 1, 1, 1);

		gtk_grid_attach(GTK_GRID(grid), label2, 0, 2, 1, 1);
		gtk_widget_set_valign (label2, GTK_ALIGN_CENTER);
		gtk_widget_set_halign (label2, GTK_ALIGN_END);

		gtk_grid_attach(GTK_GRID(grid), spell_combo, 1, 2, 1, 1);
		gtk_widget_set_hexpand(spell_combo, TRUE);

		gtk_grid_attach(GTK_GRID(grid), button_refresh, 2, 2, 1, 1);

		gtk_box_pack_start(GTK_BOX(inner_vbox), grid, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	return dialog;
}
