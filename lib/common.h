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


#ifndef COMMON_H
#define COMMON_H 1


/* Returns: TRUE if ptr points to a non-zero value. */
#define NZV(ptr) \
	((ptr) && (ptr)[0])


#define DICT_FLAGS_FOCUS_PANEL_ENTRY	1
#define DICT_FLAGS_MODE_DICT			2
#define DICT_FLAGS_MODE_WEB				4
#define DICT_FLAGS_MODE_SPELL			8

#define XFCE_DICT_SELECTION	"XFCE_DICT_SEL"


typedef enum
{
	DICTMODE_DICT = 0,
	DICTMODE_WEB,
	DICTMODE_SPELL,
	DICTMODE_LAST_USED
} dict_mode_t;

typedef enum
{
	WEBMODE_OTHER = 0,
	WEBMODE_LEO_GERENG,
	WEBMODE_LEO_GERFRE,
	WEBMODE_LEO_GERSPA,
	WEBMODE_LEO_GERITA,
	WEBMODE_LEO_GERCHI
} web_mode_t;


enum
{
	NO_CONNECTION,
	NO_ERROR
};


typedef struct
{
	/* settings */
	dict_mode_t mode_in_use;
	dict_mode_t mode_default;
	web_mode_t web_mode;

	gboolean show_panel_entry;
	gint panel_entry_size;

	gint port;
	gchar *server;
	gchar *dictionary;

	gchar *web_url;

	gchar *spell_bin;
	gchar *spell_dictionary;

	gboolean is_plugin;	/* specify whether the panel plugin loaded or not */

	/* status values */
	gchar *searched_word;  /* word to query the server */
	gboolean query_is_running;
	gint query_status;
	gchar *query_buffer;

	/* widgets */
	GtkWidget *window;
	GtkWidget *statusbar;
	GtkWidget *close_button;
	GtkWidget *close_menu_item;
	GtkWidget *pref_menu_item;
	GtkWidget *main_entry;
	GtkWidget *panel_entry;
	GtkWidget *main_textview;
	GtkTextBuffer *main_textbuffer;
	GtkTextTag *main_boldtag;
	GdkPixbuf *icon;

	GtkWidget *web_entry_box;
} DictData;


dict_mode_t dict_set_search_mode_from_flags(dict_mode_t mode, gchar flags);
void dict_free_data(DictData *dd);
void dict_write_rc_file(DictData *dd);
void dict_read_rc_file(DictData *dd);
void dict_search_word(DictData *dd, const gchar *word);
gboolean dict_open_browser(DictData *dd, const gchar *uri);
void dict_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
							 GtkSelectionData *data, guint info, guint ltime, DictData *dd);

DictData *dict_create_dictdata();


#endif
