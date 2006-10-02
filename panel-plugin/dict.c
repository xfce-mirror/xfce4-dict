/*  $Id$
 *
 *  Copyright © 2006 Enrico Tröger <enrico.troeger@uvena.de>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>

#include <libxfcegui4/libxfcegui4.h>
#include <libxfce4panel/xfce-panel-plugin.h>
#include <libxfce4panel/xfce-panel-convenience.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>

#include "inline-icon.h"

#define BUF_SIZE 256


typedef struct
{
    XfcePanelPlugin *plugin;

    GtkWidget *window;
    GtkWidget *statusbar;
    GtkWidget *main_entry;
	GtkWidget *panel_entry;
	GtkWidget *main_textview;
	GtkTextBuffer *main_textbuffer;
	GtkTextTag *main_boldtag;
	//GtkWidget *main_dict_combo;

    GtkWidget *server_entry;
    GtkWidget *dict_combo;
    GtkWidget *port_spinner;
    GtkWidget *panel_entry_size_label;
    GtkWidget *panel_entry_size_spinner;
	GtkWidget *check_panel_entry;

    GtkWidget *panel_button;
    GtkWidget *panel_button_image;
    GtkTooltips *tooltips;

    gboolean show_panel_entry;
    gint panel_entry_size;
    gint port;
    gchar *server;
    gchar *dictionary;

    GdkPixbuf *icon;
}
DictData;




/* Panel Plugin Interface */

static void dict_properties_dialog(XfcePanelPlugin *plugin, DictData *dd);
static void dict_construct(XfcePanelPlugin *plugin);

XFCE_PANEL_PLUGIN_REGISTER_INTERNAL(dict_construct);


/* internal functions */


static GdkPixbuf *load_and_scale(const guint8 *data, int dstw, int dsth)
{
	GdkPixbuf *pb, *pb_scaled;
	int pb_w, pb_h;

	pb = gdk_pixbuf_new_from_inline(-1, data, FALSE, NULL);
	pb_w = gdk_pixbuf_get_width(pb);
	pb_h = gdk_pixbuf_get_height(pb);

	if (dstw == pb_w && dsth == pb_h)
		return(pb);
	else if (dstw < 0)
		dstw = (dsth * pb_w) / pb_h;
	else if (dsth < 0)
		dsth = (dstw * pb_h) / pb_w;

	pb_scaled = gdk_pixbuf_scale_simple(pb, dstw, dsth, GDK_INTERP_HYPER);
	g_object_unref(G_OBJECT(pb));

	return (pb_scaled);
}


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
	//fcntl( fd, F_SETFL, O_NONBLOCK );

	return (fd);
}


static void send_command(gint fd, const gchar *str)
{
	gchar buf[256];
	gint len = strlen (str);

	snprintf(buf, 256, "%s\n", str);
	send(fd, buf, len + 2, 0);
}


static gchar *get_answer(gint fd)
{
	gboolean fol = FALSE;
	gboolean sol = FALSE;
	gboolean tol = FALSE;
	GString *str = g_string_sized_new(100);
	gchar c, *tmp;
	gchar ec[3];

	alarm(10); // abort after 5 seconds, there should went wrong something
	while (read(fd, &c, 1) > 0)
	{
		if (tol) // third char of line
		{
			ec[2] = c;
		}
		if (sol) // second char of line
		{
			ec[1] = c;
			sol = FALSE;
			tol = TRUE;
		}
		if (fol) // first char of line
		{
			ec[0] = c;
			fol = FALSE;
			sol = TRUE;
		}
		if (c == '\n') // last char of line, so the next run is first char of next line
			fol = TRUE;

		g_string_append_c(str, c);
		if (tol &&
			(
				(strncmp(ec, "250", 3) == 0) || // ok
				(strncmp(ec, "554", 3) == 0) ||	// no databases present
				(strncmp(ec, "500", 3) == 0) ||	// unknown command
				(strncmp(ec, "501", 3) == 0) ||	// syntax error
				(strncmp(ec, "552", 3) == 0)	// nothing found
			)
		)
			 break;
	}
	alarm(0);

	g_string_append_c(str, '\0');
	tmp = str->str;
	g_string_free(str, FALSE);

	return tmp;
}


/* old code, never worked correctly with big results
static void get_answer(gint fd, gchar *buf)
{
	gint len;

	len = recv(fd, buf, BUF_SIZE - 1, 0);

	if (len <= 0) return;

	while (len > 1 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
	{
		buf[len - 1] = '\0';
		len--;
	}
}
*/


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


static void clear_text_buffer(DictData *dd)
{
	GtkTextIter start_iter, end_iter;

	// clear the TextBuffer
	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &start_iter);
	gtk_text_buffer_get_end_iter(dd->main_textbuffer, &end_iter);
	gtk_text_buffer_delete(dd->main_textbuffer, &start_iter, &end_iter);
}


static gboolean dict_ask_server(DictData *dd, const gchar *word)
{
	gint fd, i, max_lines;
	gint defs_found = 0;
	static gchar cmd[BUF_SIZE];
	gchar *buffer = NULL;
	gchar **lines;
	gchar *answer, *tmp, *stripped;
	GtkTextIter iter;

	if (word == NULL || strlen(word) == 0) return FALSE;

	clear_text_buffer(dd);
	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &iter);

	if ((fd = open_socket(dd->server, dd->port)) == -1)
	{
		dict_status_add(dd, _("Could not connect to server."));
		return FALSE;
	}

	if (strlen(word) > (BUF_SIZE - 11))
	{
		dict_status_add(dd, _("Input is too long."));
		return FALSE;
	}

	// take only the first part of the dictionary string, so let the string end at the space
	i = 0;
	while (dd->dictionary[i] != ' ') i++;
	dd->dictionary[i] = '\0';

	snprintf(cmd, BUF_SIZE, "define %s \"%s\"\n", dd->dictionary, word);
	send_command(fd, cmd);

	// and now, "append" again the rest of the dictionary string again
	dd->dictionary[i] = ' ';

	answer = buffer = get_answer(fd);
	close(fd);

	if (strncmp("220", answer, 3) != 0)
	{
		dict_status_add(dd, _("Server not ready."));
		return FALSE;
	}

	// go to next line
	while (*answer != '\n') *answer++;
	*answer++;

	if (strncmp("552", answer, 3) == 0)
	{
		dict_status_add(dd, _("No matches could be found for \"%s\"."), word);
		return TRUE;
	}
	else if (strncmp("150", answer, 3) != 0 && strncmp("552", answer, 3) != 0)
	{
		dict_status_add(dd, _("Unknown error while quering the server."));
		return FALSE;
	}
	defs_found = atoi(answer + 4);
	dict_status_add(dd, _("%d definition(s) found."), defs_found);

	// go to next line
	while (*answer != '\n') *answer++;
	*answer++;

	// parse output
	lines = g_strsplit(answer, "\r\n", -1);
	max_lines = g_strv_length(lines);
	if (lines == NULL || max_lines == 0) return FALSE;

	gtk_text_buffer_insert(dd->main_textbuffer, &iter, "\n", 1);

	i = -1;
	while (i < max_lines)
	{
		i++;
		if (strncmp(lines[i], "250", 3) == 0) break; // end of data
		if (strncmp(lines[i], "151", 3) != 0) continue; // unexpected line start, try next line

		// get the used dictionary
		tmp = strchr(lines[i], '"'); // get the opening " of quoted search word
		if (tmp != NULL)
		{
			tmp = strchr(tmp + 1, '"'); // get the closing " of quoted search word
			if (tmp != NULL)
			{
				// skip the found quote and the following space
				gtk_text_buffer_insert_with_tags(dd->main_textbuffer, &iter, tmp + 2, -1,
																dd->main_boldtag, NULL);
				gtk_text_buffer_insert(dd->main_textbuffer, &iter, "\n", 1);
			}
		}
		if (i >= (max_lines - 2)) break;

		// all following lines represents the translation
		i += 2; // skip the next two lines
		while (lines[i] != NULL && lines[i][0] != '.' && lines[i][0] != '\r' && lines[i][0] != '\n')
		{
			stripped = g_strstrip(lines[i]);
			if (strlen(stripped) > 0)
			{
				gtk_text_buffer_insert(dd->main_textbuffer, &iter, stripped, -1);
				if (i < (max_lines - 1) && lines[i + 1][0] != '.')
					gtk_text_buffer_insert(dd->main_textbuffer, &iter, "\n", 1);
			}
			i++;
		}
		gtk_text_buffer_insert(dd->main_textbuffer, &iter, "\n\n", 2);
	}
	g_strfreev(lines);
	g_free(buffer);

	// clear the panel entry to not search again when you click on the panel button
	gtk_entry_set_text(GTK_ENTRY(dd->panel_entry), "");
	return TRUE;
}


static void dict_free_data(XfcePanelPlugin *plugin, DictData *dd)
{
    g_free(dd->dictionary);
    g_free(dd->server);
    g_object_unref(dd->icon);
    gtk_object_sink(GTK_OBJECT(dd->tooltips));
    g_free(dd);
}


static gboolean dict_set_size(XfcePanelPlugin *plugin, gint size, DictData *dd)
{
	gint width;
	//g_object_unref(G_OBJECT(dd->icon));
	dd->icon = load_and_scale(dict_icon_data, size, -1);

    gtk_image_set_from_pixbuf(GTK_IMAGE(dd->panel_button_image), dd->icon);

    if (dd->show_panel_entry && xfce_panel_plugin_get_orientation(dd->plugin) == GTK_ORIENTATION_HORIZONTAL)
    	width = size + dd->panel_entry_size;
    else
		width = size;

    gtk_widget_set_size_request(GTK_WIDGET(plugin), width, size);

    return TRUE;
}


static void dict_change_orientation(XfcePanelPlugin *plugin, GtkOrientation orientation, DictData *dd)
{
	if (orientation == GTK_ORIENTATION_VERTICAL)
		gtk_widget_hide(dd->panel_entry);
	else
		gtk_widget_show(dd->panel_entry);
}


static void dict_style_set(XfcePanelPlugin *plugin, gpointer ignored, DictData *dd)
{
    dict_set_size(plugin, xfce_panel_plugin_get_size(plugin), dd);
}


static void dict_read_rc_file(XfcePanelPlugin *plugin, DictData *dd)
{
    gchar *file;
    XfceRc *rc;
    gint port = 2628;
    gint panel_entry_size = 120;
    gboolean show_panel_entry = FALSE;
    const gchar *server = "dict.org";
    const gchar *dict = "*";

    if ((file = xfce_panel_plugin_lookup_rc_file(plugin)) != NULL)
    {
        rc = xfce_rc_simple_open(file, TRUE);
        g_free(file);
        if (rc != NULL)
        {
            show_panel_entry = xfce_rc_read_bool_entry(rc, "show_panel_entry", show_panel_entry);
            panel_entry_size = xfce_rc_read_int_entry(rc, "panel_entry_size", panel_entry_size);
            port = xfce_rc_read_int_entry(rc, "port", port);
            server = xfce_rc_read_entry(rc, "server", server);
            dict = xfce_rc_read_entry(rc, "dict", dict);

            xfce_rc_close(rc);
        }
    }

    dd->show_panel_entry = show_panel_entry;
    dd->panel_entry_size = panel_entry_size;
    dd->port = port;
    dd->server = g_strdup(server);
    dd->dictionary = g_strdup(dict);
}


static void dict_write_rc_file(XfcePanelPlugin *plugin, DictData *dd)
{
    gchar *file;
    XfceRc *rc;

    if (! (file = xfce_panel_plugin_save_location(plugin, TRUE))) return;

    rc = xfce_rc_simple_open(file, FALSE);
    g_free(file);

    if (! rc) return;

    xfce_rc_write_bool_entry(rc, "show_panel_entry", dd->show_panel_entry);
    xfce_rc_write_int_entry(rc, "panel_entry_size", dd->panel_entry_size);
    xfce_rc_write_int_entry(rc, "port", dd->port);
    xfce_rc_write_entry(rc, "server", dd->server);
    xfce_rc_write_entry(rc, "dict", dd->dictionary);

    xfce_rc_close(rc);
}


static gboolean dict_get_dict_list_cb(GtkWidget *button, DictData *dd)
{
	gint fd, i;
	gint max_lines;
	gchar *buffer = NULL;
	gchar **lines;
	const gchar *host;
	gint port;

	host = gtk_entry_get_text(GTK_ENTRY(dd->server_entry));
	port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dd->port_spinner));

	if ((fd = open_socket(host, port)) == -1)
	{
		xfce_err(_("Could not connect to server."));
		return FALSE;
	}

	send_command(fd, "show databases");

	// read all server output and hope the buffer is big enough (8KB should be enough)
	buffer = get_answer(fd);
	close(fd);

	// go to next line
	while (*buffer != '\n') *buffer++;
	*buffer++;

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

	// go to next line
	while (*buffer != '\n') *buffer++;
	*buffer++;

	// clear the combo box
	i = gtk_tree_model_iter_n_children(gtk_combo_box_get_model(GTK_COMBO_BOX(dd->dict_combo)), NULL);
	for (i -= 1; i > 2; i--)  // first three entries (*, ! and ----) should always exist
	{
		gtk_combo_box_remove_text(GTK_COMBO_BOX(dd->dict_combo), i);
	}

	// parse output
	lines = g_strsplit(buffer, "\r\n", -1);
	max_lines = g_strv_length(lines);
	if (lines == NULL || max_lines == 0) return FALSE;

	i = 0;
	while (i < max_lines && lines[i][0] != '.')
	{
		gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo), lines[i]);
		i++;
	}

	g_strfreev(lines);
	g_free(buffer);

	// set the active entry to * because we don't know where the previously selected item now is in
	// the list and we also don't know whether it exists at all, and I don't walk through the list
	gtk_combo_box_set_active(GTK_COMBO_BOX(dd->dict_combo), 0);

	return TRUE;
}


static void dict_dialog_response(GtkWidget *dlg, gint response, DictData *dd)
{
    gchar *tmp;

    g_object_set_data(G_OBJECT(dd->plugin), "dialog", NULL);

    if (response == GTK_RESPONSE_OK)
    {
		tmp = gtk_combo_box_get_active_text(GTK_COMBO_BOX(dd->dict_combo));
		if (tmp == NULL || tmp[0] == '-')
		{
			xfce_err(_("You have chosen an invalid dictionary entry."));
			return;
		}

		dd->port = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dd->port_spinner));

		g_free(dd->server);
		dd->server = g_strdup(gtk_entry_get_text(GTK_ENTRY(dd->server_entry)));

		g_free(dd->dictionary);
		dd->dictionary = g_strdup(tmp);

		dd->show_panel_entry = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dd->check_panel_entry));
		dd->panel_entry_size = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(dd->panel_entry_size_spinner));
		if (dd->show_panel_entry && xfce_panel_plugin_get_orientation(dd->plugin) == GTK_ORIENTATION_HORIZONTAL)
		{
			gtk_widget_show(dd->panel_entry);
		}
		else
			gtk_widget_hide(dd->panel_entry);

		dict_set_size(dd->plugin, xfce_panel_plugin_get_size(dd->plugin), dd);
		dict_write_rc_file(dd->plugin, dd);
    }
    gtk_widget_destroy(dlg);
    xfce_panel_plugin_unblock_menu(dd->plugin);
}


static void show_panel_entry_toggled(GtkToggleButton *tb, DictData *dd)
{
	gtk_widget_set_sensitive(dd->panel_entry_size_spinner, gtk_toggle_button_get_active(tb));
	gtk_widget_set_sensitive(dd->panel_entry_size_label, gtk_toggle_button_get_active(tb));
}


static void dict_properties_dialog(XfcePanelPlugin *plugin, DictData *dd)
{
    GtkWidget *dlg, *header, *vbox, *table, *label1, *label2, *label3;
    GtkWidget *button_get_list;

    xfce_panel_plugin_block_menu(plugin);

    dlg = gtk_dialog_new_with_buttons(_("Properties"),
                GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(plugin))),
                GTK_DIALOG_DESTROY_WITH_PARENT |
                GTK_DIALOG_NO_SEPARATOR,
                GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                GTK_STOCK_CLOSE, GTK_RESPONSE_OK,
                NULL);

    g_object_set_data(G_OBJECT(plugin), "dialog", dlg);

    gtk_window_set_position(GTK_WINDOW(dlg), GTK_WIN_POS_CENTER);

    g_signal_connect(dlg, "response", G_CALLBACK(dict_dialog_response), dd);

    gtk_container_set_border_width(GTK_CONTAINER(dlg), 2);

    header = xfce_create_header(NULL, _("Dictionary plugin"));
    gtk_widget_set_size_request(GTK_BIN(header)->child, 200, 32);
    gtk_container_set_border_width(GTK_CONTAINER(header), 6);
    gtk_widget_show(header);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG (dlg)->vbox), header, FALSE, TRUE, 0);

    vbox = gtk_vbox_new(FALSE, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 6);
    gtk_widget_show(vbox);
    gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dlg)->vbox), vbox, TRUE, TRUE, 0);

    /* server address */
    label1 = gtk_label_new_with_mnemonic(_("Server :"));
    gtk_widget_show(label1);

    dd->server_entry = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(dd->server_entry), 256);
    if (dd->server != NULL)
    {
    	gtk_entry_set_text(GTK_ENTRY(dd->server_entry), dd->server);
    }
    gtk_widget_show(dd->server_entry);

    /* server port */
    label2 = gtk_label_new_with_mnemonic(_("Server Port :"));
    gtk_widget_show(label2);

    dd->port_spinner = gtk_spin_button_new_with_range(0.0, 65536.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dd->port_spinner), dd->port);
    gtk_widget_show(dd->port_spinner);

    /* dictionary */
    label3 = gtk_label_new_with_mnemonic(_("Dictionary :"));
    gtk_widget_show(label3);

    dd->dict_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo), _("* (use all)"));
    gtk_combo_box_append_text(GTK_COMBO_BOX(dd->dict_combo), _("! (use all, stop after first match)"));
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


	/* show panel entry check box */
	dd->check_panel_entry = gtk_check_button_new_with_label(_("Show text field in the panel"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dd->check_panel_entry), dd->show_panel_entry);
    gtk_tooltips_set_tip(dd->tooltips, dd->check_panel_entry,
		_("This option can only be used when the panel has a horizontal orientation."), NULL);
	gtk_widget_show(dd->check_panel_entry);
	g_signal_connect(dd->check_panel_entry, "toggled", G_CALLBACK(show_panel_entry_toggled), dd);

    /* panel entry size */
    dd->panel_entry_size_label = gtk_label_new_with_mnemonic(_("Text field size :"));
    gtk_widget_show(dd->panel_entry_size_label);
    dd->panel_entry_size_spinner = gtk_spin_button_new_with_range(0.0, 500.0, 1.0);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(dd->panel_entry_size_spinner), dd->panel_entry_size);

   	//gtk_widget_set_sensitive(dd->check_panel_entry, gtk_toggle_button_get_active(tb));
    gtk_widget_show(dd->panel_entry_size_spinner);


	/* put it all together */
    table = gtk_table_new(5, 3, FALSE);
    gtk_widget_show(table);
	gtk_table_set_row_spacings(GTK_TABLE(table), 10);
	gtk_table_set_col_spacings(GTK_TABLE(table), 10);

	gtk_table_attach(GTK_TABLE(table), label1, 0, 1, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label1), 1, 0);

	gtk_table_attach(GTK_TABLE(table), dd->server_entry, 1, 2, 0, 1,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(table), label2, 0, 1, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label2), 1, 0);

	gtk_table_attach(GTK_TABLE(table), dd->port_spinner, 1, 2, 1, 2,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(table), label3, 0, 1, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label3), 1, 0);

	gtk_table_attach(GTK_TABLE(table), dd->dict_combo, 1, 2, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(table), button_get_list, 2, 3, 2, 3,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(table), dd->check_panel_entry, 1, 2, 3, 4,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

	gtk_table_attach(GTK_TABLE(table), dd->panel_entry_size_label, 0, 1, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label3), 1, 0);

	gtk_table_attach(GTK_TABLE(table), dd->panel_entry_size_spinner, 1, 2, 4, 5,
					(GtkAttachOptions) (GTK_FILL),
					(GtkAttachOptions) (0), 0, 0);

    gtk_box_pack_start(GTK_BOX(vbox), table, FALSE, FALSE, 0);

    gtk_widget_show(dlg);
}


static void entry_activate_cb(GtkEntry *entry, DictData *dd)
{
	const gchar *entered_text;

	if ((GtkWidget*)entry == dd->panel_entry)
	{
		entered_text = gtk_entry_get_text(GTK_ENTRY(dd->panel_entry));
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), entered_text);
		gtk_widget_show(dd->window);
		gtk_window_deiconify(GTK_WINDOW(dd->window));
		gtk_window_present(GTK_WINDOW(dd->window));
	}
	else
		entered_text = gtk_entry_get_text(GTK_ENTRY(dd->main_entry));

	dict_ask_server(dd, entered_text);
}


static void entry_button_clicked_cb(GtkButton *button, DictData *dd)
{
	entry_activate_cb(NULL, dd);
	gtk_widget_grab_focus(dd->main_entry);
}


static void clear_button_clicked_cb(GtkButton *button, DictData *dd)
{
	clear_text_buffer(dd);

	// clear the entries
	gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
	gtk_entry_set_text(GTK_ENTRY(dd->panel_entry), "");

	dict_status_add(dd, _("Ready."));
}


static void close_button_clicked(GtkWidget *button, DictData *dd)
{
	gtk_widget_hide(dd->window);
}


static void dict_create_main_dialog(DictData *dd)
{
	GtkWidget *main_box;
	GtkWidget *entry_box, *label_box, *entry_label, *entry_button, *clear_button, *close_button;
	GtkWidget *sep, *align, *scrolledwindow_results;
	//GtkWidget *dict_box, *dict_label, *combo_event_box;

	dd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(dd->window), "xfce4-dict-plugin");
	gtk_window_set_icon(GTK_WINDOW(dd->window), dd->icon);
	gtk_window_set_default_size(GTK_WINDOW(dd->window), 500, 300);

	g_signal_connect(G_OBJECT(dd->window), "delete_event", G_CALLBACK(gtk_widget_hide), NULL);

	main_box = gtk_vbox_new(FALSE, 0);
	gtk_widget_show(main_box);
	gtk_container_add(GTK_CONTAINER(dd->window), main_box);

	// entry box (label, entry, buttons)
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
	g_signal_connect(dd->main_entry, "activate", G_CALLBACK(entry_activate_cb), dd);

	entry_button = gtk_button_new_from_stock("gtk-find");
	gtk_widget_show(entry_button);
	gtk_box_pack_start(GTK_BOX(entry_box), entry_button, FALSE, FALSE, 0);
	g_signal_connect(entry_button, "clicked", G_CALLBACK(entry_button_clicked_cb), dd);

	clear_button = gtk_button_new_from_stock("gtk-clear");
	gtk_widget_show(clear_button);
	gtk_box_pack_start(GTK_BOX(entry_box), clear_button, FALSE, FALSE, 0);
	g_signal_connect(clear_button, "clicked", G_CALLBACK(clear_button_clicked_cb), dd);

	// just make some space
	align = gtk_alignment_new(1, 0.5, 0, 0);
	gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 10, 0);
	gtk_widget_show(align);
	gtk_container_add(GTK_CONTAINER(align), gtk_label_new(""));
	gtk_box_pack_start(GTK_BOX(entry_box), align, FALSE, FALSE, 0);

	close_button = gtk_button_new_from_stock("gtk-close");
	gtk_widget_show(close_button);
	g_signal_connect(close_button, "clicked", G_CALLBACK(close_button_clicked), dd);
	gtk_box_pack_end(GTK_BOX(entry_box), close_button, FALSE, FALSE, 2);

	// insert it here and it will(hopefully) be placed before the Close button
	sep = gtk_vseparator_new();
	gtk_widget_show(sep);
	gtk_box_pack_end(GTK_BOX(entry_box), sep, FALSE, FALSE, 5);

/*
	// dictionary chooser area
	dict_box = gtk_hbox_new(FALSE, 10);
	gtk_widget_show(dict_box);
	gtk_box_pack_start(GTK_BOX(main_box), dict_box, FALSE, FALSE, 5);

	dict_label = gtk_label_new(_("Dictioanry to use:"));
	gtk_widget_show(dict_label);
	gtk_box_pack_start(GTK_BOX(dict_box), dict_label, FALSE, FALSE, 0);

    dd->main_dict_combo = gtk_combo_box_new_text();
    gtk_combo_box_append_text(GTK_COMBO_BOX(dd->main_dict_combo), "*");
    gtk_combo_box_append_text(GTK_COMBO_BOX(dd->main_dict_combo), "!");

    gtk_widget_show(dd->main_dict_combo);

    combo_event_box = gtk_event_box_new();
    gtk_widget_show(combo_event_box);
	gtk_container_add(GTK_CONTAINER(combo_event_box), dd->main_dict_combo);
    gtk_tooltips_set_tip(dd->tooltips, combo_event_box,
		_("Define the dictionary to be used.\nChoose \"*\" to use all available dictionaries.\nChoose \"!\" to use all available dictionaries, but stop the search after first result."), NULL);
	gtk_box_pack_start(GTK_BOX(dict_box), combo_event_box, FALSE, FALSE, 0);
*/

	// results area
	scrolledwindow_results = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow_results);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_results), 4);
	gtk_box_pack_start(GTK_BOX(main_box), scrolledwindow_results, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow_results),
								GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_results),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	// searched words textview stuff
	dd->main_textview = gtk_text_view_new();
	gtk_text_view_set_editable(GTK_TEXT_VIEW(dd->main_textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(dd->main_textview), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(dd->main_textview), 5);
	dd->main_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(dd->main_textview));
	dd->main_boldtag = gtk_text_buffer_create_tag(dd->main_textbuffer,
			"bold", "weight", PANGO_WEIGHT_BOLD, NULL);

	gtk_widget_show(dd->main_textview);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_results), dd->main_textview);

	// status bar
	dd->statusbar = gtk_statusbar_new();
	gtk_widget_show(dd->statusbar);
	gtk_box_pack_end(GTK_BOX(main_box), dd->statusbar, FALSE, FALSE, 0);
}


static void dict_about_dialog(GtkWidget *widget, DictData *dd)
{
	GtkWidget *dialog;
	XfceAboutInfo *info;

	info = xfce_about_info_new("xfce4-dict-plugin", VERSION,
                               _("A plugin to query a Dict server."),
                               XFCE_COPYRIGHT_TEXT("2006", "Enrico Tröger"),
                               XFCE_LICENSE_GPL);

	xfce_about_info_add_credit(info, "Enrico Tröger", "enrico.troeger@uvena.de", _("Developer"));
	//xfce_about_info_set_homepage(info, "http://goodies.xfce.org");

	dialog = xfce_about_dialog_new_with_values(GTK_WINDOW(widget), info, dd->icon);
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(gtk_widget_destroy), NULL);
	gtk_window_set_title(GTK_WINDOW(dialog), "xfce4-dict-plugin");
	gtk_dialog_run(GTK_DIALOG(dialog));

	xfce_about_info_free(info);
}


static void dict_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context,
									gint x, gint y,GtkSelectionData *data, guint info, guint time,
									DictData *dd)
{
	if ((data->length >= 0) && (data->format == 8))
	{
		if (drag_context->action == GDK_ACTION_ASK)
		{
			drag_context->action = GDK_ACTION_COPY;
		}
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), data->data);
		dict_ask_server(dd, data->data);
		gtk_widget_show(dd->window);
		gtk_window_deiconify(GTK_WINDOW(dd->window));
		gtk_window_present(GTK_WINDOW(dd->window));
		gtk_drag_finish(drag_context, TRUE, FALSE, time);
	}
}


static void dict_panel_button_clicked(GtkWidget *button, DictData *dd)
{
	if (GTK_WIDGET_VISIBLE(dd->window))
		gtk_widget_hide(dd->window);
	else
	{
		const gchar *panel_text = gtk_entry_get_text(GTK_ENTRY(dd->panel_entry));
		if (strcmp("", panel_text) != 0)
		{
			dict_ask_server(dd, panel_text);
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), panel_text);
		}
		gtk_widget_grab_focus(dd->main_entry);
		gtk_widget_show(dd->window);
	}
}

static gboolean dict_panel_entry_buttonpress_cb(GtkWidget *entry, GdkEventButton *event, DictData *dd)
{
  GtkWidget *toplevel;

  /* Determine toplevel parent widget */
  toplevel = gtk_widget_get_toplevel(entry);

  /* Grab entry focus if possible */
  if (event->button != 3 && toplevel && toplevel->window)
    xfce_panel_plugin_focus_widget(dd->plugin, entry);

  return FALSE;
}


static void signal_cb(gint sig)
{
	// do nothing here and hope we never get called
}


static void dict_construct(XfcePanelPlugin *plugin)
{
	DictData *dd = g_new0(DictData, 1);
	GtkWidget *hbox;

    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    dd->plugin = plugin;

	//dd->icon = load_and_scale(dict_icon_data, 24, -1);

    dict_read_rc_file(plugin, dd);

    dd->panel_button = xfce_create_panel_button();

    dd->tooltips = gtk_tooltips_new();
    gtk_tooltips_set_tip(dd->tooltips, dd->panel_button, _("Look up a word"), NULL);

    dd->panel_button_image = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(dd->panel_button), GTK_WIDGET(dd->panel_button_image));

    gtk_widget_show_all(dd->panel_button);

    g_signal_connect(dd->panel_button, "clicked", G_CALLBACK(dict_panel_button_clicked), dd);

    dict_create_main_dialog(dd);

    g_signal_connect(plugin, "free-data", G_CALLBACK(dict_free_data), dd);

    g_signal_connect(plugin, "size-changed", G_CALLBACK(dict_set_size), dd);

	g_signal_connect(plugin, "orientation-changed", G_CALLBACK(dict_change_orientation), dd);

    g_signal_connect(plugin, "style-set", G_CALLBACK(dict_style_set), dd);

    xfce_panel_plugin_menu_show_configure(plugin);
    g_signal_connect(plugin, "configure-plugin", G_CALLBACK(dict_properties_dialog), dd);

    xfce_panel_plugin_menu_show_about(plugin);
    g_signal_connect(plugin, "about", G_CALLBACK(dict_about_dialog), dd);

	/* panel entry */
	dd->panel_entry = gtk_entry_new();
	gtk_entry_set_width_chars(GTK_ENTRY(dd->panel_entry), 15);
	g_signal_connect(dd->panel_entry, "activate", G_CALLBACK(entry_activate_cb), dd);
	g_signal_connect(dd->panel_entry, "button-press-event", G_CALLBACK(dict_panel_entry_buttonpress_cb), dd);

	if (dd->show_panel_entry && xfce_panel_plugin_get_orientation(dd->plugin) == GTK_ORIENTATION_HORIZONTAL)
		gtk_widget_show(dd->panel_entry);

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);

    gtk_container_add(GTK_CONTAINER(hbox), dd->panel_button);
    gtk_container_add(GTK_CONTAINER(hbox), dd->panel_entry);
    gtk_container_add(GTK_CONTAINER(plugin), hbox);

    xfce_panel_plugin_add_action_widget(plugin, dd->panel_button);

    /* DnD stuff */
    gtk_drag_dest_set(GTK_WIDGET(dd->panel_button), GTK_DEST_DEFAULT_ALL,
		(GtkTargetEntry[]) { { "STRING", 0, 0 }, { "text/plain", 0, 0 } }, 2, GDK_ACTION_COPY);
    g_signal_connect(dd->panel_button, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);
    g_signal_connect(dd->panel_entry, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);

	dict_status_add(dd, _("Ready."));

	siginterrupt(SIGALRM, 1);
	signal(SIGALRM, signal_cb);
}
XFCE_PANEL_PLUGIN_REGISTER_EXTERNAL(dict_construct);
