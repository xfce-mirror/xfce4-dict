/*  Copyright 2008-2011 Enrico Tröger <enrico(at)xfce(dot)org>
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


gboolean dict_find_panel_plugin(gchar flags, const gchar *text)
{
	gboolean  ret = FALSE;
	GError   *error = NULL;
	Dict     *proxy;

	proxy = dict_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
																			G_DBUS_PROXY_FLAGS_NONE,
																			"org.xfce.Dict",
																			"/org/xfce/Dict",
																			NULL,
																			&error);

	if (!proxy)
	{
		g_warning ("error connecting to org.xfce.Dict, reason was: %s", error->message);
		g_clear_error(&error);
		return FALSE;
	}

	ret = dict_call_search_sync (proxy, text, NULL, &error);

	if (error)
	{
		g_warning ("failed to connecting to org.xfce.Dict, reason was: %s", error->message);
		g_clear_error(&error);
		return FALSE;
	}

	return ret;
}
