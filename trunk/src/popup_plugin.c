/*  $Id$
 *
 *  Copyright 2008      Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *  Copyright 2006      Darren Salt
 *  Copyright 2002-2006 Olivier Fourdan
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


#include <gtk/gtk.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>
#include <string.h>

#include "libdict.h"


static gboolean check_is_running(GtkWidget *widget, Window *xid)
{
	GdkScreen *gscreen;
	gchar selection_name[32];
	Atom selection_atom;

	gscreen = gtk_widget_get_screen(widget);
	g_snprintf(selection_name, sizeof(selection_name), XFCE_DICT_SELECTION"%d",
		gdk_screen_get_number(gscreen));
	selection_atom = XInternAtom(GDK_DISPLAY(), selection_name, False);

	if ((*xid = XGetSelectionOwner(GDK_DISPLAY(), selection_atom)))
		return TRUE;

	return FALSE;
}


gboolean dict_find_panel_plugin(gchar flags, const gchar *text)
{
	gboolean ret = FALSE;
	GdkEventClient gev;
	GtkWidget *win;
	Window id;

	win = gtk_invisible_new();
	gtk_widget_realize(win);

	gev.type = GDK_CLIENT_EVENT;
	gev.window = win->window;
	gev.send_event = TRUE;
	gev.message_type = gdk_atom_intern("STRING", FALSE);
	gev.data_format = 8;

	if (text == NULL)
		text = "";
	else if (strlen(text) > 12)
		g_warning("The passed search text is longer than 12 characters and was truncated. Currently you can pass only a maximum of 12 characters to the panel plugin (or even less when using non-ASCII characters).");

	/* format of the send string: "xfdict?text":
	 * "xfdict" is for identification of ourselves
	 * ? is a bitmask to control the behaviour, it can contain one or more of DICT_FLAGS_*,
	 * we send it as %c to ensure it takes only one char in the string,
	 * everything after this is the text to search, given on command line */
	/** FIXME: remove the limit of 12 characters, maybe by sending more than message or by
	  *        using another IPC mechanism, maybe DBus? */
	g_snprintf(gev.data.b, sizeof gev.data.b, "xfdict%c%s", flags, text);

	if (check_is_running(win, &id))
	{
		gdk_event_send_client_message((GdkEvent*) &gev, (GdkNativeWindow) id);
		ret = TRUE;
	}

	gdk_flush();
	gtk_widget_destroy(win);

	return ret;
}
