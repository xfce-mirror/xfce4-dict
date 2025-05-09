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


#ifndef COMMON_H
#define COMMON_H 1


/* Returns: TRUE if ptr points to a non-zero value. */
#define NZV(ptr) \
	((ptr) && (ptr)[0])

/* G_GNUC_FALLTHROUGH definition, from GLib 2.68.0 */
#if ! GLIB_CHECK_VERSION (2, 60, 0)
#if __GNUC__ > 6
#define G_GNUC_FALLTHROUGH __attribute__((fallthrough))
#elif g_macro__has_attribute (fallthrough)
#define G_GNUC_FALLTHROUGH __attribute__((fallthrough))
#else
#define G_GNUC_FALLTHROUGH
#endif
#endif

#define DICT_FLAGS_FOCUS_PANEL_ENTRY	1
#define DICT_FLAGS_MODE_DICT			2
#define DICT_FLAGS_MODE_WEB				4
#define DICT_FLAGS_MODE_SPELL			8
#define DICT_FLAGS_MODE_LLM				16

#define XFCE_DICT_SELECTION	"XFCE_DICT_SEL"


typedef enum
{
	DICTMODE_DICT = 0,
	DICTMODE_WEB,
	DICTMODE_SPELL,
	DICTMODE_LAST_USED,
	DICTMODE_LLM
} dict_mode_t;


enum
{
	NO_ERROR,
	NO_CONNECTION,
	NOTHING_FOUND,
	NO_DATABASES,
	UNKNOWN_DATABASE,
	BAD_COMMAND,
	SERVER_NOT_READY
};

#define TAG_HEADING "heading"
#define TAG_ERROR "error"
#define TAG_SUCCESS "success"
#define TAG_LINK "link"
#define TAG_BOLD "bold"
#define TAG_PHONETIC "phonetic"

typedef struct
{
	/* settings */
	dict_mode_t mode_in_use;
	dict_mode_t mode_default;

	gboolean show_panel_entry;
	gint panel_entry_size;

	gchar *port;
	gchar *server;
	gchar *dictionary;

	gchar *web_url;

	gchar *spell_bin;
	gchar *spell_dictionary;

	gchar *llm_server;
	gchar *llm_port;
	gchar *llm_model;
	gchar *llm_prompt;

	gboolean verbose_mode;
	gboolean is_plugin;	/* specify whether the panel plugin loaded or not */

	/* status values */
	gchar *searched_word;  /* word to query the server */
	gboolean query_is_running;
	gint query_status;
	gchar *query_buffer;

	/* main window's geometry */
	gint geometry[5];

	/* widgets */
	GtkWidget *window;
	GtkWidget *statusbar;
	GtkWidget *cancel_query_button;
	GtkWidget *close_button;
	GtkWidget *close_menu_item;
	GtkWidget *pref_menu_item;
	GtkWidget *main_combo;
	GtkWidget *main_entry;
	GtkWidget *radio_button_dict;
	GtkWidget *radio_button_web;
	GtkWidget *radio_button_spell;
	GtkWidget *radio_button_llm;
	GtkWidget *panel_entry;
	GtkWidget *main_textview;
	GtkTextBuffer *main_textbuffer;
	GtkTextIter textiter;
	GtkTextTag *link_tag;
	GtkTextTag *phon_tag;
	GtkTextTag *error_tag;
	GtkTextTag *success_tag;
	GtkTextMark *mark_click;
	GdkPixbuf *icon;

	GdkRGBA *color_link;
	GdkRGBA *color_phonetic;
	GdkRGBA *color_correct;
	GdkRGBA *color_incorrect;

	/* speed reader */
	gint speedreader_wpm;
	gint speedreader_grouping;
	gchar *speedreader_font;
	gboolean speedreader_mark_paragraphs;
} DictData;


dict_mode_t dict_set_search_mode_from_flags(dict_mode_t mode, gchar flags);
void dict_free_data(DictData *dd);
void dict_write_rc_file(DictData *dd);
void dict_read_rc_file(DictData *dd);
void dict_search_word(DictData *dd, const gchar *word);
void dict_drag_data_received(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y,
							 GtkSelectionData *data, guint info, guint ltime, DictData *dd);

DictData *dict_create_dictdata(void);
gboolean  dict_start_web_query(DictData *dd, const gchar *word);
gchar    *dict_get_web_query_uri(DictData *dd, const gchar *word);
gchar    *dict_get_clipboard_contents(void);
void      dict_acquire_dbus_name(DictData *dd);

void dict_show_msgbox(DictData *dd, gint type, const gchar *text, ...) G_GNUC_PRINTF (3, 4);

gint open_socket(const gchar *host_name, const gchar *port);

#endif
