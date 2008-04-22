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



#include "common.h"
#include "aspell.h"
#include "dictd.h"
#include "gui.h"



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

	result = str->str;
	g_free(haystack);
	g_string_free(str, FALSE);
	return str_replace(result, needle, replacement);
}


/* taken from xarchiver, thanks Giuseppe */
gboolean dict_open_browser(DictData *dd, const gchar *uri)
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

	result = gdk_spawn_on_screen(gtk_widget_get_screen(dd->window), NULL, argv, NULL,
				G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL);

	g_free (browser_path);

	return result;
}


static gboolean start_web_query(DictData *dd, const gchar *word)
{
	gboolean success = TRUE;
	gchar *uri;

	uri = str_replace(g_strdup(dd->web_url), "{word}", dd->searched_word);
	if (! dict_open_browser(dd, uri))
	{
		xfce_err(_("Browser could not be opened. Please check your preferences."));
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
		dict_gui_status_add(dd, _("Invalid input."));
		return;
	}

	g_free(dd->searched_word);
	if (! g_utf8_validate(word, -1, NULL))
	{	/* try to convert non-UTF8 input otherwise stop the query */
		dd->searched_word = g_locale_to_utf8(word, -1, NULL, NULL, NULL);
		if (dd->searched_word == NULL || ! g_utf8_validate(dd->searched_word, -1, NULL))
		{
			dict_gui_status_add(dd, _("Invalid non-UTF8 input."));
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
			dict_gui_set_panel_entry_text(dd, "");
			return;
		}
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), dd->searched_word);
		dict_gui_set_panel_entry_text(dd, dd->searched_word);
	}
	else
	{
		dd->searched_word = g_strdup(word);
	}

	dict_gui_clear_text_buffer(dd);

	switch (dd->mode_in_use)
	{
		case DICTMODE_WEB:
		{
			browser_started = start_web_query(dd, dd->searched_word);

			break;
		}
		case DICTMODE_SPELL:
		{
			dict_aspell_start_query(dd, dd->searched_word);
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


void dict_read_rc_file(DictData *dd)
{
	XfceRc *rc;
	gint mode_in_use = DICTMODE_DICT;
	gint mode_default = DICTMODE_LAST_USED;
	gint port = 2628;
	gint panel_entry_size = 120;
	gboolean show_panel_entry = FALSE;
	const gchar *server = "dict.org";
	const gchar *dict = "*";
	const gchar *weburl = NULL;
	const gchar *spell_bin = "aspell";
	const gchar *spell_dictionary = "en";
	const gchar *geo = "-1;0;0;0;0;";

	if ((rc = xfce_rc_config_open(XFCE_RESOURCE_CONFIG, "xfce4-dict/xfce4-dict.rc", TRUE)) != NULL)
	{
		mode_in_use = xfce_rc_read_int_entry(rc, "mode_in_use", mode_in_use);
		mode_default = xfce_rc_read_int_entry(rc, "mode_default", mode_default);
		weburl = xfce_rc_read_entry(rc, "web_url", weburl);
		show_panel_entry = xfce_rc_read_bool_entry(rc, "show_panel_entry", show_panel_entry);
		panel_entry_size = xfce_rc_read_int_entry(rc, "panel_entry_size", panel_entry_size);
		port = xfce_rc_read_int_entry(rc, "port", port);
		server = xfce_rc_read_entry(rc, "server", server);
		dict = xfce_rc_read_entry(rc, "dict", dict);
		spell_bin = xfce_rc_read_entry(rc, "spell_bin", spell_bin);
		spell_dictionary = xfce_rc_read_entry(rc, "spell_dictionary", spell_dictionary);

		geo = xfce_rc_read_entry(rc, "geometry", geo);
		parse_geometry(dd, geo);

		xfce_rc_close(rc);
	}

	dd->mode_default = mode_default;
	if (dd->mode_default == DICTMODE_LAST_USED)
		dd->mode_in_use = mode_in_use;
	else
		dd->mode_in_use = dd->mode_default;

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
		gchar geometry_string[128];

		xfce_rc_write_int_entry(rc, "mode_in_use", dd->mode_in_use);
		xfce_rc_write_int_entry(rc, "mode_default", dd->mode_default);
		if (dd->web_url != NULL)
			xfce_rc_write_entry(rc, "web_url", dd->web_url);
		xfce_rc_write_bool_entry(rc, "show_panel_entry", dd->show_panel_entry);
		xfce_rc_write_int_entry(rc, "panel_entry_size", dd->panel_entry_size);
		xfce_rc_write_int_entry(rc, "port", dd->port);
		xfce_rc_write_entry(rc, "server", dd->server);
		xfce_rc_write_entry(rc, "dict", dd->dictionary);
		xfce_rc_write_entry(rc, "spell_bin", dd->spell_bin);
		xfce_rc_write_entry(rc, "spell_dictionary", dd->spell_dictionary);

		g_snprintf(geometry_string, sizeof(geometry_string), "%d;%d;%d;%d;%d;",
			dd->geometry[0], dd->geometry[1], dd->geometry[2], dd->geometry[3], dd->geometry[4]);
		xfce_rc_write_entry(rc, "geometry", geometry_string);

		xfce_rc_close(rc);
	}
}


void dict_free_data(DictData *dd)
{
	dict_write_rc_file(dd);

	gtk_widget_destroy(dd->window);

	g_free(dd->searched_word);
	g_free(dd->dictionary);
	g_free(dd->server);
	g_free(dd->web_url);
	g_free(dd->spell_bin);

	if (dd->icon != NULL)
		g_object_unref(dd->icon);

	g_free(dd);
}


void dict_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
							 GtkSelectionData *data, guint info, guint ltime, DictData *dd)
{
	if ((data != NULL) && (data->length >= 0) && (data->format == 8))
	{
/*
		GtkWidget *source = gtk_drag_get_source_widget(drag_context);

		if (widget == dd->main_entry &&
			source != NULL &&
			gtk_widget_get_toplevel(source) == dd->window)
		{
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
		}
		else
*/		{
			dict_search_word(dd, (const gchar*) data->data);
		}

		drag_context->action = GDK_ACTION_COPY;
		gtk_drag_finish(drag_context, TRUE, FALSE, ltime);
	}
}


DictData *dict_create_dictdata()
{
	DictData *dd = g_new0(DictData, 1);

	/* create a new DictData structure and fill relevant fields with NULL */

	dd->searched_word = NULL;
	dd->query_is_running = FALSE;
	dd->query_status = NO_ERROR;
	dd->panel_entry = NULL;

	return dd;
}

