/*  $Id$
 *
 *  Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#include <stdio.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if HAVE_LOCALE_H
# include <locale.h>
#endif

/* Simple forward declaration to avoid inclusion of libxfce4panel headers */
typedef struct XfcePanelPlugin_t XfcePanelPlugin;

#include "common.h"
#include "popup_plugin.h"



extern void dict_panel_plugin_unblock(DictData *dd)
{
	// no-op
}


extern void dict_panel_plugin_save_settings(DictData *dd)
{
	// no-op
}


gboolean main_quit(GtkWidget *widget, GdkEvent *event, DictData *dd)
{
	dict_free_data(dd);
    gtk_main_quit();

    return FALSE;
}


void close_button_clicked(GtkWidget *button, DictData *dd)
{
	main_quit(NULL, NULL, dd);
}


gint main(gint argc, gchar *argv[])
{
	DictData *dd;

#ifdef ENABLE_NLS
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

	gtk_init(&argc, &argv);
    gtk_window_set_default_icon_name("xfce4-dict");

	/* try to find an existing panel plugin and pop it up */
	if (dict_find_panel_plugin())
		exit(0);

	/* no plugin found, start usual stand-alone app */

	dd = g_new0(DictData, 1);

	g_thread_init(NULL);

	/* initialise some pointers, unused panel plugins pointers defaults to NULL when not used */
	dd->plugin = NULL;
	dd->searched_word = NULL;
	dd->query_is_running = FALSE;
	dd->query_status = NO_ERROR;
	dd->panel_button = NULL;
	dd->tooltips = NULL;
	dd->panel_button_image = NULL;
	dd->panel_entry = NULL;

	dict_read_rc_file(dd);

	dict_create_main_window(dd);

	g_signal_connect(dd->window, "delete-event", G_CALLBACK(main_quit), dd);
	g_signal_connect(dd->close_button, "clicked", G_CALLBACK(close_button_clicked), dd);

	dict_status_add(dd, _("Ready."));

	siginterrupt(SIGALRM, 1);
	signal(SIGALRM, dict_signal_cb);

	gtk_widget_show_all(dd->window);

	gtk_main();

	return 0;
}
