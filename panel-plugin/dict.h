/*  $Id$
 *
 *  Copyright © 2007 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef DICT_H
#define DICT_H 1


// Returns: TRUE if ptr points to a non-zero value.
#define NZV(ptr) \
	((ptr) && (ptr)[0])


typedef enum
{
	DICTMODE_DICT = 0,
	DICTMODE_WEB,
	DICTMODE_SPELL
} _dict_mode;

typedef enum
{
	WEBMODE_OTHER,
	WEBMODE_LEO_GERENG,
	WEBMODE_LEO_GERFRE,
	WEBMODE_LEO_GERSPA
} _web_mode;


enum
{
	NO_CONNECTION,
	NO_ERROR
};


typedef struct
{
    XfcePanelPlugin *plugin;

    _dict_mode mode;
    _web_mode web_mode;

    GtkWidget *window;
    GtkWidget *statusbar;
    GtkWidget *main_entry;
	GtkWidget *panel_entry;
	GtkWidget *main_textview;
	GtkTextBuffer *main_textbuffer;
	GtkTextTag *main_boldtag;

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

	gchar *searched_word;  // word to query the server
	gboolean query_is_running;
	gint query_status;
	gchar *query_buffer;

	GtkWidget *web_entry_label;
	GtkWidget *web_entry;
	GtkWidget *web_radio_leo_gereng;
	GtkWidget *web_radio_leo_gerfre;
	GtkWidget *web_radio_leo_gerspa;
	GtkWidget *web_radio_other;
	gchar *web_url;

	GtkWidget *spell_entry;
    GtkWidget *spell_combo;
	gchar *spell_bin;
	gchar *spell_dictionary;

    GdkPixbuf *icon;
} DictData;



void dict_status_add(DictData *dd, const gchar *format, ...);
void dict_clear_text_buffer(DictData *dd);
void dict_start_aspell_query(DictData *dd, const gchar *word);

#endif
