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


/* All files in lib/ form a collection of common used code by the xfce4-dict stand-alone
 * application and the xfce4-dict-plugin.
 * This file contains very common code and has some glue to combine the spell query code,
 * the dictd server query code and the web dictionary code together. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <gtk/gtk.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libxfce4util/libxfce4util.h>

#include "common.h"
#include "spell.h"
#include "dictd.h"
#include "gui.h"
#include "dbus.h"



/* TODO make me UTF-8 safe */
static gint str_pos(const gchar *haystack, const gchar *needle)
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
static gchar *str_replace(gchar *haystack, const gchar *needle, const gchar *replacement)
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
	lt_pos = str_pos(haystack, needle);

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

	g_free(haystack);
	result = g_string_free(str, FALSE);
	return str_replace(result, needle, replacement);
}


/* taken from xarchiver, thanks Giuseppe */
static gboolean open_browser(DictData *dd, const gchar *uri)
{
	gchar *argv[3];
	gchar *browser_path = NULL;
	gboolean result = FALSE;
	guint i = 0;
	const gchar *browsers[] = {
#if LIBXFCE4UI_CHECK_VERSION(4, 21, 0)
    "xdg-open", "xfce-open", "htmlview", "firefox", "mozilla",
#else
    "xdg-open", "exo-open", "htmlview", "firefox", "mozilla",
#endif
    "opera", "epiphany", "konqueror", "seamonkey", NULL };

	while (browsers[i] != NULL && (browser_path = g_find_program_in_path(browsers[i])) == NULL)
		i++;

	if (browser_path == NULL)
	{
		g_warning("No browser could be found in your path.");
		return FALSE;
	}

	argv[0] = browser_path;
	argv[1] = (gchar*) uri;
	argv[2] = NULL;

	result = g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_free(browser_path);

	return result;
}


gchar *dict_get_web_query_uri(DictData *dd, const gchar *word)
{
	gchar *uri;

#if GLIB_CHECK_VERSION(2, 16, 0)
	gchar *tmp = str_replace(g_strdup(dd->web_url), "{word}", dd->searched_word);
	uri = g_uri_escape_string(tmp,
			G_URI_RESERVED_CHARS_GENERIC_DELIMITERS G_URI_RESERVED_CHARS_SUBCOMPONENT_DELIMITERS,
			FALSE);
	if (uri != NULL)
		g_free(tmp);
	else
		uri = tmp;
#else
	uri = str_replace(g_strdup(dd->web_url), "{word}", dd->searched_word);
#endif

	return uri;
}


gboolean dict_start_web_query(DictData *dd, const gchar *word)
{
	gboolean success = TRUE;
	gchar *uri = dict_get_web_query_uri(dd, word);


	if (! NZV(uri))
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR,
			_("The search URL is empty. Please check your preferences."));
		success = FALSE;
	}
	else if (! open_browser(dd, uri))
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR,
			_("Browser could not be opened. Please check your preferences."));
		success = FALSE;
	}
	g_free(uri);

	return success;
}


dict_mode_t dict_set_search_mode_from_flags(dict_mode_t mode, gchar flags)
{
	if (flags & DICT_FLAGS_MODE_DICT)
		mode = DICTMODE_DICT;
	else if (flags & DICT_FLAGS_MODE_WEB)
		mode = DICTMODE_WEB;
	else if (flags & DICT_FLAGS_MODE_SPELL)
		mode = DICTMODE_SPELL;

	return mode;
}


void dict_search_word(DictData *dd, const gchar *word)
{
	gboolean browser_started = FALSE;

	/* sanity checks */
	if (! NZV(word))
	{
		/* just show the main window */
		dict_gui_show_main_window(dd);
		return;
	}

	g_free(dd->searched_word);
	if (! g_utf8_validate(word, -1, NULL))
	{	/* try to convert non-UTF8 input otherwise stop the query */
		dd->searched_word = g_locale_to_utf8(word, -1, NULL, NULL, NULL);
		if (dd->searched_word == NULL || ! g_utf8_validate(dd->searched_word, -1, NULL))
		{
			dict_gui_status_add(dd, _("Invalid non-UTF8 input"));
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
			dict_gui_set_panel_entry_text(dd, "");
			return;
		}
	}
	else
	{
		dd->searched_word = g_strdup(word);
	}

	/* Set the main entry text */
	gtk_entry_set_text(GTK_ENTRY(dd->main_entry), dd->searched_word);

	/* remove leading and trailing spaces */
	g_strstrip(dd->searched_word);
	gtk_combo_box_text_prepend_text(GTK_COMBO_BOX_TEXT(dd->main_combo), dd->searched_word);

	dict_gui_clear_text_buffer(dd);

	switch (dd->mode_in_use)
	{
		case DICTMODE_WEB:
		{
			browser_started = dict_start_web_query(dd, dd->searched_word);
			break;
		}
		case DICTMODE_SPELL:
		{
			dict_spell_start_query(dd, dd->searched_word, FALSE);
			break;
		}
		default:
		{
			dict_dictd_start_query(dd, dd->searched_word);
			break;
		}
	}
	/* If the browser was successfully started and we are not in the stand-alone app,
	 * then hide the main window in favour of the started browser.
	 * If we are in the stand-alone app, don't hide the main window, we don't want this */
	if (browser_started && dd->is_plugin)
	{
		gtk_widget_hide(dd->window);
	}
	else
	{
		dict_gui_show_main_window(dd);
	}
	/* clear the panel entry to not search again when you click on the panel button */
	dict_gui_set_panel_entry_text(dd, "");
}


static void parse_geometry(DictData *dd, const gchar *str)
{
	gint i;

	/* first init first element with -1 */
	dd->geometry[0] = -1;

	sscanf(str, "%d;%d;%d;%d;%d;",
		&dd->geometry[0], &dd->geometry[1], &dd->geometry[2], &dd->geometry[3], &dd->geometry[4]);

	/* don't use insane values but when main windows was maximized last time, pos might be
	 * negative at least on Windows for some reason */
	if (dd->geometry[4] != 1)
	{
		for (i = 0; i < 4; i++)
		{
			if (dd->geometry[i] < -1)
				dd->geometry[i] = -1;
		}
	}
}


static gchar *get_spell_program(void)
{
	gchar *path;

	path = g_find_program_in_path("enchant-2");
	if (path != NULL)
		return path;

	path = g_find_program_in_path("aspell");
	if (path != NULL)
		return path;

	return g_strdup("");
}


static gchar *get_default_lang(void)
{
	const gchar *lang = g_getenv("LANG");
	gchar *result = NULL;

	if (NZV(lang))
	{
		if (*lang == 'C' || *lang == 'c')
			lang = "en";
		else
		{	/* if we have something like de_DE.UTF-8, strip everything from the period to the end */
			gchar *period = strchr(lang, '.');
			if (period != NULL)
				result = g_strndup(lang, g_utf8_pointer_to_offset(lang, period));
		}
	}
	else
		lang = "en";

	return (result != NULL) ? result : g_strdup(lang);
}


void dict_read_rc_file(DictData *dd)
{
	XfceRc *rc;
	gint mode_in_use = DICTMODE_DICT;
	gint mode_default = DICTMODE_LAST_USED;
	gint panel_entry_size = 20;
	gint wpm = 400;
	gint grouping = 1;
	gboolean mark_paragraphs = FALSE;
	gboolean show_panel_entry = FALSE;
	gchar *spell_bin_default = get_spell_program();
	gchar *spell_dictionary_default = get_default_lang();
	const gchar *port = "2628";
	const gchar *server = "dict.org";
	const gchar *dict = "*";
	const gchar *weburl = NULL;
	const gchar *spell_bin = NULL;
	const gchar *spell_dictionary = NULL;
	const gchar *geo = "-1;0;0;0;0;";
	const gchar *link_color_str = "#0000ff";
	const gchar *phon_color_str = "#006300";
	const gchar *error_color_str = "#800000";
	const gchar *success_color_str = "#107000";
	const gchar *speedreader_font = "Sans 32";

	if ((rc = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, "xfce4-dict/xfce4-dict.rc", TRUE)) != NULL)
	{
		mode_in_use = xfce_rc_read_int_entry(rc, "mode_in_use", mode_in_use);
		mode_default = xfce_rc_read_int_entry(rc, "mode_default", mode_default);
		weburl = xfce_rc_read_entry(rc, "web_url", weburl);
		show_panel_entry = xfce_rc_read_bool_entry(rc, "show_panel_entry", show_panel_entry);
		panel_entry_size = xfce_rc_read_int_entry(rc, "panel_entry_size", panel_entry_size);
		port = xfce_rc_read_entry(rc, "port", port);
		server = xfce_rc_read_entry(rc, "server", server);
		dict = xfce_rc_read_entry(rc, "dict", dict);
		spell_bin = xfce_rc_read_entry(rc, "spell_bin", spell_bin_default);
		spell_dictionary = xfce_rc_read_entry(rc, "spell_dictionary", spell_dictionary_default);

		link_color_str = xfce_rc_read_entry(rc, "link_color", link_color_str);
		phon_color_str = xfce_rc_read_entry(rc, "phonetic_color", phon_color_str);
		error_color_str = xfce_rc_read_entry(rc, "error_color", error_color_str);
		success_color_str = xfce_rc_read_entry(rc, "success_color", success_color_str);

		speedreader_font = xfce_rc_read_entry(rc, "speedreader_font", speedreader_font);
		wpm = xfce_rc_read_int_entry(rc, "speedreader_wpm", wpm);
		grouping = xfce_rc_read_int_entry(rc, "speedreader_grouping", grouping);
		mark_paragraphs = xfce_rc_read_bool_entry(rc, "speedreader_mark_paragraphs", mark_paragraphs);

		geo = xfce_rc_read_entry(rc, "geometry", geo);
		parse_geometry(dd, geo);
	}

	dd->mode_default = mode_default;
	if (dd->mode_default == DICTMODE_LAST_USED)
		dd->mode_in_use = mode_in_use;
	else
		dd->mode_in_use = dd->mode_default;

	if (! NZV(weburl) && dd->mode_in_use == DICTMODE_WEB)
		dd->mode_in_use = DICTMODE_DICT;

	dd->web_url = g_strdup(weburl);
	dd->show_panel_entry = show_panel_entry;
	dd->panel_entry_size = panel_entry_size;
	dd->port = g_strdup(port);
	dd->server = g_strdup(server);
	dd->dictionary = g_strdup(dict);
	if (spell_bin != NULL)
	{
		dd->spell_bin = g_strdup(spell_bin);
		g_free(spell_bin_default);
	}
	else
		dd->spell_bin = spell_bin_default;
	if (spell_dictionary != NULL)
	{
		dd->spell_dictionary = g_strdup(spell_dictionary);
		g_free(spell_dictionary_default);
	}
	else
		dd->spell_dictionary = spell_dictionary_default;

	dd->color_link = g_new0(GdkRGBA, 1);
	gdk_rgba_parse(dd->color_link, link_color_str);
	dd->color_phonetic = g_new0(GdkRGBA, 1);
	gdk_rgba_parse(dd->color_phonetic, phon_color_str);
	dd->color_incorrect = g_new0(GdkRGBA, 1);
	gdk_rgba_parse(dd->color_incorrect, error_color_str);
	dd->color_correct = g_new0(GdkRGBA, 1);
	gdk_rgba_parse(dd->color_correct, success_color_str);

	dd->speedreader_mark_paragraphs = mark_paragraphs;
	dd->speedreader_wpm = wpm;
	dd->speedreader_grouping = grouping;
	dd->speedreader_font = g_strdup(speedreader_font);

	xfce_rc_close(rc);
}


void dict_write_rc_file(DictData *dd)
{
	XfceRc *rc;

	if ((rc = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, "xfce4-dict/xfce4-dict.rc", FALSE)) != NULL)
	{
		gchar *geometry_string;
		gchar *link_color_str, *phon_color_str, *error_color_str, *success_color_str;

		xfce_rc_write_int_entry(rc, "mode_in_use", dd->mode_in_use);
		xfce_rc_write_int_entry(rc, "mode_default", dd->mode_default);
		if (dd->web_url != NULL)
			xfce_rc_write_entry(rc, "web_url", dd->web_url);
		xfce_rc_write_bool_entry(rc, "show_panel_entry", dd->show_panel_entry);
		xfce_rc_write_int_entry(rc, "panel_entry_size", dd->panel_entry_size);
		xfce_rc_write_entry(rc, "port", dd->port);
		xfce_rc_write_entry(rc, "server", dd->server);
		xfce_rc_write_entry(rc, "dict", dd->dictionary);
		xfce_rc_write_entry(rc, "spell_bin", dd->spell_bin);
		xfce_rc_write_entry(rc, "spell_dictionary", dd->spell_dictionary);

		link_color_str = gdk_rgba_to_string(dd->color_link);
		phon_color_str = gdk_rgba_to_string(dd->color_phonetic);
		error_color_str = gdk_rgba_to_string(dd->color_incorrect);
		success_color_str = gdk_rgba_to_string(dd->color_correct);
		xfce_rc_write_entry(rc, "link_color", link_color_str);
		xfce_rc_write_entry(rc, "phonetic_color", phon_color_str);
		xfce_rc_write_entry(rc, "error_color", error_color_str);
		xfce_rc_write_entry(rc, "success_color", success_color_str);

		geometry_string = g_strdup_printf("%d;%d;%d;%d;%d;",
			dd->geometry[0], dd->geometry[1], dd->geometry[2], dd->geometry[3], dd->geometry[4]);
		xfce_rc_write_entry(rc, "geometry", geometry_string);

		xfce_rc_write_entry(rc, "speedreader_font", dd->speedreader_font);
		xfce_rc_write_int_entry(rc, "speedreader_wpm", dd->speedreader_wpm);
		xfce_rc_write_int_entry(rc, "speedreader_grouping", dd->speedreader_grouping);
		xfce_rc_write_bool_entry(rc, "speedreader_mark_paragraphs", dd->speedreader_mark_paragraphs);

		g_free(link_color_str);
		g_free(phon_color_str);
		g_free(error_color_str);
		g_free(success_color_str);
		g_free(geometry_string);

		xfce_rc_close(rc);
	}
}


void dict_free_data(DictData *dd)
{
	dict_write_rc_file(dd);

	dict_gui_finalize(dd);

	gtk_widget_destroy(dd->window);

	g_free(dd->searched_word);
	g_free(dd->dictionary);
	g_free(dd->server);
	g_free(dd->web_url);
	g_free(dd->spell_bin);
	g_free(dd->speedreader_font);

	g_free(dd->color_link);
	g_free(dd->color_phonetic);
	g_free(dd->color_correct);
	g_free(dd->color_incorrect);

	if (dd->icon != NULL)
		g_object_unref(dd->icon);

	g_free(dd);
}


void dict_drag_data_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y,
							 GtkSelectionData *data, guint info, guint ltime, DictData *dd)
{
	if ((data != NULL) && (gtk_selection_data_get_length(data) >= 0) && (gtk_selection_data_get_format(data) == 8))
	{
		dict_search_word(dd, (const gchar*) gtk_selection_data_get_data (data));
		gtk_drag_finish(context, TRUE, FALSE, ltime);
	}
}


DictData *dict_create_dictdata(void)
{
	DictData *dd = g_new0(DictData, 1);

	/* create a new DictData structure and fill relevant fields with NULL */

	dd->searched_word = NULL;
	dd->query_is_running = FALSE;
	dd->query_status = NO_ERROR;
	dd->panel_entry = NULL;

	return dd;
}


void dict_show_msgbox(DictData *dd, gint type, const gchar *text, ...)
{
	GtkWidget *dialog;
	GtkWindow *parent;
	GString *str;
	va_list args;
	const gchar *title;

	str = g_string_new(NULL);

	va_start(args, text);
	g_string_append_vprintf(str, text, args);
	va_end(args);

	switch (type)
	{
		case GTK_MESSAGE_ERROR:
			title = _("Error");
			break;
		case GTK_MESSAGE_WARNING:
			title = _("warning");
			break;
		default:
			title = "";
	}

	parent = (dd->window) ? GTK_WINDOW(dd->window) : NULL;
	dialog = gtk_message_dialog_new(parent, GTK_DIALOG_DESTROY_WITH_PARENT,
                                  type, GTK_BUTTONS_OK, "%s", str->str);
	gtk_window_set_title(GTK_WINDOW(dialog), title);

	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	g_string_free(str, TRUE);
}


gchar *dict_get_clipboard_contents(void)
{
	gchar *text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));

	if (! text)
		text = gtk_clipboard_wait_for_text(gtk_clipboard_get(GDK_SELECTION_PRIMARY));

	return text;
}


static gboolean on_handle_search(Dict *skeleton, GDBusMethodInvocation *invocation,
	const gchar *arg_phrase, gpointer user_data)
{
	dict_search_word((DictData *) user_data, arg_phrase);
	dict_complete_search(skeleton, invocation);
	return TRUE;
}


static void on_name_acquired(GDBusConnection *connection, const gchar *name,
	gpointer user_data)
{
	Dict *skeleton = dict_skeleton_new();
	g_signal_connect (skeleton, "handle-search", G_CALLBACK(on_handle_search), user_data);
	g_dbus_interface_skeleton_export (G_DBUS_INTERFACE_SKELETON (skeleton),
		connection, "/org/xfce/Dict", NULL);
}


void dict_acquire_dbus_name(DictData *dd)
{
	g_bus_own_name(G_BUS_TYPE_SESSION,
      "org.xfce.Dict",
      G_BUS_NAME_OWNER_FLAGS_NONE,
      NULL,
      on_name_acquired,
      NULL,
      dd,
      NULL);
}
