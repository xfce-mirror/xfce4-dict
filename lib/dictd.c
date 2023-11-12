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


/* This file contains all networking related code to query a remote (or local)
 * dictd server (RFC 2229). */
/* TODO: This should be more separated from GUI code, sometime. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

#include <signal.h>
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
#include <stdlib.h>


#include "common.h"
#include "dictd.h"
#include "gui.h"
#include "spell.h"
#include "prefs.h"


#define BUF_SIZE 256



static gint open_socket(const gchar *host_name, const gchar *port)
{
	struct addrinfo hints, *res, *res0;
	gint fd = -1;
	gint opt = 1;

	memset(&hints, 0, sizeof (hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if (getaddrinfo(host_name, port, &hints, &res0))
		return (-1);

	for (res = res0; res; res = res->ai_next) {
		fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
		if (fd < 0)
			continue;

		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (gchar *) &opt, sizeof (opt));
		if (connect(fd, res->ai_addr, res->ai_addrlen) != 0) {
			close(fd);
			fd = -1;
			continue;
		}

		break;
	}

	freeaddrinfo(res0);
	return (fd);
}


static void send_command(gint fd, const gchar *str)
{
	gchar buf[256];
	gint len = strlen (str);

	g_snprintf(buf, 256, "%s\r\n", str);
	send(fd, buf, len + 2, 0);
}


static gchar *phon_find_start(gchar *buf, gchar **start_str, gchar **end_str)
{
	gchar *start;

	/* we check only once for the various (\, / and [...]) formats for phonetic information
	 * per line, for further occurrences on the same line we use the same format */
	if (**start_str == '\0')
	{
		start = strchr(buf, '\\');
		if (start != NULL)
		{
			*start_str = *end_str = "\\";
		}
		else
		{
			start = strchr(buf, '/');
			if (start != NULL)
			{
				*start_str = *end_str = "/";
			}
			else
			{
				start = strchr(buf, '[');
				if (start != NULL)
				{
					*start_str = "[";
					*end_str = "]";
				}
			}
		}
	}
	else
	{
		start = strchr(buf, **start_str);
		if (start != NULL)
		{
			*start_str = *end_str = *start_str;
		}
	}

	return start;
}


/* We parse the first line differently as there are usually no links
 * but instead phonetic information */
static void parse_header(DictData *dd, GString *buffer, GString *target)
{
	gchar *start;
	gchar *end;
	gsize len;
	gchar end_char;
	gchar *start_str = "";
	gchar *end_str = "";

	while (buffer->len > 0)
	{
		start = phon_find_start(buffer->str, &start_str, &end_str);
		end_char = *end_str;
		len = 0;

		if (start == NULL)
		{
			/* no phonetics at all, so add the text to the body to get at least possible
			 * links parsed and return */
			g_string_prepend(target, buffer->str);
			g_string_erase(buffer, 0, -1); /* remove already handled text */
			return;
		}
		/* get length of text *before* the end char */
		while (len < buffer->len && (buffer->str + len) != start)
			len++;

		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, buffer->str, len);
		len++; /* skip the end char */
		g_string_erase(buffer, 0, len); /* remove already handled text */

		start = buffer->str; /* set new start */
		end = strchr(start, end_char);
		if (start > end)
		{
			/* start & end chars don't match, skip this part */
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, start_str, 1);
			continue;
		}
		len = end - buffer->str; /* length of the phonetic string */

		gtk_text_buffer_insert_with_tags_by_name(dd->main_textbuffer, &dd->textiter,
				buffer->str, len, TAG_PHONETIC, NULL);

		g_string_erase(buffer, 0, len + 1); /* remove already handled text */
	}
}


static GtkTextTag *create_tag(DictData *dd, const gchar *link_str)
{
	GtkTextTag *tag;

	tag = gtk_text_buffer_create_tag(dd->main_textbuffer, NULL,
		"underline", PANGO_UNDERLINE_SINGLE,
		"foreground-rgba", dd->color_link, NULL);

	g_object_set_data_full(G_OBJECT(tag), TAG_LINK, g_strdup(link_str), g_free);

	return tag;
}


/* ignore links like {n} or {f} as they are often found in translation dictionaries and
 * used for giving additional type information but not intended to link or reference something */
static gboolean ignore_short_link(const gchar *str)
{
	if (str == NULL)
		return TRUE;

	/* ignore {n}, {f}, {m}  (surrounding braces are already stripped in 'str') */
	if (strcmp("f", str) == 0 ||
		strcmp("m", str) == 0 ||
		strcmp("n", str) == 0 ||
		strcmp("vr", str) == 0 ||
		strcmp("vt", str) == 0 ||
		strcmp("pl", str) == 0)
	{
		return TRUE;
	}

	return FALSE;
}


/* Find any cross-references like {reference} and make them clickable */
static void parse_body(DictData *dd, GString *buffer)
{
	gchar *start;
	gchar *end;
	gsize len;
	GtkTextTag *tag;
	gchar *found_link;

	while (buffer->len > 0)
	{
		start = strchr(buffer->str, '{');
		len = 0;

		if (start == NULL)
		{	/* no links at all, so add the text and go */
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, buffer->str, buffer->len);
			return;
		}
		/* get length of text *before* the next '{' */
		while (len < buffer->len && (buffer->str + len) != start)
			len++;

		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, buffer->str, len);
		len++; /* skip the '{' */
		g_string_erase(buffer, 0, len); /* remove already handled text */

		start = buffer->str; /* set new start */
		end = strchr(start, '}');
		if (start > end)
		{
			/* braces don't match, skip this part, e.g. 'fd-deu-eng' returns
			 * '    frozen}; to be cold; to freeze {froze' */
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "{", 1);
			continue;
		}
		len = end - buffer->str; /* length of the link */
		found_link = g_strndup(buffer->str, len);

		/* ignore {n}, {f}, ... */
		if (ignore_short_link(found_link))
		{
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "{", 1);
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, buffer->str, len);
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "}", 1);
		}
		else
		{
			tag = create_tag(dd, found_link);
			gtk_text_buffer_insert_with_tags(dd->main_textbuffer, &dd->textiter,
				buffer->str, len, tag, NULL);
		}
		g_free(found_link);
		g_string_erase(buffer, 0, len + 1); /* remove already handled text */
	}
}


static gint process_response_content(DictData *dd, gchar **lines, gint line_no, gint max_lines,
									 GString *header, GString *body)
{
	gchar **dict_parts;
	gboolean is_header;

	line_no++;
	if (strncmp(lines[line_no], "250", 3) == 0)
		return max_lines; /* end of data */
	if (strncmp(lines[line_no], "error:", 6) == 0) /* an error occurred */
	{
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, lines[line_no], -1);
		return max_lines;
	}
	if (strncmp(lines[line_no], "151", 3) != 0)
		return line_no; /* unexpected line start, try next line */

	/* get the used dictionary */
	dict_parts = g_strsplit(lines[line_no], "\"", -1);

	if (g_strv_length(dict_parts) > 3)
	{	gtk_text_buffer_insert_with_tags_by_name(dd->main_textbuffer, &dd->textiter,
			g_strstrip(dict_parts[3]), -1, TAG_BOLD, NULL);

		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, " (", 2);
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter,
			g_strstrip(dict_parts[2]), -1);
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, ")\n", 2);
	}
	g_strfreev(dict_parts);

	if (line_no >= (max_lines - 2))
		return max_lines;

	/* all following lines represents the translation */
	line_no++;
	is_header = TRUE;

	while (lines[line_no] != NULL && lines[line_no][0] != '\r' && lines[line_no][0] != '\n')
	{
		/* check for a leading period indicating end of text response */
		if (lines[line_no][0] == '.')
		{
			/* a double period at line start is a masked period, cf. RFC 2229 */
			if (strlen(lines[line_no]) > 1 && lines[line_no][1] == '.')
				/* the RFC says we should coolapse the two periods into one but we go the
				 * lazy way and simply replace the first period by a space */
				lines[line_no][0] = ' ';
			else
				break; /* we reached the end of the test response */
		}
		if (is_header && lines[line_no][0] != ' ')
		{
			g_string_append(header, lines[line_no]);
			g_string_append_c(header, '\n');
		}
		else
		{
			g_string_append(body, lines[line_no]);
			g_string_append_c(body, '\n');
			is_header = FALSE;
		}

		line_no++;
	}
	parse_header(dd, header, body);
	parse_body(dd, body);
	gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n\n", 2);
	g_string_erase(header, 0, -1);
	g_string_erase(body, 0, -1);

	return line_no;
}


static void clear_query_buffer(DictData *dd)
{
	g_free(dd->query_buffer);
	dd->query_buffer = NULL;
}


static void append_web_search_link(DictData *dd, gboolean prepend_whitespace)
{
	gchar *label, *text;

	if (dd->web_url == NULL || dd->mode_in_use != DICTMODE_DICT)
		return;

	label = _(dict_prefs_get_web_url_label(dd));
	text = g_strdup_printf(
		/* for translators: the first wildcard is the search term, the second wildcard
			* is the name of the preferred web search engine */
		_("Search \"%s\" using \"%s\""),
		dd->searched_word, label);

	if (prepend_whitespace)
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n\n", 2);

	gtk_text_buffer_insert_with_tags_by_name(dd->main_textbuffer, &dd->textiter,
		_("Web Search:"), -1, TAG_HEADING, NULL);

	gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
	gtk_text_buffer_insert_with_tags_by_name(dd->main_textbuffer, &dd->textiter,
		text, -1, TAG_LINK, NULL);
	g_free(text);
}


static gboolean process_server_response(gpointer user_data)
{
	DictData *dd = user_data;
	gint max_lines, i;
	gint defs_found = 0;
	gchar *answer, *tmp;
	gchar **lines;
	GString *header = g_string_sized_new(256);
	GString *body = g_string_sized_new(512);

	switch (dd->query_status)
	{
		case NO_CONNECTION:
		{
			dict_gui_status_add(dd, _("Could not connect to server."));
			dd->query_status = NO_ERROR;
			return FALSE;
		}
		case SERVER_NOT_READY:
		{
			dict_gui_status_add(dd, _("The server is not ready."));
			clear_query_buffer(dd);
			return FALSE;
		}
		case UNKNOWN_DATABASE:
		{
			dict_gui_status_add(dd,
				_("Invalid dictionary specified. Please check your preferences."));
			clear_query_buffer(dd);
			return FALSE;
		}
	}

	if (! NZV(dd->query_buffer))
	{
		dict_gui_status_add(dd, _("Unknown error while querying the server."));
		clear_query_buffer(dd);
		return FALSE;
	}

	answer = dd->query_buffer;
	/* go to next line */
	while (*answer != '\n')
		answer++;
	answer++;

	if (dd->query_status == NOTHING_FOUND)
	{
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
		gtk_text_buffer_insert_with_tags_by_name(dd->main_textbuffer, &dd->textiter,
			_("Dictionary Results:"), -1, TAG_HEADING, NULL);

		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
		tmp = g_strdup_printf(_("No matches could be found for \"%s\"."), dd->searched_word);
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, tmp, -1);
		dict_gui_textview_apply_tag_to_word(dd->main_textbuffer, dd->searched_word, &dd->textiter,
			TAG_ERROR, TAG_BOLD, NULL);
		dict_gui_status_add(dd, "%s", tmp);
		g_free(tmp);
		clear_query_buffer(dd);

		/* if we had no luck searching a word, maybe we have a typo so try searching with
		 * spell check and offer a Web search*/
		append_web_search_link (dd, TRUE);

		if (NZV(dd->spell_bin))
		{
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
			dict_spell_start_query(dd, dd->searched_word, FALSE);
		}

		return FALSE;
	}
	else if (strncmp("150", answer, 3) != 0 && dd->query_status != NOTHING_FOUND)
	{
		dict_gui_status_add(dd, _("Unknown error while querying the server."));
		clear_query_buffer(dd);
		return FALSE;
	}
	defs_found = atoi(answer + 4);
	dict_gui_status_add(dd, ngettext("%d definition found.",
                                     "%d definitions found.",
                                     defs_found), defs_found);

	/* go to next line */
	while (*answer != '\n')
		answer++;
	answer++;

	/* parse output */
	lines = g_strsplit(answer, "\r\n", -1);
	max_lines = g_strv_length(lines);
	if (lines == NULL || max_lines == 0)
	{
		clear_query_buffer(dd);
		return FALSE;
	}

	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &dd->textiter);
	gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);

	i = -1;
	while (i < max_lines)
	{
		i = process_response_content(dd, lines, i, max_lines, header, body);
	}

	append_web_search_link (dd, FALSE);

	g_strfreev(lines);
	clear_query_buffer(dd);

	g_string_free(header, TRUE);
	g_string_free(body, TRUE);

	return FALSE;
}


static gint get_answer(gint fd, gchar **buffer)
{
	gboolean fol = TRUE;
	gboolean sol = FALSE;
	gboolean tol = FALSE;
	GString *str;
	gchar c;
	gchar ec[3];
	gint query_status = NO_ERROR;

	if (buffer != NULL)
		str = g_string_sized_new(100);

	alarm(10); /* abort after 10 seconds, there should went wrong something */
	while (read(fd, &c, 1) > 0)
	{
		if (tol) /* third char of line */
		{
			ec[2] = c;
		}
		if (sol) /* second char of line */
		{
			ec[1] = c;
			sol = FALSE;
			tol = TRUE;
		}
		if (fol) /* first char of line */
		{
			ec[0] = c;
			fol = FALSE;
			sol = TRUE;
		}
		if (c == '\n') /* last char of line, so the next run is first char of next line */
		{
			fol = TRUE;
			sol = FALSE;
			tol = FALSE;
		}

		if (buffer != NULL)
			g_string_append_c(str, c);

		if (tol)
		{
			if (strncmp(ec, "250", 3) == 0 || 	/* ok */
				strncmp(ec, "500", 3) == 0 ||	/* unknown command */
				strncmp(ec, "501", 3) == 0)		/* syntax error */
			{
				break;
			}
			else if (strncmp(ec, "220", 3) == 0 ||	/* server ready */
					 strncmp(ec, "221", 3) == 0)	/* good bye */
			{
				query_status = NO_ERROR;
				break;
			}
			else if (strncmp(ec, "420", 3) == 0 ||
					 strncmp(ec, "421", 3) == 0) /* server not ready (server down or shutdown) */
			{
				query_status = SERVER_NOT_READY;
				break;
			}
			else if (strncmp(ec, "500", 3) == 0 ||
					 strncmp(ec, "501", 3) == 0) /* bad command or parameters */
			{
				query_status = BAD_COMMAND;
				break;
			}
			else if (strncmp(ec, "550", 3) == 0) /* invalid database */
			{
				query_status = UNKNOWN_DATABASE;
				break;
			}
			else if (strncmp(ec, "552", 3) == 0) /* nothing found */
			{
				query_status = NOTHING_FOUND;
				break;
			}
			else if (strncmp(ec, "554", 3) == 0) /* no databases present */
			{
				query_status = NO_DATABASES;
				break;
			}
		}
	}
	alarm(0);

	if (buffer != NULL)
	{
		g_string_append_c(str, '\0');
		*buffer = g_string_free(str, FALSE);
	}

	return query_status;
}


static gpointer ask_server(DictData *dd)
{
	gint fd, i;
	static gchar cmd[BUF_SIZE];

	if ((fd = open_socket(dd->server, dd->port)) == -1)
	{
		dd->query_status = NO_CONNECTION;
		g_idle_add(process_server_response, dd);
		g_thread_exit(NULL);
		return NULL;
	}
	dd->query_is_running = TRUE;
	dd->query_status = NO_CONNECTION;

	dd->query_status = get_answer(fd, NULL);
	if (dd->query_status == NO_ERROR)
	{
		/* take only the first part of the dictionary string, so let the string end at the space */
		i = 0;
		while (dd->dictionary[i] != ' ')
			i++;
		dd->dictionary[i] = '\0';

		g_snprintf(cmd, BUF_SIZE, "DEFINE %s \"%s\"", dd->dictionary, dd->searched_word);
		send_command(fd, cmd);

		/* and now, "append" again the rest of the dictionary string again */
		dd->dictionary[i] = ' ';

		dd->query_status = get_answer(fd, &(dd->query_buffer));
	}
	send_command(fd, "QUIT");
	get_answer(fd, NULL);
	close(fd);

	dd->query_is_running = FALSE;
	/* delegate parsing the response and related GUI stuff to GTK's main thread through the main loop */
	g_idle_add(process_server_response, dd);

	g_thread_exit(NULL);
	return NULL;
}


static void signal_cb(gint sig)
{
	/* do nothing here and hope we never get called */
}


static void dictd_init(void)
{
#ifdef SIGALRM
	static gboolean initialized = FALSE;

	if (G_UNLIKELY(! initialized))
	{
		struct sigaction action;
		action.sa_handler = &signal_cb;
		action.sa_flags = SA_RESTART;
		sigaction(SIGALRM, &action, NULL);

		initialized = TRUE;
	}
#endif
}


void dict_dictd_start_query(DictData *dd, const gchar *word)
{
	if (dd->query_is_running)
	{
		gdk_display_beep (gdk_display_get_default ());
	}
	else
	{
		dict_gui_status_add(dd, _("Querying %s..."), dd->server);

		dictd_init();

		/* start the thread to query the server */
		g_thread_new(NULL, (GThreadFunc) ask_server, dd);
	}
}


void dict_dictd_get_information(GtkWidget *button, DictData *dd)
{
	gint fd;
	gchar *buffer = NULL;
	gchar *answer = NULL;
	gchar *text, *end;
	GtkEntry *entry_server = g_object_get_data(G_OBJECT(button), "server_entry");
	GtkEntry *entry_port = g_object_get_data(G_OBJECT(button), "port_entry");
	const gchar *server;
	const gchar *port;
	GtkWidget *dialog, *label, *swin, *vbox;

	dictd_init();

	server = gtk_entry_get_text(entry_server);
	port = gtk_entry_get_text(entry_port);

	if ((fd = open_socket(server, port)) == -1)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("Could not connect to server."));
		return;
	}

	dd->query_status = NO_CONNECTION;

	dd->query_status = get_answer(fd, NULL);
	if (dd->query_status != NO_ERROR)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("Could not connect to server."));
		return;
	}

	send_command(fd, "SHOW SERVER");

	/* read all server output */
	dd->query_status = get_answer(fd, &answer);
	buffer = answer;
	send_command(fd, "QUIT");
	get_answer(fd, NULL);
	close(fd);

	/* go to next line */
	while (*buffer != '\n')
		buffer++;
	buffer++;

	if (strncmp("114", buffer, 3) != 0)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR,
			_("An error occurred while querying server information."));
		return;
	}

	/* go to next line */
	while (*buffer != '\n')
		buffer++;

	buffer++;

	end = strstr(buffer, ".\r\n250");
	*end = '\0';

	text = g_strdup_printf(_("Server Information for \"%s\""), server);
	dialog = xfce_titled_dialog_new_with_mixed_buttons(text,
				GTK_WINDOW(dd->window),
				GTK_DIALOG_DESTROY_WITH_PARENT,
				"window-close", _("_Close"), GTK_RESPONSE_CLOSE, NULL);
	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_container_set_border_width(GTK_CONTAINER(vbox), 5);
	gtk_container_add(GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (dialog))), vbox);
	gtk_box_set_spacing(GTK_BOX(vbox), 6);
	g_free(text);

	gtk_window_set_default_size(GTK_WINDOW(dialog), 550, 400);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);

	text = g_markup_printf_escaped("<tt>%s</tt>", buffer);
	label = gtk_label_new(text);
	gtk_label_set_use_markup(GTK_LABEL(label), TRUE);
	gtk_widget_set_vexpand(label, TRUE);
	g_free(text);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
		GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(swin), label);

	gtk_box_pack_start(GTK_BOX(vbox), swin, TRUE, TRUE, 0);

	gtk_widget_show_all(vbox);
	gtk_dialog_run(GTK_DIALOG(dialog));
	gtk_widget_destroy(dialog);

	g_free(answer);
}


void dict_dictd_get_list(GtkWidget *button, DictData *dd)
{
	gint fd, i;
	gint max_lines;
	gchar *buffer = NULL;
	gchar *answer = NULL;
	gchar **lines;
	GtkWidget *dict_combo = g_object_get_data(G_OBJECT(button), "dict_combo");
	GtkEntry *entry_server = g_object_get_data(G_OBJECT(button), "server_entry");
	GtkEntry  *entry_port = g_object_get_data(G_OBJECT(button), "port_entry");
	const gchar *server;
	const gchar *port;

	dictd_init();

	server = gtk_entry_get_text(entry_server);
	port = gtk_entry_get_text(entry_port);

	if ((fd = open_socket(server, port)) == -1)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("Could not connect to server."));
		return;
	}

	dd->query_status = NO_CONNECTION;

	dd->query_status = get_answer(fd, NULL);
	if (dd->query_status != NO_ERROR)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("Could not connect to server."));
		return;
	}

	send_command(fd, "SHOW DATABASES");

	/* read all server output */
	dd->query_status = get_answer(fd, &answer);
	buffer = answer;
	send_command(fd, "QUIT");
	get_answer(fd, NULL);
	close(fd);

	/* go to next line */
	while (*buffer != '\n')
		buffer++;
	buffer++;

	if (strncmp("554", buffer, 3) == 0)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("The server doesn't offer any databases."));
		return;
	}
	else if (strncmp("110", buffer, 3) != 0 && strncmp("554", buffer, 3) != 0)
	{
		dict_show_msgbox(dd, GTK_MESSAGE_ERROR, _("Unknown error while querying the server."));
		return;
	}

	/* go to next line */
	while (*buffer != '\n')
		buffer++;
	buffer++;

	/* clear the combo box */
	i = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(dict_combo)), NULL);
	for (i -= 1; i > 2; i--)  /* first three entries (*, ! and ----) should always exist */
	{
		gtk_combo_box_text_remove(GTK_COMBO_BOX_TEXT(dict_combo), i);
	}

	/* parse output */
	lines = g_strsplit(buffer, "\r\n", -1);
	max_lines = g_strv_length(lines);
	if (lines == NULL || max_lines == 0)
		return;

	i = 0;
	while (i < max_lines && lines[i][0] != '.')
	{
		gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(dict_combo), lines[i]);
		i++;
	}

	g_strfreev(lines);
	g_free(answer);

	/* set the active entry to * because we don't know where the previously selected item now is in
	 * the list and we also don't know whether it exists at all, and I don't walk through the list */
	gtk_combo_box_set_active(GTK_COMBO_BOX(dict_combo), 0);
}
