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


/* Preferences dialog and helper functions. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>

#include "common.h"
#include "prefs.h"
#include "dictd.h"
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
	NOTEBOOK_PAGE_ASPELL
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


static void get_spell_dictionaries(GtkWidget *spell_combo, DictData *dd)
{
	if (NZV(dd->spell_bin))
	{
		gchar *tmp = NULL, *cmd, *locale_cmd;

		cmd = g_strconcat(dd->spell_bin, " dump dicts", NULL);
		locale_cmd = g_locale_from_utf8(cmd, -1, NULL, NULL, NULL);
		if (locale_cmd == NULL)
			locale_cmd = g_strdup(cmd);
		g_spawn_command_line_sync(locale_cmd, &tmp, NULL, NULL, NULL);
		if (NZV(tmp))
		{
			gchar **list = g_strsplit_set(tmp, "\n\r", -1);
			gchar *item;
			guint i, len = g_strv_length(list);
			for (i = 0; i < len; i++)
			{
				item = g_strstrip(list[i]);
				gtk_combo_box_append_text(GTK_COMBO_BOX(spell_combo), item);
				if (strcmp(dd->spell_dictionary, item) == 0)
					gtk_combo_box_set_active(GTK_COMBO_BOX(spell_combo), i);
			}
			g_strfreev(list);
		}

		g_free(cmd);
		g_free(locale_cmd);
		g_free(tmp);
	}
}


void dict_prefs_dialog_response(GtkWidget *dlg, gint response, DictData *dd)
{
	gchar *dictionary, *search_url;

	/* check some values before actually saving the settings in case we need to return to
	 * the dialog */
	dictionary = gtk_combo_box_get_active_text(
		GTK_COMBO_BOX(g_object_get_data(G_OBJECT(dlg), "dict_combo")));
	if (! NZV(dictionary) || dictionary[0] == '-')
	{
		xfce_err(_("You have chosen an invalid dictionary."));
		g_free(dictionary);
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(g_object_get_data(G_OBJECT(dlg), "notebook")), NOTEBOOK_PAGE_DICTD);
		gtk_widget_grab_focus(GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "dict_combo")));
		return;
	}
	search_url = g_strdup(gtk_entry_get_text(
			GTK_ENTRY(g_object_get_data(G_OBJECT(dlg), "web_entry"))));
	if (! NZV(search_url) || search_url[0] == '-')
	{
		xfce_err(_("You must set a valid search URL."));
		g_free(search_url);
		gtk_notebook_set_current_page(
			GTK_NOTEBOOK(g_object_get_data(G_OBJECT(dlg), "notebook")), NOTEBOOK_PAGE_WEB);
		gtk_widget_grab_focus(GTK_WIDGET(g_object_get_data(G_OBJECT(dlg), "web_entry")));
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
	dd->web_url = search_url;

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
	GtkWidget *table, *button;

	table = gtk_table_new(4, 2, FALSE);
	gtk_table_set_row_spacings(GTK_TABLE(table), 2);
	gtk_table_set_col_spacings(GTK_TABLE(table), 2);

	for (i = 0; web_dicts[i].label != NULL; i++)
	{
		offset = i % 2;
		button = gtk_button_new_with_label(_(web_dicts[i].label));
		g_signal_connect(button, "clicked", G_CALLBACK(web_dict_button_clicked), web_dicts[i].url);
		g_object_set_data(G_OBJECT(button), "web_entry", entry);
		gtk_widget_show(button);

		gtk_table_attach(GTK_TABLE(table),
				button, offset, 1 + offset, i - offset, i + 1 - offset,
				(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
				(GtkAttachOptions) (0), 5, 5);
	}

	return table;
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


GtkWidget *dict_prefs_dialog_show(GtkWidget *parent, DictData *dd)
{
	GtkWidget *dialog, *inner_vbox, *notebook, *notebook_vbox;
	GtkWidget *label1, *label2, *label3;

	dialog = xfce_titled_dialog_new_with_buttons(
		_("Preferences"), GTK_WINDOW(parent),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
		NULL);

	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
	gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-settings");
	if (! dd->is_plugin) /* the response callback is run by the plugin's callback */
		g_signal_connect(dialog, "response", G_CALLBACK(dict_prefs_dialog_response), dd);

	notebook = gtk_notebook_new();
	GTK_WIDGET_UNSET_FLAGS(notebook, GTK_CAN_FOCUS);
	gtk_widget_show(notebook);
	g_object_set_data(G_OBJECT(dialog), "notebook", notebook);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);

	/*
	 * Page: general
	 */
#define PAGE_GENERAL /* only for navigation in Geany's symbol list ;-) */
	{
		GtkWidget *radio_button, *label;
		GSList *search_method;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("General")), NOTEBOOK_PAGE_GENERAL);

		label = gtk_label_new(_("<b>Default search method:</b>"));
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		radio_button = gtk_radio_button_new_with_label(NULL, _("Dict"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_DICT)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_DICT));
		g_signal_connect(G_OBJECT(radio_button), "toggled", G_CALLBACK(search_method_changed), dd);

		radio_button = gtk_radio_button_new_with_label(search_method, _("Web"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_WEB)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_WEB));
		g_signal_connect(G_OBJECT(radio_button), "toggled", G_CALLBACK(search_method_changed), dd);

		radio_button = gtk_radio_button_new_with_label(search_method, _("Spell Check"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_SPELL)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_SPELL));
		g_signal_connect(G_OBJECT(radio_button), "toggled", G_CALLBACK(search_method_changed), dd);

		radio_button = gtk_radio_button_new_with_label(search_method, _("Last used method"));
		search_method = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radio_button));
		if (dd->mode_default == DICTMODE_LAST_USED)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_button), TRUE);
		gtk_widget_show(radio_button);
		gtk_box_pack_start(GTK_BOX(inner_vbox), radio_button, FALSE, FALSE, 0);
		g_object_set_data(G_OBJECT(radio_button), "type", GINT_TO_POINTER(DICTMODE_LAST_USED));
		g_signal_connect(G_OBJECT(radio_button), "toggled", G_CALLBACK(search_method_changed), dd);

		/* show panel entry check box */
		if (dd->is_plugin)
		{
			GtkWidget *pe_hbox, *panel_entry_size_label, *panel_entry_size_spinner, *check_panel_entry;

			label = gtk_label_new(_("<b>Panel text field:</b>"));
			gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
			gtk_misc_set_alignment(GTK_MISC(label), 0, 1);
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

			pe_hbox = gtk_hbox_new(FALSE, 0);
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
		GtkWidget *table, *button_get_list, *button_get_info;
		GtkWidget *server_entry, *port_spinner, *dict_combo;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Dict")), NOTEBOOK_PAGE_DICTD);

		/* server address */
		label1 = gtk_label_new_with_mnemonic(_("Server:"));
		gtk_widget_show(label1);

		server_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(server_entry), 256);
		if (dd->server != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(server_entry), dd->server);
		}
		gtk_widget_show(server_entry);

		/* server port */
		label2 = gtk_label_new_with_mnemonic(_("Server Port:"));
		gtk_widget_show(label2);

		port_spinner = gtk_spin_button_new_with_range(0.0, 65536.0, 1.0);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(port_spinner), dd->port);
		gtk_widget_show(port_spinner);

		/* dictionary */
		label3 = gtk_label_new_with_mnemonic(_("Dictionary:"));
		gtk_widget_show(label3);

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

		gtk_widget_show(dict_combo);

		g_object_set_data(G_OBJECT(dialog), "server_entry", server_entry);
		g_object_set_data(G_OBJECT(dialog), "port_spinner", port_spinner);
		g_object_set_data(G_OBJECT(dialog), "dict_combo", dict_combo);

		button_get_list = gtk_button_new_from_stock("gtk-refresh");
		gtk_widget_show(button_get_list);
		g_signal_connect(button_get_list, "clicked", G_CALLBACK(dict_dictd_get_list), dd);
		g_object_set_data(G_OBJECT(button_get_list), "dict_combo", dict_combo);
		g_object_set_data(G_OBJECT(button_get_list), "port_spinner", port_spinner);
		g_object_set_data(G_OBJECT(button_get_list), "server_entry", server_entry);

		button_get_info = gtk_button_new_from_stock("gtk-info");
		gtk_widget_show(button_get_info);
		g_signal_connect(button_get_info, "clicked", G_CALLBACK(dict_dictd_get_information), dd);
		g_object_set_data(G_OBJECT(button_get_info), "port_spinner", port_spinner);
		g_object_set_data(G_OBJECT(button_get_info), "server_entry", server_entry);

		/* put it all together */
		table = gtk_table_new(3, 3, FALSE);
		gtk_widget_show(table);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);

		gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);
		gtk_misc_set_alignment(GTK_MISC(label1), 1, 0);

		gtk_table_attach(GTK_TABLE(table), server_entry, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), button_get_info, 2, 3, 0, 1,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label2), 1, 0);

		gtk_table_attach(GTK_TABLE(table), port_spinner, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label3, 0, 1, 2, 3,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label3), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dict_combo, 1, 2, 2, 3,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 0, 0);

		gtk_table_attach(GTK_TABLE(table), button_get_list, 2, 3, 2, 3,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);

		gtk_box_pack_start(GTK_BOX(inner_vbox), table, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	/*
	 * Page: WEB
	 */
#define PAGE_WEB /* only for navigation in Geany's symbol list ;-) */
	{
		GtkWidget *label, *web_entry_label, *web_entry, *web_entry_box, *web_dicts_table;

		notebook_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Web")), NOTEBOOK_PAGE_WEB);

		label = gtk_label_new(_("<b>Web search URL:</b>"));
		gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label, FALSE, FALSE, 0);

		web_entry_label = gtk_label_new_with_mnemonic(_("URL:"));
		gtk_widget_show(web_entry_label);
		web_entry = gtk_entry_new();
		if (dd->web_url != NULL)
			gtk_entry_set_text(GTK_ENTRY(web_entry), dd->web_url);
		gtk_widget_show(web_entry);

		web_entry_box = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(web_entry_box);

		web_dicts_table = create_web_dicts_table(web_entry);
		gtk_widget_show(web_dicts_table);
		gtk_box_pack_start(GTK_BOX(inner_vbox), web_dicts_table, FALSE, FALSE, 0);

		gtk_box_pack_start(GTK_BOX(web_entry_box), web_entry_label, FALSE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(web_entry_box), web_entry, TRUE, TRUE, 0);
		gtk_box_pack_start(GTK_BOX(inner_vbox), web_entry_box, FALSE, FALSE, 0);

		g_object_set_data(G_OBJECT(dialog), "web_entry", web_entry);

		label1 = wrap_label_new(_("Enter an URL to a web site which offer translation or dictionary services. Use {word} as placeholder for the searched word."));
		gtk_misc_set_alignment(GTK_MISC(label1), 0, 0);
		gtk_widget_show(label1);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	/*
	 * Page: ASPELL
	 */
#define PAGE_ASPELL /* only for navigation in Geany's symbol list ;-) */
	{
		GtkWidget *table, *spell_entry, *spell_combo;

		notebook_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_container_set_border_width(GTK_CONTAINER(inner_vbox), 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_insert_page(GTK_NOTEBOOK(notebook),
			notebook_vbox, gtk_label_new(_("Spell Check")), NOTEBOOK_PAGE_ASPELL);

		label1 = gtk_label_new_with_mnemonic(_("Aspell program:"));
		gtk_widget_show(label1);

		spell_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(spell_entry), 256);
		if (dd->spell_bin != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(spell_entry), dd->spell_bin);
		}
		gtk_widget_show(spell_entry);

		label2 = gtk_label_new_with_mnemonic(_("Dictionary:"));
		gtk_widget_show(label2);

		spell_combo = gtk_combo_box_new_text();
		get_spell_dictionaries(spell_combo, dd);
		gtk_widget_show(spell_combo);

		g_object_set_data(G_OBJECT(dialog), "spell_combo", spell_combo);
		g_object_set_data(G_OBJECT(dialog), "spell_entry", spell_entry);


		table = gtk_table_new(2, 2, FALSE);
		gtk_widget_show(table);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);

		gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);
		gtk_misc_set_alignment(GTK_MISC(label1), 1, 0);

		gtk_table_attach(GTK_TABLE(table), spell_entry, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label2), 1, 0);

		gtk_table_attach(GTK_TABLE(table), spell_combo, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_box_pack_start(GTK_BOX(inner_vbox), table, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	return dialog;
}
