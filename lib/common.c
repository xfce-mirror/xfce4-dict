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


/* All files in lib/ form a collection of common used code by the xfce4-dict stand-alone
 * application and the xfce4-dict-plugin.
 * This file contains very common code and has some glue to combine the aspell query code,
 * the dictd server query code and the web dictionary code together. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>



/* Simple forward declaration to avoid inclusion of libxfce4panel headers */
typedef struct XfcePanelPlugin_t XfcePanelPlugin;

#include "common.h"
#include "aspell.h"
#include "dictd.h"
#include "inline-icon.h"



/* the following extern declared functions are called here for interaction with the panel
 * plugin (if in use) and to avoid to require linkage to libxfce4panel */
extern void dict_panel_plugin_save_settings(DictData *dd);
extern void dict_panel_plugin_unblock(DictData *dd);



/* TODO make me UTF-8 safe */
static gint dict_str_pos(const gchar *haystack, const gchar *needle)
{
	gint haystack_length = strlen(haystack);
	gint needle_length = strlen(needle);
	gint i, j, pos = -1;

	if (needle_length > haystack_length)
	{
		return -1;
	}
	else
	{
		for (i = 0; (i < haystack_length) && pos == -1; i++)
		{
			if (haystack[i] == needle[0] && needle_length == 1)	return i;
			else if (haystack[i] == needle[0])
			{
				for (j = 1; (j < needle_length); j++)
				{
					if (haystack[i+j] == needle[j])
					{
						if (pos == -1) pos = i;
					}
					else
					{
						pos = -1;
						break;
					}
				}
			}
		}
		return pos;
	}
}


/* Replaces all occurrences of needle in haystack with replacement.
 * All strings have to be NULL-terminated and needle and replacement have to be different,
 * e.g. needle "%" and replacement "%%" causes an endless loop */
static gchar *dict_str_replace(gchar *haystack, const gchar *needle, const gchar *replacement)
{
	gint i;
	gchar *start;
	gint lt_pos;
	gchar *result;
	GString *str;

	if (haystack == NULL)
		return NULL;

	if (needle == NULL || replacement == NULL)
		return haystack;

	if (strcmp(needle, replacement) == 0)
		return haystack;

	start = strstr(haystack, needle);
	lt_pos = dict_str_pos(haystack, needle);

	if (start == NULL || lt_pos == -1)
		return haystack;

	/* substitute by copying */
	str = g_string_sized_new(strlen(haystack));
	for (i = 0; i < lt_pos; i++)
	{
		g_string_append_c(str, haystack[i]);
	}
	g_string_append(str, replacement);
	g_string_append(str, haystack + lt_pos + strlen(needle));

	result = str->str;
	g_free(haystack);
	g_string_free(str, FALSE);
	return dict_str_replace(result, needle, replacement);
}


void dict_set_panel_entry_text(DictData *dd, const gchar *text)
{
	if (dd->panel_entry != NULL)
		gtk_entry_set_text(GTK_ENTRY(dd->panel_entry), text);
}


void dict_show_main_window(DictData *dd)
{
	gtk_widget_show(dd->window);
	gtk_window_deiconify(GTK_WINDOW(dd->window));
	gtk_window_present(GTK_WINDOW(dd->window));
}


void dict_status_add(DictData *dd, const gchar *format, ...)
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


void dict_clear_text_buffer(DictData *dd)
{
	GtkTextIter start_iter, end_iter;

	/* clear the TextBuffer */
	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &start_iter);
	gtk_text_buffer_get_end_iter(dd->main_textbuffer, &end_iter);
	gtk_text_buffer_delete(dd->main_textbuffer, &start_iter, &end_iter);
}


/* taken from xarchiver, thanks Giuseppe */
static gboolean dict_open_browser(DictData *dd, const gchar *uri)
{
	gchar *argv[3];
	gchar *browser_path;
	gboolean result = FALSE;

	browser_path = g_find_program_in_path("exo-open");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("htmlview");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("firefox");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("mozilla");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("opera");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("epiphany");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("konqueror");
	if (browser_path == NULL)
		browser_path = g_find_program_in_path("seamonkey");

	if (browser_path == NULL) return FALSE;

	argv[0] = browser_path;
	argv[1] = (gchar*) uri;
	argv[2] = NULL;

	result = gdk_spawn_on_screen(gtk_widget_get_screen(GTK_WIDGET(dd->plugin)), NULL, argv, NULL,
				G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_free (browser_path);

	return result;
}


static gboolean dict_start_web_query(DictData *dd, const gchar *word)
{
	gboolean use_leo = FALSE;
	gboolean success = TRUE;
	gchar *uri, *base;

	switch (dd->web_mode)
	{
		case WEBMODE_LEO_GERENG:
		{
			base = "http://dict.leo.org/ende?search={word}";
			use_leo = TRUE;
			break;
		}
		case WEBMODE_LEO_GERFRE:
		{
			base = "http://dict.leo.org/frde?search={word}";
			use_leo = TRUE;
			break;
		}
		case WEBMODE_LEO_GERSPA:
		{
			base = "http://dict.leo.org/esde?search={word}";
			use_leo = TRUE;
			break;
		}
		default: base = dd->web_url;
	}

	if (use_leo)
	{
		/* convert the text into ISO-8869-15 because dict.leo.org expects it ;-( */
		gchar *tmp = g_convert(dd->searched_word, -1,
								"ISO-8859-15", "UTF-8", NULL, NULL, NULL);
		if (tmp != NULL)
		{
			g_free(dd->searched_word);
			dd->searched_word = tmp;
		}
	}
	uri = dict_str_replace(g_strdup(base), "{word}", dd->searched_word);
	if (! dict_open_browser(dd, uri))
	{
		xfce_err(_("Browser could not be opened. Please check your preferences."));
		success = FALSE;
	}
	g_free(uri);

	return success;
}


void dict_search_word(DictData *dd, const gchar *word)
{
	gboolean browser_started = FALSE;

	/* sanity checks */
	if (! NZV(word))
	{
		dict_status_add(dd, _("Invalid input."));
		return;
	}

	g_free(dd->searched_word);
	if (! g_utf8_validate(word, -1, NULL))
	{	/* try to convert non-UTF8 input otherwise stop the query */
		dd->searched_word = g_locale_to_utf8(word, -1, NULL, NULL, NULL);
		if (dd->searched_word == NULL || ! g_utf8_validate(dd->searched_word, -1, NULL))
		{
			dict_status_add(dd, _("Invalid non-UTF8 input."));
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
			dict_set_panel_entry_text(dd, "");
			return;
		}
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), dd->searched_word);
		dict_set_panel_entry_text(dd, dd->searched_word);
	}
	else
	{
		dd->searched_word = g_strdup(word);
	}

	switch (dd->mode)
	{
		case DICTMODE_DICT:
		{
			dict_start_server_query(dd, dd->searched_word);
			break;
		}
		case DICTMODE_WEB:
		{
			browser_started = dict_start_web_query(dd, dd->searched_word);
			break;
		}
		case DICTMODE_SPELL:
		{
			/* TODO search only for the first word when working on a sentence,
			 * workout a better solution */
			gchar *first_word_end = dd->searched_word;
			if ((first_word_end = strchr(dd->searched_word, ' ')) ||
				(first_word_end = strchr(dd->searched_word, '-')) ||
				(first_word_end = strchr(dd->searched_word, '_')) ||
				(first_word_end = strchr(dd->searched_word, '.')) ||
				(first_word_end = strchr(dd->searched_word, ',')))
			{
				*first_word_end = '\0';
			}

			dict_start_aspell_query(dd, dd->searched_word);
			break;
		}
	}

	/* If we started a web search, the browser was successfully started and we are not in the
	 * stand-alone app, then hide the main window in favour of the started browser.
	 * If we are in the stand-alone app, don't hide the main window, we don't want this */
	if (browser_started && dd->plugin != NULL)
	{
		gtk_widget_hide(dd->window);
	}
	else
	{
		dict_show_main_window(dd);
	}
}


void dict_read_rc_file(DictData *dd)
{
	XfceRc *rc;
	gint mode = DICTMODE_DICT;
	gint webmode = WEBMODE_LEO_GERENG;
	gint port = 2628;
	gint panel_entry_size = 120;
	gboolean show_panel_entry = FALSE;
	const gchar *server = "dict.org";
	const gchar *dict = "*";
	const gchar *weburl = NULL;
	const gchar *spell_bin = "aspell";
	const gchar *spell_dictionary = "en";

	if ((rc = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, "xfce4-dict/xfce4-dict.rc", TRUE)) != NULL)
	{
		mode = xfce_rc_read_int_entry(rc, "mode", mode);
		webmode = xfce_rc_read_int_entry(rc, "web_mode", webmode);
		weburl = xfce_rc_read_entry(rc, "web_url", weburl);
		show_panel_entry = xfce_rc_read_bool_entry(rc, "show_panel_entry", show_panel_entry);
		panel_entry_size = xfce_rc_read_int_entry(rc, "panel_entry_size", panel_entry_size);
		port = xfce_rc_read_int_entry(rc, "port", port);
		server = xfce_rc_read_entry(rc, "server", server);
		dict = xfce_rc_read_entry(rc, "dict", dict);
		spell_bin = xfce_rc_read_entry(rc, "spell_bin", spell_bin);
		spell_dictionary = xfce_rc_read_entry(rc, "spell_dictionary", spell_dictionary);

		xfce_rc_close(rc);
	}

	dd->mode = mode;
	dd->web_mode = webmode;
	dd->web_url = g_strdup(weburl);
	dd->show_panel_entry = show_panel_entry;
	dd->panel_entry_size = panel_entry_size;
	dd->port = port;
	dd->server = g_strdup(server);
	dd->dictionary = g_strdup(dict);
	dd->spell_bin = g_strdup(spell_bin);
	dd->spell_dictionary = g_strdup(spell_dictionary);
}


void dict_write_rc_file(DictData *dd)
{
	XfceRc *rc;

	if ((rc = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, "xfce4-dict/xfce4-dict.rc", FALSE)) != NULL)
	{
		xfce_rc_write_int_entry(rc, "mode", dd->mode);
		xfce_rc_write_int_entry(rc, "web_mode", dd->web_mode);
		if (dd->web_url != NULL)
			xfce_rc_write_entry(rc, "web_url", dd->web_url);
		xfce_rc_write_bool_entry(rc, "show_panel_entry", dd->show_panel_entry);
		xfce_rc_write_int_entry(rc, "panel_entry_size", dd->panel_entry_size);
		xfce_rc_write_int_entry(rc, "port", dd->port);
		xfce_rc_write_entry(rc, "server", dd->server);
		xfce_rc_write_entry(rc, "dict", dd->dictionary);
		xfce_rc_write_entry(rc, "spell_bin", dd->spell_bin);
		xfce_rc_write_entry(rc, "spell_dictionary", dd->spell_dictionary);

		xfce_rc_close(rc);
	}
}


void dict_free_data(DictData *dd)
{
	if (dd->plugin != NULL)
	{
		/* Destroy the setting dialog, if this open */
		GtkWidget *dialog = g_object_get_data(G_OBJECT(dd->plugin), "dialog");
		if (dialog)
			gtk_widget_destroy(dialog);
	}

    gtk_widget_destroy(dd->window);

	dict_write_rc_file(dd);
	g_free(dd->searched_word);
	g_free(dd->dictionary);
	g_free(dd->server);
	g_free(dd->web_url);
	g_free(dd->spell_bin);

	if (dd->icon != NULL)
		g_object_unref(dd->icon);
	if (dd->tooltips != NULL)
		gtk_object_sink(GTK_OBJECT(dd->tooltips));

	g_free(dd);
}




static void dict_properties_dialog_response(GtkWidget *dlg, gint response, DictData *dd)
{
	g_object_set_data(G_OBJECT(dd->plugin), "dialog", NULL);

	if (response == GTK_RESPONSE_OK)
	{
		gchar *tmp;
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

		if (dd->plugin != NULL)
			dict_panel_plugin_save_settings(dd);

		/* save settings */
		dict_write_rc_file(dd);
	}
	gtk_widget_destroy(dlg);

	if (dd->plugin != NULL)
		dict_panel_plugin_unblock(dd);
}


static void dict_show_panel_entry_toggled(GtkToggleButton *tb, DictData *dd)
{
	gtk_widget_set_sensitive(dd->panel_entry_size_spinner, gtk_toggle_button_get_active(tb));
	gtk_widget_set_sensitive(dd->panel_entry_size_label, gtk_toggle_button_get_active(tb));
}


static void dict_use_webserver_toggled(GtkToggleButton *tb, DictData *dd)
{
	gtk_widget_set_sensitive(dd->web_entry, gtk_toggle_button_get_active(tb));
	gtk_widget_set_sensitive(dd->web_entry_label, gtk_toggle_button_get_active(tb));
}


void dict_get_spell_dictionaries(DictData *dd)
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


void dict_properties_dialog(DictData *dd)
{
	GtkWidget *dlg, *header, *vbox, *label3, *parent;

	parent = (dd->plugin != NULL) ? GTK_WIDGET(dd->plugin) : dd->window;

	dlg = gtk_dialog_new_with_buttons(_("Properties"),
				GTK_WINDOW(gtk_widget_get_toplevel(parent)),
				GTK_DIALOG_DESTROY_WITH_PARENT |
				GTK_DIALOG_NO_SEPARATOR,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
				NULL);

	if (dd->plugin != NULL)
		g_object_set_data(G_OBJECT(dd->plugin), "dialog", dlg);

	gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);

	g_signal_connect(dlg, "response", G_CALLBACK(dict_properties_dialog_response), dd);

	gtk_container_set_border_width(GTK_CONTAINER(dlg), 2);

	header = xfce_create_header(NULL, _("Dictionary plugin"));
	gtk_container_set_border_width(GTK_CONTAINER(header), 6);
	gtk_widget_show(header);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG (dlg)->vbox), header, FALSE, TRUE, 0);

	vbox = gtk_vbox_new(FALSE, 3);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
	gtk_widget_show(vbox);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), vbox, TRUE, TRUE, 0);

	/*
	 * Mode: DICT
	 */
	 {
		GtkWidget *label1, *label2, *table, *button_get_list, *frame1;

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
		g_signal_connect(button_get_list, "clicked", G_CALLBACK(dict_get_dict_list_cb), dd);


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
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label2), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->port_spinner, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label3, 0, 1, 2, 3,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label3), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->dict_combo, 1, 2, 2, 3,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 0, 0);

		gtk_table_attach(GTK_TABLE(table), button_get_list, 2, 3, 2, 3,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);


		frame1 = gtk_frame_new(_("Use a DICT server"));
		gtk_frame_set_shadow_type(GTK_FRAME(frame1), GTK_SHADOW_ETCHED_OUT);
		gtk_widget_show(frame1);
		gtk_container_set_border_width(GTK_CONTAINER(frame1), 3);
		gtk_container_add(GTK_CONTAINER(frame1), table);
		gtk_box_pack_start(GTK_BOX(vbox), frame1, FALSE, FALSE, 0);
	}

	/*
	 * Mode: WEB
	 */
	{
		GtkWidget  *web_vbox, *entry_hbox, *help_label, *frame2;
		GSList *web_type;

		web_vbox = gtk_vbox_new(FALSE, 5);
		gtk_widget_show(web_vbox);

		dd->web_radio_leo_gereng = gtk_radio_button_new_with_label(NULL,
													_("dict.leo.org - German <-> English"));
		web_type = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dd->web_radio_leo_gereng));
		if (dd->web_mode == WEBMODE_LEO_GERENG)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gereng), TRUE);
		gtk_widget_show(dd->web_radio_leo_gereng);
		gtk_box_pack_start(GTK_BOX(web_vbox), dd->web_radio_leo_gereng, FALSE, FALSE, 0);

		dd->web_radio_leo_gerfre = gtk_radio_button_new_with_label(web_type,
													_("dict.leo.org - German <-> French"));
		web_type = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dd->web_radio_leo_gerfre));
		if (dd->web_mode == WEBMODE_LEO_GERFRE)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gerfre), TRUE);
		gtk_widget_show(dd->web_radio_leo_gerfre);
		gtk_box_pack_start(GTK_BOX(web_vbox), dd->web_radio_leo_gerfre, FALSE, FALSE, 0);

		dd->web_radio_leo_gerspa = gtk_radio_button_new_with_label(web_type,
													_("dict.leo.org - German <-> Spanish"));
		web_type = gtk_radio_button_get_group(GTK_RADIO_BUTTON(dd->web_radio_leo_gerspa));
		if (dd->web_mode == WEBMODE_LEO_GERSPA)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_leo_gerspa), TRUE);
		gtk_widget_show(dd->web_radio_leo_gerspa);
		gtk_box_pack_start(GTK_BOX(web_vbox), dd->web_radio_leo_gerspa, FALSE, FALSE, 0);

		dd->web_radio_other = gtk_radio_button_new_with_label(web_type, _("Use another website"));
		if (dd->web_mode == WEBMODE_OTHER)
			gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->web_radio_other), TRUE);
		gtk_widget_show(dd->web_radio_other);
		g_signal_connect(G_OBJECT(dd->web_radio_other), "toggled",
												G_CALLBACK(dict_use_webserver_toggled), dd);
		gtk_box_pack_start(GTK_BOX(web_vbox), dd->web_radio_other, FALSE, FALSE, 0);

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
		gtk_box_pack_start(GTK_BOX(web_vbox), entry_hbox, FALSE, FALSE, 0);

		help_label = gtk_label_new(_("Enter an URL to a web site which offer translation services.\nUse {word} as placeholder for the searched word."));
		gtk_label_set_line_wrap(GTK_LABEL(help_label), TRUE);
		gtk_misc_set_alignment(GTK_MISC(help_label), 0, 0);
		gtk_widget_show(help_label);
		gtk_box_pack_start(GTK_BOX(web_vbox), help_label, TRUE, TRUE, 0);

		frame2 = gtk_frame_new(_("Use a web site"));
		gtk_frame_set_shadow_type(GTK_FRAME(frame2), GTK_SHADOW_ETCHED_OUT);
		gtk_widget_show(frame2);
		gtk_container_set_border_width(GTK_CONTAINER(frame2), 3);
		gtk_container_add(GTK_CONTAINER(frame2), web_vbox);
		gtk_box_pack_start(GTK_BOX(vbox), frame2, TRUE, TRUE, 0);
	}

	/*
	 * Mode: ASPELL
	 */
	{
		GtkWidget *label4, *label5, *table, *frame3;

		label4 = gtk_label_new_with_mnemonic(_("Aspell program:"));
		gtk_widget_show(label4);

		dd->spell_entry = gtk_entry_new();
		gtk_entry_set_max_length(GTK_ENTRY(dd->spell_entry), 256);
		if (dd->spell_bin != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dd->spell_entry), dd->spell_bin);
		}
		gtk_widget_show(dd->spell_entry);

		label5 = gtk_label_new_with_mnemonic(_("Dictionary:"));
		gtk_widget_show(label5);

		dd->spell_combo = gtk_combo_box_new_text();
		dict_get_spell_dictionaries(dd);
		gtk_widget_show(dd->spell_combo);

		table = gtk_table_new(2, 2, FALSE);
		gtk_widget_show(table);
		gtk_table_set_row_spacings(GTK_TABLE(table), 5);
		gtk_table_set_col_spacings(GTK_TABLE(table), 5);

		gtk_table_attach(GTK_TABLE(table), label4, 0, 1, 0, 1,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 5);
		gtk_misc_set_alignment(GTK_MISC(label4), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->spell_entry, 1, 2, 0, 1,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		gtk_table_attach(GTK_TABLE(table), label5, 0, 1, 1, 2,
						(GtkAttachOptions) (GTK_FILL),
						(GtkAttachOptions) (0), 5, 0);
		gtk_misc_set_alignment(GTK_MISC(label5), 1, 0);

		gtk_table_attach(GTK_TABLE(table), dd->spell_combo, 1, 2, 1, 2,
						(GtkAttachOptions) (GTK_FILL | GTK_EXPAND),
						(GtkAttachOptions) (0), 5, 5);

		frame3 = gtk_frame_new(_("Use Aspell"));
		gtk_frame_set_shadow_type(GTK_FRAME(frame3), GTK_SHADOW_ETCHED_OUT);
		gtk_widget_show(frame3);
		gtk_container_set_border_width(GTK_CONTAINER(frame3), 3);
		gtk_container_add(GTK_CONTAINER(frame3), table);
		gtk_box_pack_start(GTK_BOX(vbox), frame3, TRUE, FALSE, 0);
	}

	/* Display text entry in the panel */
	{
		GtkWidget *pe_hbox, *label;

		/* show panel entry check box */
		dd->check_panel_entry = gtk_check_button_new_with_label(
													_("Show text field in the panel"));
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->check_panel_entry), dd->show_panel_entry);
		gtk_tooltips_set_tip(dd->tooltips, dd->check_panel_entry,
			_("This option can only be used when the panel has a horizontal orientation."), NULL);
		gtk_widget_show(dd->check_panel_entry);
		g_signal_connect(G_OBJECT(dd->check_panel_entry), "toggled",
													G_CALLBACK(dict_show_panel_entry_toggled), dd);

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

		label = gtk_label_new(""); /* just to make some space, should be done better */
		gtk_widget_show(label);
		gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), dd->check_panel_entry, FALSE, FALSE, 0);
		gtk_box_pack_start(GTK_BOX(vbox), pe_hbox, FALSE, FALSE, 0);

	}

	/* init the sensitive widgets */
	dict_use_webserver_toggled(GTK_TOGGLE_BUTTON(dd->web_radio_other), dd);
	dict_show_panel_entry_toggled(GTK_TOGGLE_BUTTON(dd->check_panel_entry), dd);

	gtk_widget_show(dlg);
}


void dict_entry_activate_cb(GtkEntry *entry, DictData *dd)
{
	const gchar *entered_text;

	if (entry != NULL && GTK_WIDGET(entry) == dd->panel_entry)
	{
		entered_text = gtk_entry_get_text(GTK_ENTRY(dd->panel_entry));
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), entered_text);
	}
	else
		entered_text = gtk_entry_get_text(GTK_ENTRY(dd->main_entry));

	dict_search_word(dd, entered_text);
}


void dict_entry_button_clicked_cb(GtkButton *button, DictData *dd)
{
	dict_entry_activate_cb(NULL, dd);
	gtk_widget_grab_focus(dd->main_entry);
}


void dict_clear_button_clicked_cb(GtkButton *button, DictData *dd)
{
	dict_clear_text_buffer(dd);

	/* clear the entries */
	gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
	dict_set_panel_entry_text(dd, "");

	dict_status_add(dd, _("Ready."));
}


void dict_search_mode_dict_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode = DICTMODE_DICT;
		gtk_widget_grab_focus(dd->main_entry);
	}
}


void dict_search_mode_web_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode = DICTMODE_WEB;
		gtk_widget_grab_focus(dd->main_entry);
	}
}


void dict_search_mode_spell_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode = DICTMODE_SPELL;
		gtk_widget_grab_focus(dd->main_entry);
	}
}


void dict_create_main_window(DictData *dd)
{
	GtkWidget *main_box;
	GtkWidget *entry_box, *label_box, *entry_label, *entry_button, *clear_button;
	GtkWidget *sep, *align, *scrolledwindow_results;
	GdkPixbuf *icon;
	GtkWidget *method_chooser, *radio, *label;

	dd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(dd->window), "Xfce Dictionary");
	gtk_window_set_default_size(GTK_WINDOW(dd->window), 500, 300);

	icon = gdk_pixbuf_new_from_inline(-1, dict_icon_data, FALSE, NULL);
	gtk_window_set_icon(GTK_WINDOW(dd->window), icon);
	g_object_unref(icon);

	main_box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(main_box);
	gtk_container_add(GTK_CONTAINER(dd->window), main_box);

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
	g_signal_connect(dd->main_entry, "activate", G_CALLBACK(dict_entry_activate_cb), dd);

	entry_button = gtk_button_new_from_stock("gtk-find");
	gtk_widget_show(entry_button);
	gtk_box_pack_start(GTK_BOX(entry_box), entry_button, FALSE, FALSE, 0);
	g_signal_connect(entry_button, "clicked", G_CALLBACK(dict_entry_button_clicked_cb), dd);

	clear_button = gtk_button_new_from_stock("gtk-clear");
	gtk_widget_show(clear_button);
	gtk_box_pack_start(GTK_BOX(entry_box), clear_button, FALSE, FALSE, 0);
	g_signal_connect(clear_button, "clicked", G_CALLBACK(dict_clear_button_clicked_cb), dd);

	/* just make some space */
	align = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 10, 0);
	gtk_widget_show(align);
	gtk_container_add(GTK_CONTAINER(align), gtk_label_new(""));
	gtk_box_pack_start(GTK_BOX(entry_box), align, FALSE, FALSE, 0);

	dd->close_button = gtk_button_new_from_stock("gtk-close");
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
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode == DICTMODE_DICT));
	g_signal_connect(radio, "toggled", G_CALLBACK(dict_search_mode_dict_toggled), dd);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), _("Web"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode == DICTMODE_WEB));
	g_signal_connect(radio, "toggled", G_CALLBACK(dict_search_mode_web_toggled), dd);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(radio), _("Spellcheck"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode == DICTMODE_SPELL));
	g_signal_connect(radio, "toggled", G_CALLBACK(dict_search_mode_spell_toggled), dd);
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
	dd->main_boldtag = gtk_text_buffer_create_tag(dd->main_textbuffer,
			"bold", "weight", PANGO_WEIGHT_BOLD, NULL);

	gtk_widget_show(dd->main_textview);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_results), dd->main_textview);

	/* status bar */
	dd->statusbar = gtk_statusbar_new();
	gtk_widget_show(dd->statusbar);
	gtk_box_pack_end(GTK_BOX(main_box), dd->statusbar, FALSE, FALSE, 0);
}


const guint8 *dict_get_icon_data(void)
{
	return dict_icon_data;
}


void dict_about_dialog(GtkWidget *widget, DictData *dd)
{
	GtkWidget *dialog;
	XfceAboutInfo *info;

	info = xfce_about_info_new("xfce4-dict", VERSION,
							   _("A client program to query different dictionaries."),
							   XFCE_COPYRIGHT_TEXT("2006-2008", "Enrico Tröger"),
							   XFCE_LICENSE_GPL);

	xfce_about_info_add_credit(info, "Enrico Tröger", "enrico(dot)troeger(at)uvena(dot)de", _("Developer"));
	xfce_about_info_set_homepage(info, "http://goodies.xfce.org");

	dialog = xfce_about_dialog_new_with_values(GTK_WINDOW(widget), info, dd->icon);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(gtk_widget_destroy), NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), "Xfce Dictionary");
	gtk_dialog_run(GTK_DIALOG(dialog));

	xfce_about_info_free(info);
}


void dict_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
							 GtkSelectionData *data, guint info, guint ltime, DictData *dd)
{
	if ((data->length >= 0) && (data->format == 8))
	{
		drag_context->action = GDK_ACTION_COPY;

		if (widget == dd->main_entry)
		{
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
		}
		else
		{
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), (const gchar*) data->data);
			dict_search_word(dd, (const gchar*) data->data);
		}
		gtk_drag_finish(drag_context, TRUE, FALSE, ltime);
	}
}


void dict_signal_cb(gint sig)
{
	/* do nothing here and hope we never get called */
}


