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


/* This file contains all networking related code to query a remote (or local)
 * dictd server (RFC 2229). */
/* TODO: This should be more separated from GUI code, sometime. */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <libxfcegui4/libxfcegui4.h>

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


#include "common.h"
#include "dictd.h"
#include "gui.h"
#include "aspell.h"


#define BUF_SIZE 256



static gint open_socket(const gchar *host_name, gint port)
{
	struct sockaddr_in addr;
	struct hostent *host_p;
	gint fd;
	gint opt = 1;

	memset((gchar *) &addr, 0, sizeof (addr));

	if ((addr.sin_addr.s_addr = inet_addr(host_name)) == INADDR_NONE)
	{
		host_p = gethostbyname(host_name);
		if (host_p == NULL)
			return (-1);
		memcpy((gchar *)(&addr.sin_addr), host_p->h_addr, (size_t)host_p->h_length);
	}

	addr.sin_family  = AF_INET;
	addr.sin_port    = htons((gushort) port);

	if ((fd = socket (AF_INET, SOCK_STREAM, 0)) < 0)
		return (-1);

	setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (gchar *) &opt, sizeof (opt));

	if (connect(fd, (struct sockaddr *) &addr, sizeof (addr)) != 0)
	{
		close(fd);
		return (-1);
	}
	/*fcntl( fd, F_SETFL, O_NONBLOCK );*/

	return (fd);
}


static void send_command(gint fd, const gchar *str)
{
	gchar buf[256];
	gint len = strlen (str);

	g_snprintf(buf, 256, "%s\n", str);
	send(fd, buf, len + 2, 0);
}


static gboolean process_server_response(DictData *dd)
{
	gint max_lines, i;
	gint defs_found = 0;
	gchar *answer, *tmp, *stripped;
	gchar **lines, **dict_parts;

	if (dd->query_status == NO_CONNECTION)
	{
		dict_gui_status_add(dd, _("Could not connect to server."));
		dd->query_status = NO_ERROR;
		return FALSE;
	}

	if (dd->query_buffer == NULL || strlen(dd->query_buffer) == 0)
	{
		dict_gui_status_add(dd, _("Unknown error while quering the server."));
		g_free(dd->query_buffer);
		return FALSE;
	}

	answer = dd->query_buffer;
	if (dd->query_status == SERVER_NOT_READY)
	{
		dict_gui_status_add(dd, _("Server not ready."));
		g_free(dd->query_buffer);
		return FALSE;
	}

	if (dd->query_status == UNKNOWN_DATABASE)
	{
		dict_gui_status_add(dd, _("Invalid dictionary specified. Please check your preferences."));
		g_free(dd->query_buffer);
		return FALSE;
	}

	/* go to next line */
	while (*answer != '\n') answer++;
	answer++;

	if (dd->query_status == NOTHING_FOUND)
	{
		dict_gui_status_add(dd, _("Ready."));

		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
		tmp = g_strdup_printf(_("No matches could be found for \"%s\"."), dd->searched_word);
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, tmp, -1);
		g_free(tmp);
		g_free(dd->query_buffer);

		/* if we had no luck searching a word, maybe we have a typo so try searching with
		 * aspell */
		if (NZV(dd->spell_bin))
		{
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
			dict_aspell_start_query(dd, dd->searched_word);
		}

		return FALSE;
	}
	else if (strncmp("150", answer, 3) != 0 && dd->query_status != NOTHING_FOUND)
	{
		dict_gui_status_add(dd, _("Unknown error while quering the server."));
		g_free(dd->query_buffer);
		return FALSE;
	}
	defs_found = atoi(answer + 4);
	dict_gui_status_add(dd, ngettext("%d definition found.",
                                     "%d definitions found.",
                                     defs_found), defs_found);

	/* go to next line */
	while (*answer != '\n') answer++;
	answer++;

	/* parse output */
	lines = g_strsplit(answer, "\r\n", -1);
	max_lines = g_strv_length(lines);
	if (lines == NULL || max_lines == 0)
	{
		g_free(dd->query_buffer);
		return FALSE;
	}

	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &dd->textiter);
	gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);

	i = -1;
	while (i < max_lines)
	{
		i++;
		if (strncmp(lines[i], "250", 3) == 0)
			break; /* end of data */
		if (strncmp(lines[i], "error:", 6) == 0) /* an error occurred */
		{
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, lines[i], -1);
			break;
		}
		if (strncmp(lines[i], "151", 3) != 0)
			continue; /* unexpected line start, try next line */

		/* get the used dictionary */
		dict_parts = g_strsplit(lines[i], "\"", -1);

		if (g_strv_length(dict_parts) > 3)
		{	gtk_text_buffer_insert_with_tags(dd->main_textbuffer, &dd->textiter,
				g_strstrip(dict_parts[3]), -1, dd->main_tag_bold, NULL);

			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, " (", 2);
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter,
				g_strstrip(dict_parts[2]), -1);
			gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, ")\n", 2);
		}
		g_strfreev(dict_parts);

		if (i >= (max_lines - 2))
			break;

		/* all following lines represents the translation */
		i += 2; /* skip the next two lines */
		while (lines[i] != NULL && lines[i][0] != '\r' && lines[i][0] != '\n')
		{
			/* check for a leading period indicating end of text response */
			if (lines[i][0] == '.')
			{
				/* a double period at line start is a masked period, cf. RFC 2229 */
				if (strlen(lines[i]) > 1 && lines[i][1] == '.')
					/* the RFC says we should coolapse the two periods into one but we go the
					 * lazy way and simply replace the first period by a space */
					lines[i][0] = ' ';
				else
					break; /* we reached the end of the test response */
			}
			stripped = g_strstrip(lines[i]);
			if (strlen(stripped) > 0)
			{
				gtk_text_buffer_insert_with_tags(dd->main_textbuffer, &dd->textiter, stripped, -1,
					dd->main_tag_indent, NULL);
				if (i < (max_lines - 1) && lines[i + 1][0] != '.')
					gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n", 1);
			}
			i++;
		}
		gtk_text_buffer_insert(dd->main_textbuffer, &dd->textiter, "\n\n", 2);
	}
	g_strfreev(lines);
	g_free(dd->query_buffer);

	return FALSE;
}


static gchar *get_answer(DictData *dd, gint fd)
{
	gboolean fol = FALSE;
	gboolean sol = FALSE;
	gboolean tol = FALSE;
	GString *str = g_string_sized_new(100);
	gchar c;
	gchar ec[3];

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
			fol = TRUE;

		g_string_append_c(str, c);
		if (tol)
		{
			if (strncmp(ec, "250", 3) == 0 || 	/* ok */
				strncmp(ec, "500", 3) == 0 ||	/* unknown command */
				strncmp(ec, "501", 3) == 0)		/* syntax error */
			{
				break;
			}
			else if (strncmp(ec, "220", 3) == 0) /* server not ready */
			{
				dd->query_status = SERVER_NOT_READY;
				break;
			}
			else if (strncmp(ec, "550", 3) == 0) /* invalid database */
			{
				dd->query_status = UNKNOWN_DATABASE;
				break;
			}
			else if (strncmp(ec, "552", 3) == 0) /* nothing found */
			{
				dd->query_status = NOTHING_FOUND;
				break;
			}
			else if (strncmp(ec, "554", 3) == 0) /* no databases present */
			{
				dd->query_status = NO_DATABASES;
				break;
			}
		}
	}
	alarm(0);

	g_string_append_c(str, '\0');

	return g_string_free(str, FALSE);
}


static void ask_server(DictData *dd)
{
	gint fd, i;
	static gchar cmd[BUF_SIZE];

	if ((fd = open_socket(dd->server, dd->port)) == -1)
	{
		dd->query_status = NO_CONNECTION;
		g_idle_add((GSourceFunc) process_server_response, dd);
		g_thread_exit(NULL);
		return;
	}

	dd->query_is_running = TRUE;
	dd->query_status = NO_ERROR;

	/* take only the first part of the dictionary string, so let the string end at the space */
	i = 0;
	while (dd->dictionary[i] != ' ') i++;
	dd->dictionary[i] = '\0';

	g_snprintf(cmd, BUF_SIZE, "define %s \"%s\"\n", dd->dictionary, dd->searched_word);
	send_command(fd, cmd);

	/* and now, "append" again the rest of the dictionary string again */
	dd->dictionary[i] = ' ';

	dd->query_buffer = get_answer(dd, fd);
	close(fd);

	dd->query_is_running = FALSE;
	/* delegate parsing the response and related GUI stuff to GTK's main thread through the main loop */
	g_idle_add((GSourceFunc) process_server_response, dd);

	g_thread_exit(NULL);
}


void signal_cb(gint sig)
{
	/* do nothing here and hope we never get called */
}


static void dictd_init()
{
#ifdef SIGALRM
	static gboolean initialized = FALSE;

	if (G_UNLIKELY(! initialized))
	{
		siginterrupt(SIGALRM, 1);
		signal(SIGALRM, signal_cb);

		initialized = TRUE;
	}
#endif
}


void dict_dictd_start_query(DictData *dd, const gchar *word)
{
	if (dd->query_is_running)
	{
		gdk_beep();
	}
	else
	{
		dict_gui_status_add(dd, _("Querying %s..."), dd->server);

		dictd_init();

		/* start the thread to query the server */
		g_thread_create((GThreadFunc) ask_server, dd, FALSE, NULL);
	}
}


gboolean dict_dictd_get_list(GtkWidget *button, DictData *dd)
{
	gint fd, i;
	gint max_lines;
	gchar *buffer = NULL;
	gchar *answer = NULL;
	gchar **lines;
	GtkWidget *dict_combo = g_object_get_data(G_OBJECT(button), "dict_combo");
	GtkEntry *entry_server = g_object_get_data(G_OBJECT(button), "server_entry");
	GtkSpinButton *entry_port = g_object_get_data(G_OBJECT(button), "port_spinner");
	const gchar *server;
	gint port;

	dictd_init();

	server = gtk_entry_get_text(entry_server);
	port = gtk_spin_button_get_value_as_int(entry_port);

	if ((fd = open_socket(server, port)) == -1)
	{
		xfce_err(_("Could not connect to server."));
		return FALSE;
	}

	send_command(fd, "show databases");

	dd->query_status = NO_ERROR;

	/* read all server output */
	answer = buffer = get_answer(dd, fd);
	close(fd);

	/* go to next line */
	while (*buffer != '\n') buffer++;
	buffer++;

	if (strncmp("554", buffer, 3) == 0)
	{
		close(fd);
		xfce_err(_("The server doesn't offer any databases."));
		return TRUE;
	}
	else if (strncmp("110", buffer, 3) != 0 && strncmp("554", buffer, 3) != 0)
	{
		xfce_err(_("Unknown error while quering the server."));
		close(fd);
		return FALSE;
	}

	/* go to next line */
	while (*buffer != '\n') buffer++;
	buffer++;

	/* clear the combo box */
	i = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(dict_combo)), NULL);
	for (i -= 1; i > 2; i--)  /* first three entries (*, ! and ----) should always exist */
	{
		gtk_combo_box_remove_text(GTK_COMBO_BOX(dict_combo), i);
	}

	/* parse output */
	lines = g_strsplit(buffer, "\r\n", -1);
	max_lines = g_strv_length(lines);
	if (lines == NULL || max_lines == 0) return FALSE;

	i = 0;
	while (i < max_lines && lines[i][0] != '.')
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(dict_combo), lines[i]);
		i++;
	}

	g_strfreev(lines);
	g_free(answer);

	/* set the active entry to * because we don't know where the previously selected item now is in
	 * the list and we also don't know whether it exists at all, and I don't walk through the list */
	gtk_combo_box_set_active(GTK_COMBO_BOX(dict_combo), 0);

	return TRUE;
}

