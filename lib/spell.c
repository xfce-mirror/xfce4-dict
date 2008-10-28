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


/* Code to execute the aspell or enchant (or ispell or anything command line compatible)
 * binary with a given search term and reads it output. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>

#include "common.h"
#include "spell.h"
#include "gui.h"


typedef struct
{
	DictData *dd;
	gchar *word;
} iodata;


static GIOChannel *set_up_io_channel(gint fd, GIOCondition cond, GIOFunc func, gpointer data)
{
	GIOChannel *ioc;

	ioc = g_io_channel_unix_new(fd);

	g_io_channel_set_flags(ioc, G_IO_FLAG_NONBLOCK, NULL);
	g_io_channel_set_encoding(ioc, NULL, NULL);
	/* "auto-close" */
	g_io_channel_set_close_on_unref(ioc, TRUE);

	g_io_add_watch(ioc, cond, func, (gpointer) data);
	g_io_channel_unref(ioc);

	return ioc;
}


static gboolean iofunc_read(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	iodata *iod = data;
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg, *tmp;
		DictData *dd = iod->dd;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg != NULL)
		{
			if (msg[0] == '&')
			{	/* & cmd 17 7: ... */
				gint count;
				tmp = strchr(msg + 2, ' ') + 1;
				count = atoi(tmp);
				dict_gui_status_add(dd, ngettext("%d suggestion found.",
												 "%d suggestions found.",
												 count), count);

				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
				tmp = g_strdup_printf(_("Suggestions for \"%s\":"), iod->word);
				gtk_text_buffer_insert_with_tags_by_name(
					dd->main_textbuffer, &dd->textiter, tmp, -1, "bold", NULL);
				g_free(tmp);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);

				tmp = strchr(msg, ':') + 2;
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, g_strchomp(tmp), -1);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n\n", 2);
			}
			else if (msg[0] == '*')
			{
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
				tmp = g_strdup_printf(_("\"%s\" is spelled correctly."), iod->word);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, tmp, -1);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n\n", 2);
				g_free(tmp);
			}
			else if (msg[0] == '#')
			{
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
				tmp = g_strdup_printf(_("No suggestions could be found for \"%s\"."),
					iod->word);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, tmp, -1);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n\n", 2);
				g_free(tmp);
			}
			g_free(msg);
		}
		return TRUE;
	}

	g_free(iod->word);
	g_free(iod);

	return FALSE;
}


static gboolean iofunc_read_err(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (cond & (G_IO_IN | G_IO_PRI))
	{
		gchar *msg;
		DictData *dd = data;

		while (g_io_channel_read_line(ioc, &msg, NULL, NULL, NULL) && msg != NULL)
		{
			/* translation hint:
			 * Error while executing <spell command, e.g. "aspell"> (<error message>) */
			dict_gui_status_add(dd, _("Error while executing \"%s\" (%s)."),
				dd->spell_bin, g_strstrip(msg));
			g_free(msg);
		}
		return TRUE;
	}

	return FALSE;
}


static gboolean iofunc_write(GIOChannel *ioc, GIOCondition cond, gpointer data)
{
	if (NZV((gchar *) data))
		g_io_channel_write_chars(ioc, (gchar *) data, -1, NULL, NULL);

	g_free(data);

	return FALSE;
}


void dict_spell_start_query(DictData *dd, const gchar *word)
{
	GError  *error = NULL;
	gchar  **argv;
	gchar	*locale_cmd;
	gint     stdout_fd;
	gint     stderr_fd;
	gint     stdin_fd;
	guint	 i;
	gsize	 tts_len;
	gchar  **tts; /* text to search */
	iodata	*iod;

	if (! NZV(dd->spell_bin))
	{
		dict_gui_status_add(dd, _("Please set the spell check command in the preferences dialog."));
		return;
	}

	if (! NZV(word))
	{
		dict_gui_status_add(dd, _("Invalid input."));
		return;
	}

	tts = g_strsplit_set(word, " -_,.", 0);
	tts_len = g_strv_length(tts);

	for (i = 0; i < tts_len; i++)
	{
		locale_cmd = g_locale_from_utf8(dd->spell_bin, -1, NULL, NULL, NULL);
		if (locale_cmd == NULL)
			locale_cmd = g_strdup(dd->spell_bin);

		argv = g_new0(gchar*, 5);
		argv[0] = locale_cmd;
		argv[1] = g_strdup("-a");
		argv[2] = g_strdup("-d");
		argv[3] = g_strdup(dd->spell_dictionary);
		argv[4] = NULL;

		if (g_spawn_async_with_pipes(NULL, argv, NULL,
				G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD, NULL, NULL, NULL,
				&stdin_fd, &stdout_fd, &stderr_fd, &error))
		{
			iod = g_new(iodata, 1);
			iod->dd = dd;
			iod->word = g_strdup(tts[i]);

			set_up_io_channel(stdin_fd, G_IO_OUT, iofunc_write, g_strdup(tts[i]));
			set_up_io_channel(stdout_fd, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, iofunc_read, iod);
			set_up_io_channel(stderr_fd, G_IO_IN|G_IO_PRI|G_IO_HUP|G_IO_ERR|G_IO_NVAL, iofunc_read_err, dd);
			dict_gui_status_add(dd, _("Ready."));
		}
		else
		{
			dict_gui_status_add(dd, _("Process failed (%s)"), error->message);
			g_error_free(error);
			error = NULL;
		}

		g_strfreev(argv);
	}

	g_strfreev(tts);
}


static gchar *get_enchant_dict_string(GPtrArray *dicts, const gchar *str)
{
	guint i;
	gchar *result = g_strstrip(g_strdup(str));
	gchar *e;

	/* dictionaries are in the form 'en_GB (aspell)', we search the first space and change
	 * it to the end of the string to ignore the rest (provider name) */
	e = strchr(result, ' ');
	if (e != NULL)
		*e = '\0';

	/* sometimes dictionaries are named lang-LOCALE instead of lang_LOCALE, so replace the
	 * hyphen by a dash, enchant seems to not care about it. */
	for (i = 0; i < strlen(result); i++)
	{
		if (result[i] == '-')
			result[i] = '_';
	}

	/* find duplicates and skip them */
	for (i = 0; i < dicts->len; i++)
	{
		if (strcmp(g_ptr_array_index(dicts, i), result) == 0)
		{
			g_free(result);
			return NULL;
		}
	}
	return result;
}


static gchar **get_enchant_dicts(const gchar *str)
{
	gchar **list = g_strsplit_set(str, "\r\n", -1);
	gchar *item;
	guint i, len = g_strv_length(list);
	GPtrArray *dicts = g_ptr_array_new();

	for (i = 0; i < len; i++)
	{
		item = get_enchant_dict_string(dicts, list[i]);
		if (item != NULL)
			g_ptr_array_add(dicts, item);
	}

	g_strfreev(list);

	list = g_new0(gchar *, dicts->len);
	for (i = 0; i < dicts->len; i++)
	{
		list[i] = g_ptr_array_index(dicts, i);
	}
	g_ptr_array_free(dicts, TRUE);

	return list;
}


static gchar **get_aspell_dicts(const gchar *str)
{
	gchar **list = g_strsplit_set(str, "\r\n", -1);
	guint i, len = g_strv_length(list);

	for (i = 0; i < len; i++)
	{
		g_strstrip(list[i]);
	}
	return list;
}


void dict_spell_get_dictionaries(DictData *dd, GtkWidget *spell_combo)
{
	const gchar *entry_cmd = gtk_entry_get_text(
		GTK_ENTRY(g_object_get_data(G_OBJECT(spell_combo), "spell_entry")));

	if (*entry_cmd != '\0')
	{
		gchar *tmp = NULL;
		gchar *cmd, *locale_cmd;
		gboolean use_enchant = FALSE;
		GtkTreeModel *model;
		GtkTreeIter iter;

		model = gtk_combo_box_get_model(GTK_COMBO_BOX(spell_combo));
		gtk_list_store_clear(GTK_LIST_STORE(model));

		if (strstr(entry_cmd, "enchant") != NULL)
		{
			cmd = g_strdup("enchant-lsmod -list-dicts");
			use_enchant = TRUE;
		}
		else
			cmd = g_strconcat(entry_cmd, " dump dicts", NULL);

		locale_cmd = g_locale_from_utf8(cmd, -1, NULL, NULL, NULL);
		if (locale_cmd == NULL)
			locale_cmd = g_strdup(cmd);

		g_spawn_command_line_sync(locale_cmd, &tmp, NULL, NULL, NULL);

		if (NZV(tmp))
		{
			gchar **list;
			guint i, len;

			list = (use_enchant) ? get_enchant_dicts(tmp) : get_aspell_dicts(tmp);
			len = g_strv_length(list);
			for (i = 0; i < len; i++)
			{
				if (NZV(list[i]))
				{
					gtk_list_store_append(GTK_LIST_STORE(model), &iter);
					gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, list[i], -1);
					if (strcmp(dd->spell_dictionary, list[i]) == 0)
						gtk_combo_box_set_active_iter(GTK_COMBO_BOX(spell_combo), &iter);
				}
			}
			g_strfreev(list);
		}

		g_free(cmd);
		g_free(locale_cmd);
		g_free(tmp);
	}
}
