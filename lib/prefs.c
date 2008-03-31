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


#include <string.h>
#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>

#include "common.h"
#include "prefs.h"
#include "dictd.h"



static void show_panel_entry_toggled(GtkToggleButton *tb, DictData *dd)
{
	gtk_widget_set_sensitive(dd->panel_entry_size_spinner, gtk_toggle_button_get_active(tb));
	gtk_widget_set_sensitive(dd->panel_entry_size_label, gtk_toggle_button_get_active(tb));
}


static void use_webserver_toggled(GtkToggleButton *tb, DictData *dd)
{
	gtk_widget_set_sensitive(dd->web_entry, gtk_toggle_button_get_active(tb));
	gtk_widget_set_sensitive(dd->web_entry_label, gtk_toggle_button_get_active(tb));
}


static void get_spell_dictionaries(DictData *dd)
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
				gtk_combo_box_append_text(GTK_COMBO_BOX(dd->spell_combo), item);
				if (strcmp(dd->spell_dictionary, item) == 0)
					gtk_combo_box_set_active(GTK_COMBO_BOX(dd->spell_combo), i);
			}
			g_strfreev(list);
		}

		g_free(cmd);
		g_free(locale_cmd);
		g_free(tmp);
	}
}


static void prefs_dialog_response(GtkWidget *dlg, gint response, DictData *dd)
{
	gchar *tmp;

	if (response == GTK_RESPONSE_HELP)
	{
		/* show help */
		gboolean result = dict_open_browser(dd, "exo-open --launch WebBrowser " PLUGIN_WEBSITE);

		if (G_UNLIKELY(result == FALSE))
			g_warning(_("Unable to open the following url: %s"), PLUGIN_WEBSITE);

		return;
	}

	/* MODE DICT */
	tmp = gtk_combo_box_get_active_text(GTK_COMBO_BOX(dd->dict_combo));
	if (tmp == NULL || tmp[0] == '0' || tmp[0] == '-')
	{
		xfce_err(_("You have chosen an invalid dictionary entry."));
		g_free(tmp);
		return;
	}

	dd->port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dd->port_spinner));

	g_free(dd->server);
	dd->server = g_strdup(gtk_entry_get_text(GTK_ENTRY(dd->server_entry)));

	g_free(dd->dictionary);
	dd->dictionary = tmp;

	/* MODE WEB */
	if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gereng)))
		dd->web_mode = WEBMODE_LEO_GERENG;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gerfre)))
		dd->web_mode = WEBMODE_LEO_GERFRE;
	else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gerspa)))
		dd->web_mode = WEBMODE_LEO_GERSPA;
	else
	{
		dd->web_mode = WEBMODE_OTHER;

		g_free(dd->web_url);
		dd->web_url = g_strdup(gtk_entry_get_text(GTK_ENTRY(dd->web_entry)));
	}

	/* MODE SPELL */
	tmp = gtk_combo_box_get_active_text(GTK_COMBO_BOX(dd->spell_combo));
	if (NZV(tmp))
	{
		g_free(dd->spell_dictionary);
		dd->spell_dictionary = tmp;
	}

	g_free(dd->spell_bin);
	dd->spell_bin = g_strdup(gtk_entry_get_text(GTK_ENTRY(dd->spell_entry)));

	/* general settings */
	dd->show_panel_entry = gtk_toggle_button_get_active(
							GTK_TOGGLE_BUTTON(dd->check_panel_entry));
	dd->panel_entry_size = gtk_spin_button_get_value_as_int(
							GTK_SPIN_BUTTON(dd->panel_entry_size_spinner));

	/* save settings */
	dict_write_rc_file(dd);

	gtk_widget_destroy(dlg);
}


GtkWidget *dict_prefs_dialog_show(GtkWidget *parent, DictData *dd)
{
	GtkWidget *dialog, *inner_vbox, *notebook, *notebook_vbox;
	GtkWidget *label1, *label2, *label3;

	dialog = xfce_titled_dialog_new_with_buttons(
		_("Xfce Dictionary"), GTK_WINDOW(parent),
		GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_NO_SEPARATOR,
		GTK_STOCK_HELP, GTK_RESPONSE_HELP,
		GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
		NULL);

    gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_name(GTK_WINDOW(dialog), "xfce4-settings");
	g_signal_connect_after(dialog, "response", G_CALLBACK(prefs_dialog_response), dd);

	notebook = gtk_notebook_new();
	gtk_widget_show(notebook);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), notebook, FALSE, TRUE, 0);
	gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);

	/*
	 * Page: general
	 */
	{
		GtkWidget *pe_hbox;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), notebook_vbox, gtk_label_new(_("General")));

		/* show panel entry check box */
		dd->check_panel_entry = gtk_check_button_new_with_label(
													_("Show text field in the panel"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->check_panel_entry), dd->show_panel_entry);
		gtk_widget_show(dd->check_panel_entry);
		g_signal_connect(G_OBJECT(dd->check_panel_entry), "toggled",
													G_CALLBACK(show_panel_entry_toggled), dd);

		/* panel entry size */
		dd->panel_entry_size_label = gtk_label_new_with_mnemonic(_("Text field size:"));
		gtk_widget_show(dd->panel_entry_size_label);
		dd->panel_entry_size_spinner = gtk_spin_button_new_with_range(0.0, 500.0, 1.0);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(dd->panel_entry_size_spinner),
																		dd->panel_entry_size);
		gtk_widget_show(dd->panel_entry_size_spinner);

		pe_hbox = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(pe_hbox);

		gtk_box_pack_start(GTK_BOX(pe_hbox), dd->panel_entry_size_label, FALSE, FALSE, 10);
		gtk_box_pack_start(GTK_BOX(pe_hbox), dd->panel_entry_size_spinner, TRUE, TRUE, 0);

		gtk_box_pack_start(GTK_BOX(inner_vbox), dd->check_panel_entry, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(inner_vbox), pe_hbox, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);

		/* init the sensitive widgets */
		show_panel_entry_toggled(GTK_TOGGLE_BUTTON(dd->check_panel_entry), dd);
	}

	/*
	 * Page: DICTD
	 */
	 {
		GtkWidget *table, *button_get_list;

		notebook_vbox = gtk_vbox_new(FALSE, 2);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), notebook_vbox, gtk_label_new(_("Dictd")));

		/* server address */
		label1 = gtk_label_new_with_mnemonic(_("Server:"));
		gtk_widget_show(label1);

		dd->server_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(dd->server_entry), 256);
		if (dd->server != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dd->server_entry), dd->server);
		}
		gtk_widget_show(dd->server_entry);

		/* server port */
		label2 = gtk_label_new_with_mnemonic(_("Server Port:"));
		gtk_widget_show(label2);

		dd->port_spinner = gtk_spin_button_new_with_range(0.0, 65536.0, 1.0);
		gtk_spin_button_set_value(GTK_SPIN_BUTTON(dd->port_spinner), dd->port);
		gtk_widget_show(dd->port_spinner);

		/* dictionary */
		label3 = gtk_label_new_with_mnemonic(_("Dictionary:"));
		gtk_widget_show(label3);

		dd->dict_combo = gtk_combo_box_new_text();
		gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo), _("* (use all)"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo),
											_("! (use all, stop after first match)"));
		gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo), "----------------");
		if (dd->dictionary != NULL)
		{
			if (dd->dictionary[0] == '*')
				gtk_combo_box_set_active(GTK_COMBO_BOX(dd->dict_combo), 0);
			else if (dd->dictionary[0] == '!')
				gtk_combo_box_set_active(GTK_COMBO_BOX(dd->dict_combo), 1);
			else
			{
				gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo), dd->dictionary);
				gtk_combo_box_set_active(GTK_COMBO_BOX(dd->dict_combo), 3);
			}
		}

		gtk_widget_show(dd->dict_combo);

		button_get_list = gtk_button_new_from_stock("gtk-find");
		gtk_widget_show(button_get_list);
		g_signal_connect(button_get_list, "clicked", G_CALLBACK(dict_dictd_get_list), dd);

		/* put it all together */
		table = gtk_table_new(3, 3, FALSE);
		gtk_widget_show(table);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);

		gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);
		gtk_misc_set_alignment(GTK_MISC(label1), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->server_entry, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label2), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->port_spinner, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label3, 0, 1, 2, 3,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label3), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->dict_combo, 1, 2, 2, 3,
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
	{
		GtkWidget  *entry_hbox;
		GSList *web_type;

		notebook_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), notebook_vbox, gtk_label_new(_("Web")));

		dd->web_radio_leo_gereng = gtk_radio_button_new_with_label(NULL,
													_("dict.leo.org - German <-> English"));
		web_type = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dd->web_radio_leo_gereng));
		if (dd->web_mode == WEBMODE_LEO_GERENG)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gereng), TRUE);
		gtk_widget_show(dd->web_radio_leo_gereng);
		gtk_box_pack_start(GTK_BOX(inner_vbox), dd->web_radio_leo_gereng, FALSE, FALSE, 0);

		dd->web_radio_leo_gerfre = gtk_radio_button_new_with_label(web_type,
													_("dict.leo.org - German <-> French"));
		web_type = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dd->web_radio_leo_gerfre));
		if (dd->web_mode == WEBMODE_LEO_GERFRE)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gerfre), TRUE);
		gtk_widget_show(dd->web_radio_leo_gerfre);
		gtk_box_pack_start(GTK_BOX(inner_vbox), dd->web_radio_leo_gerfre, FALSE, FALSE, 0);

		dd->web_radio_leo_gerspa = gtk_radio_button_new_with_label(web_type,
													_("dict.leo.org - German <-> Spanish"));
		web_type = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dd->web_radio_leo_gerspa));
		if (dd->web_mode == WEBMODE_LEO_GERSPA)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gerspa), TRUE);
		gtk_widget_show(dd->web_radio_leo_gerspa);
		gtk_box_pack_start(GTK_BOX(inner_vbox), dd->web_radio_leo_gerspa, FALSE, FALSE, 0);

		dd->web_radio_other = gtk_radio_button_new_with_label(web_type, _("Use another website"));
		if (dd->web_mode == WEBMODE_OTHER)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_other), TRUE);
		gtk_widget_show(dd->web_radio_other);
		g_signal_connect(G_OBJECT(dd->web_radio_other), "toggled",
												G_CALLBACK(use_webserver_toggled), dd);
		gtk_box_pack_start(GTK_BOX(inner_vbox), dd->web_radio_other, FALSE, FALSE, 0);

		dd->web_entry_label = gtk_label_new_with_mnemonic(_("URL:"));
		gtk_widget_show(dd->web_entry_label);
		dd->web_entry = gtk_entry_new();
		if (dd->web_url != NULL)
			gtk_entry_set_text(GTK_ENTRY(dd->web_entry), dd->web_url);
		gtk_widget_show(dd->web_entry);

		entry_hbox = gtk_hbox_new(FALSE, 0);
		gtk_widget_show(entry_hbox);
		gtk_box_pack_start(GTK_BOX(entry_hbox), dd->web_entry_label, FALSE, TRUE, 5);
		gtk_box_pack_start(GTK_BOX(entry_hbox), dd->web_entry, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(inner_vbox), entry_hbox, FALSE, FALSE, 0);

		label1 = gtk_label_new(_("Enter an URL to a web site which offer translation services.\nUse {word} as placeholder for the searched word."));
		gtk_label_set_line_wrap(GTK_LABEL(label1), TRUE);
		gtk_misc_set_alignment(GTK_MISC(label1), 0, 0);
		gtk_widget_show(label1);
		gtk_box_pack_start(GTK_BOX(inner_vbox), label1, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);

		/* init the sensitive widgets */
		use_webserver_toggled(GTK_TOGGLE_BUTTON(dd->web_radio_other), dd);
	}

	/*
	 * Page: ASPELL
	 */
	{
		GtkWidget *table;

		notebook_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(notebook_vbox);
		inner_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(inner_vbox);
		gtk_notebook_append_page(GTK_NOTEBOOK(notebook), notebook_vbox, gtk_label_new(_("Aspell")));

		label1 = gtk_label_new_with_mnemonic(_("Aspell program:"));
		gtk_widget_show(label1);

		dd->spell_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(dd->spell_entry), 256);
		if (dd->spell_bin != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dd->spell_entry), dd->spell_bin);
		}
		gtk_widget_show(dd->spell_entry);

		label2 = gtk_label_new_with_mnemonic(_("Dictionary:"));
		gtk_widget_show(label2);

		dd->spell_combo = gtk_combo_box_new_text();
		get_spell_dictionaries(dd);
		gtk_widget_show(dd->spell_combo);

		table = gtk_table_new(2, 2, FALSE);
		gtk_widget_show(table);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);

		gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);
		gtk_misc_set_alignment(GTK_MISC(label1), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->spell_entry, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label2), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->spell_combo, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_box_pack_start(GTK_BOX(inner_vbox), table, TRUE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(notebook_vbox), inner_vbox, TRUE, TRUE, 5);
	}

	return dialog;
}
