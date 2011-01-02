/*  $Id$
 *
 *  Copyright 2006-2011 Enrico Tröger <enrico(at)xfce(dot)org>
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

#include <stdio.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdlib.h>

#include <libxfce4util/libxfce4util.h>

#if HAVE_LOCALE_H
# include <locale.h>
#endif

#include "libdict.h"
#include "popup_plugin.h"


static gboolean show_version = FALSE;
static gboolean ignore_plugin = FALSE;
static gboolean use_clipboard = FALSE;
static gboolean focus_panel_entry = FALSE;
static gboolean mode_dict = FALSE;
static gboolean mode_web = FALSE;
static gboolean mode_spell = FALSE;
static gboolean verbose_mode = FALSE;

static GOptionEntry cli_options[] =
{
	{ "dict", 'd', 0, G_OPTION_ARG_NONE, &mode_dict, N_("Search the given text using a Dict server(RFC 2229)"), NULL },
	{ "web", 'w', 0, G_OPTION_ARG_NONE, &mode_web, N_("Search the given text using a web-based search engine"), NULL },
	{ "spell", 's', 0, G_OPTION_ARG_NONE, &mode_spell, N_("Check the given text with a spell checker"), NULL },
	{ "text-field", 't', 0, G_OPTION_ARG_NONE, &focus_panel_entry, N_("Grab the focus on the text field in the panel"), NULL },
	{ "ignore-plugin", 'i', 0, G_OPTION_ARG_NONE, &ignore_plugin, N_("Start stand-alone application even if the panel plugin is loaded"), NULL },
	{ "clipboard", 'c', 0, G_OPTION_ARG_NONE, &use_clipboard, N_("Grabs the PRIMARY selection content and uses it as search text"), NULL },
	{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &verbose_mode, N_("Be verbose"), NULL },
	{ "version", 'V', 0, G_OPTION_ARG_NONE, &show_version, N_("Show version information"), NULL },
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};


static gboolean main_quit(GtkWidget *widget, GdkEvent *event, DictData *dd)
{
	dict_gui_query_geometry(dd);
	dict_free_data(dd);
    gtk_main_quit();

    return FALSE;
}


static void close_button_clicked(GtkWidget *button, DictData *dd)
{
	main_quit(NULL, NULL, dd);
}


void pref_dialog_activated(GtkMenuItem *menuitem, DictData *dd)
{
	GtkWidget *dlg;

	dlg = dict_prefs_dialog_show(gtk_widget_get_toplevel(GTK_WIDGET(menuitem)), dd);
	gtk_widget_show(dlg);
}


static gchar get_flags()
{
	gchar flags = 0;

	if (focus_panel_entry)
		flags |= DICT_FLAGS_FOCUS_PANEL_ENTRY;
	if (mode_dict)
		flags |= DICT_FLAGS_MODE_DICT;
	if (mode_web)
		flags |= DICT_FLAGS_MODE_WEB;
	if (mode_spell)
		flags |= DICT_FLAGS_MODE_SPELL;

	return flags;
}


static gchar *get_search_text(gint count, gchar **values)
{
	GString *str = g_string_sized_new(128);
	gint i;

	for (i = 1; i < count; i++)
	{
		g_string_append(str, values[i]);
		if (i < (count - 1))
			g_string_append_c(str, ' ');
	}
	return g_string_free(str, FALSE);
}


gint main(gint argc, gchar *argv[])
{
	DictData *dd;
	GOptionContext *context;
	gchar flags;
	gchar *search_text;

#ifdef ENABLE_NLS
	xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

	context = g_option_context_new(_("[TEXT]"));
	g_option_context_add_main_entries(context, cli_options, GETTEXT_PACKAGE);
	g_option_group_set_translation_domain(g_option_context_get_main_group(context), GETTEXT_PACKAGE);
	g_option_context_add_group(context, gtk_get_option_group(FALSE));
	g_option_context_parse(context, &argc, &argv, NULL);

	gtk_init(&argc, &argv);
    gtk_window_set_default_icon_name("xfce4-dict");

	g_option_context_free(context);

	if (show_version)
	{
		g_print("%s %s (Xfce %s)\n\n", PACKAGE, PACKAGE_VERSION, xfce_version_string());
		g_print("%s\n", "Copyright (c) 2006-2011");
		g_print("\tEnrico Tröger <enrico@xfce.org>\n\n");
		g_print(_("Please report bugs to <%s>."), PACKAGE_BUGREPORT);
		g_print("\n");

		return EXIT_SUCCESS;
	}

	flags = get_flags();

	if (use_clipboard)
	{
		search_text = dict_get_clipboard_contents();
	}
	else
	{
		/* concatenate remaining command line arguments */
		search_text = get_search_text(argc, argv);
	}

	/* try to find an existing panel plugin and pop it up */
	if (! ignore_plugin && dict_find_panel_plugin(flags, search_text))
	{
		g_free(search_text);
		exit(0);
	}

	/* no plugin found, start stand-alone app */

	dd = dict_create_dictdata();
	dd->is_plugin = FALSE;
	dd->verbose_mode = verbose_mode;

	g_thread_init(NULL);

	dict_read_rc_file(dd);

	/* set search mode from command line flags, if any */
	dd->mode_in_use = dict_set_search_mode_from_flags(dd->mode_in_use, flags);

	dict_gui_create_main_window(dd);

	g_signal_connect(dd->window, "delete-event", G_CALLBACK(main_quit), dd);
	g_signal_connect(dd->close_button, "clicked", G_CALLBACK(close_button_clicked), dd);
	/* file menu */
	g_signal_connect(dd->close_menu_item, "activate", G_CALLBACK(close_button_clicked), dd);
	g_signal_connect(dd->pref_menu_item, "activate", G_CALLBACK(pref_dialog_activated), dd);

	/* search text from command line options, if any */
	if (NZV(search_text))
	{
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), search_text);
		dict_search_word(dd, search_text);
	}
	else
		dict_gui_status_add(dd, _("Ready"));

	g_free(search_text);

	gtk_widget_show_all(dd->window);

	gtk_main();

	return 0;
}
