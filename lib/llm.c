/*  Copyright 2016-2025 André Miranda <andreldm(at)xfce(dot)org>
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


#include <sys/socket.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "common.h"
#include "llm.h"
#include "gui.h"


static gboolean dict_data_valid = TRUE;


static gpointer ask_server(DictData *dd)
{
	gint fd, bytes_received;
	gchar *prompt, *body, *request, **lines;
	gchar buffer[8192];
	GRegex *regex;
	GMatchInfo *match_info;

	dd->query_is_running = TRUE;

	if (! NZV(dd->searched_word))
	{
		dict_gui_status_add(dd, _("Invalid input"));
		goto exit;
	}

	if ((fd = open_socket(dd->llm_server, dd->llm_port)) == -1)
	{
		dict_gui_status_add(dd, _("Could not connect to server."));
		goto exit;
	}

	prompt = g_strdup_printf(dd->llm_prompt, dd->searched_word);
	body = g_strdup_printf("{\"model\":\"%s\",\"prompt\":\"%s\"}", dd->llm_model, prompt);
	request = g_strdup_printf(
		"POST /api/generate HTTP/1.1\r\n"
		"Host: localhost:11434\r\n"
		"Content-Type: application/json\r\n"
		"Content-Length: %zu\r\n"
		"Connection: close\r\n"
		"\r\n"
		"%s",
		strlen(body), body);
	g_free(prompt);
	g_free(body);

	send(fd, request, strlen(request), 0);
	g_free(request);

	regex = g_regex_new("response\":\"(.*)\",\"done\":false", 0, 0, NULL);

	while ((bytes_received = recv(fd, buffer, sizeof(buffer) - 1, 0)) > 0)
	{
		if (!dict_data_valid || !dd->query_is_running) break;

		buffer[bytes_received] = '\0';
		lines = g_strsplit(buffer, "\n", -1);
		for (int i = 0; lines[i] != NULL; ++i)
		{
			if (!dict_data_valid || !dd->query_is_running) break;

			if (g_regex_match(regex, lines[i], 0, &match_info))
			{
				gchar *response_text = g_match_info_fetch(match_info, 1);
				gchar *cleaned = g_strcompress(response_text);
				gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, cleaned, -1);
				g_free(cleaned);
				g_free(response_text);
			}
			g_match_info_free(match_info);
		}
		g_strfreev(lines);
	}

	g_regex_unref(regex);
	close(fd);
	if (dict_data_valid) dict_gui_status_add(dd, _("Ready"));

exit:
	if (dict_data_valid)
	{
		dd->query_is_running = FALSE;
		gtk_widget_hide(dd->cancel_query_button);
	}
	g_thread_exit(NULL);
	return NULL;
}


void dict_llm_start_query(DictData *dd, const gchar *word)
{
	if (!dd->query_is_running)
	{
		dict_gui_status_add(dd, _("Querying %s..."), dd->llm_server);
		gtk_widget_show(dd->cancel_query_button);

		/* start the thread to query the server */
		g_thread_unref(g_thread_new(NULL, (GThreadFunc) ask_server, dd));
	}
}


void dict_llm_invalidate_dict_data(void)
{
	dict_data_valid = FALSE;
}
