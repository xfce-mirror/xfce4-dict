/*  Copyright 2006-2012 Enrico Tröger <enrico(at)xfce(dot)org>
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


/* Creation of main window and helper functions (GUI stuff). */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdarg.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <libxfce4util/libxfce4util.h>

#include "common.h"
#include "gui.h"
#include "resources.h"
#include "speedreader.h"



static gboolean hovering_over_link = FALSE;
static GdkCursor *hand_cursor = NULL;
static GdkCursor *regular_cursor = NULL;
static gboolean entry_is_dirty = FALSE;


/* all textview_* functions are from the gtk-demo app to get links in the textview working */
static gchar *textview_get_hyperlink_at_iter(GtkWidget *text_view, GtkTextIter *iter, DictData *dd)
{
	GSList *tags = NULL, *tagp = NULL;
	gchar *found_link = NULL;
	gchar *result = NULL;

	tags = gtk_text_iter_get_tags(iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
	{
		GtkTextTag *tag = tagp->data;

		found_link = g_object_get_data(G_OBJECT(tag), "link");
		if (found_link != NULL)
		{
			result = g_strdup(found_link);
			break;
		}
		g_object_get(G_OBJECT(tag), "name", &found_link, NULL);
		if (found_link != NULL)
		{
			if (strcmp("link", found_link) == 0)
			{
				result = dict_get_web_query_uri(dd, dd->searched_word);
				break;
			}
			g_free(found_link);
		}
	}
	if (tags)
		g_slist_free(tags);

	return result;
}


static void textview_follow_if_link(GtkWidget *text_view, GtkTextIter *iter, DictData *dd)
{
	GSList *tags = NULL, *tagp = NULL;

	tags = gtk_text_iter_get_tags(iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
	{
		GtkTextTag *tag = tagp->data;
		gchar *found_link;

		found_link = g_object_get_data(G_OBJECT(tag), "link");
		if (found_link != NULL)
		{
			gtk_entry_set_text(GTK_ENTRY(dd->main_entry), found_link);
			dict_search_word(dd, found_link);
			break;
		}
		g_object_get(G_OBJECT(tag), "name", &found_link, NULL);
		if (found_link != NULL && strcmp("link", found_link) == 0)
		{
			gboolean browser_started = dict_start_web_query(dd, dd->searched_word);
			if (browser_started && dd->is_plugin)
				gtk_widget_hide(dd->window);

			gdk_window_set_cursor(gtk_text_view_get_window(
					GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_TEXT), regular_cursor);
			g_free(found_link);
			break;
		}
	}
	if (tags)
		g_slist_free(tags);
}


static gboolean textview_key_press_event(GtkWidget *text_view, GdkEventKey *event, DictData *dd)
{
	GtkTextIter iter;

	switch (event->keyval)
	{
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
		{
			gtk_text_buffer_get_iter_at_mark(dd->main_textbuffer, &iter,
				gtk_text_buffer_get_insert(dd->main_textbuffer));
			textview_follow_if_link(text_view, &iter, dd);
			break;
		}
		default:
			break;
	}

	return FALSE;
}


static gboolean textview_event_after(GtkWidget *text_view, GdkEvent *ev, DictData *dd)
{
	GtkTextIter start, end, iter;
	GdkEventButton *event;
	gint x, y;

	if (ev->type != GDK_BUTTON_RELEASE)
		return FALSE;

	event = (GdkEventButton *)ev;

	if (event->button != 1)
		return FALSE;

	/* we shouldn't follow a link if the user has selected something */
	gtk_text_buffer_get_selection_bounds(dd->main_textbuffer, &start, &end);
	if (gtk_text_iter_get_offset(&start) != gtk_text_iter_get_offset(&end))
		return FALSE;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_WIDGET,
		event->x, event->y, &x, &y);

	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(text_view), &iter, x, y);

	textview_follow_if_link(text_view, &iter, dd);

	return FALSE;
}


static void textview_set_cursor_if_appropriate(GtkTextView *view, gint x, gint y, GdkWindow *win)
{
	GSList *tags = NULL, *tagp = NULL;
	GtkTextIter iter;
	gboolean hovering = FALSE;

	gtk_text_view_get_iter_at_location(view, &iter, x, y);

	tags = gtk_text_iter_get_tags(&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
	{
		GtkTextTag *tag = tagp->data;
		gchar *name;

		if (g_object_get_data(G_OBJECT(tag), "link") != NULL)
		{
			hovering = TRUE;
			break;
		}
		g_object_get(G_OBJECT(tag), "name", &name, NULL);
		if (name != NULL && strcmp("link", name) == 0)
		{
			hovering = TRUE;
			g_free(name);
			break;
		}
		g_free(name);
	}

	if (hovering != hovering_over_link)
	{
		hovering_over_link = hovering;

		if (hovering_over_link)
			gdk_window_set_cursor(win, hand_cursor);
		else
			gdk_window_set_cursor(win, regular_cursor);
	}

	if (tags)
		g_slist_free(tags);
}


static gboolean textview_motion_notify_event(GtkWidget *text_view, GdkEventMotion *event)
{
	gint x, y;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view), GTK_TEXT_WINDOW_WIDGET,
		event->x, event->y, &x, &y);

	textview_set_cursor_if_appropriate(GTK_TEXT_VIEW(text_view), x, y, event->window);

	return FALSE;
}


static gboolean textview_visibility_notify_event(GtkWidget *text_view, GdkEventVisibility *event)
{
	gint wx, wy, bx, by;
	GdkDevice *pointer;
	GdkSeat *seat;

	seat = gdk_display_get_default_seat (gdk_display_get_default ());
	pointer = gdk_seat_get_pointer (seat);

	gdk_window_get_device_position(gtk_widget_get_window (text_view),
								   pointer, &wx, &wy, NULL);

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(text_view),
		GTK_TEXT_WINDOW_WIDGET, wx, wy, &bx, &by);

	textview_set_cursor_if_appropriate(GTK_TEXT_VIEW(text_view), bx, by, event->window);

	return FALSE;
}


static gchar *textview_get_text_at_cursor(DictData *dd)
{
	gchar *word;
	GtkTextIter start, end;

	if (! gtk_text_buffer_get_selection_bounds(dd->main_textbuffer, &start, &end))
	{
		gint wx, wy, bx, by;
		GdkDevice *pointer;
		GdkSeat *seat;

		seat = gdk_display_get_default_seat (gdk_display_get_default ());
		pointer = gdk_seat_get_pointer (seat);

		gdk_window_get_device_position(gtk_widget_get_window (dd->main_textview),
									   pointer, &wx, &wy, NULL);

		gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(dd->main_textview),
			GTK_TEXT_WINDOW_WIDGET, wx, wy, &bx, &by);

		gtk_text_buffer_get_iter_at_mark(dd->main_textbuffer, &start, dd->mark_click);
		if (! gtk_text_iter_starts_word(&start))
			gtk_text_iter_backward_word_start(&start);

		end = start;

		if (gtk_text_iter_inside_word(&end))
			gtk_text_iter_forward_word_end(&end);
	}

	word = gtk_text_buffer_get_text(dd->main_textbuffer, &start, &end, FALSE);

	return word;
}


static void textview_popup_search_item_cb(GtkWidget *widget, DictData *dd)
{
	gchar *word;

	word = textview_get_text_at_cursor(dd);

	if (word != NULL)
	{
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), word);
		dict_search_word(dd, word);
		gtk_widget_grab_focus(dd->main_entry);

		g_free(word);
	}
}


static gboolean textview_is_text_at_cursor(DictData *dd)
{
	gchar *text;

	text = textview_get_text_at_cursor(dd);
	if (text != NULL)
	{
		gboolean non_empty_text = NZV(text);
		g_free(text);
		return non_empty_text;
	}
	else
		return FALSE;
}


static void textview_popup_copylink_item_cb(GtkWidget *widget, DictData *dd)
{
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	GtkTextIter iter;
	gchar *hyperlink;

	gtk_text_buffer_get_iter_at_mark(dd->main_textbuffer, &iter, dd->mark_click);
	hyperlink = textview_get_hyperlink_at_iter(dd->main_textview, &iter, dd);
	if (hyperlink != NULL)
	{
		gtk_clipboard_set_text(clipboard, hyperlink, -1);
		g_free(hyperlink);
	}
}


static gboolean textview_is_hyperlink_at_cursor(DictData *dd)
{
	GtkTextIter iter;
	gchar *hyperlink;

	gtk_text_buffer_get_iter_at_mark(dd->main_textbuffer, &iter, dd->mark_click);
	hyperlink = textview_get_hyperlink_at_iter(dd->main_textview, &iter, dd);
	if (hyperlink != NULL)
	{
		g_free(hyperlink);
		return TRUE;
	}
	else
		return FALSE;
}



static void textview_populate_popup_cb(GtkTextView *textview, GtkMenu *menu, DictData *dd)
{
	GtkWidget *icon;
	GtkWidget *search;
	GtkWidget *copy_link;
	GtkWidget *sep = gtk_separator_menu_item_new();

	gtk_widget_show(sep);
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), sep);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	copy_link = gtk_image_menu_item_new_with_mnemonic (_("Copy Link"));
	icon = gtk_image_new_from_icon_name ("edit-copy-symbolic", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (copy_link), icon);
G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_widget_show_all(copy_link);
	gtk_widget_set_sensitive(GTK_WIDGET(copy_link), textview_is_hyperlink_at_cursor(dd));
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), copy_link);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	search = gtk_image_menu_item_new_with_mnemonic (_("Search"));
	icon = gtk_image_new_from_icon_name ("edit-find-symbolic", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (search), icon);
G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_widget_show_all(search);
	gtk_widget_set_sensitive(GTK_WIDGET(search), textview_is_text_at_cursor(dd));
	gtk_menu_shell_prepend(GTK_MENU_SHELL(menu), search);

	g_signal_connect(search, "activate", G_CALLBACK(textview_popup_search_item_cb), dd);
	g_signal_connect(copy_link, "activate", G_CALLBACK(textview_popup_copylink_item_cb), dd);
}


static gboolean textview_button_press_cb(GtkTextView *view, GdkEventButton *event, DictData *dd)
{
	if (event->button == 3)
	{
		gint x, y;
		GtkTextIter iter;

		gtk_text_view_window_to_buffer_coords(view, GTK_TEXT_WINDOW_TEXT, event->x, event->y, &x, &y);
		gtk_text_view_get_iter_at_location(view, &iter, x, y);
		gtk_text_buffer_move_mark(dd->main_textbuffer, dd->mark_click, &iter);

		gdk_window_set_cursor(event->window, regular_cursor);
	}

	return FALSE;
}


static gboolean textview_query_tooltip_cb(GtkWidget *widget, gint x, gint y, gboolean keyboard_mode,
										  GtkTooltip *tooltip, DictData *dd)
{
	gint tx, ty;
	GSList *tags = NULL, *tagp = NULL;
	GtkTextIter iter;

	gtk_text_view_window_to_buffer_coords(GTK_TEXT_VIEW(widget),
		GTK_TEXT_WINDOW_WIDGET, x, y, &tx, &ty);

	gtk_text_view_get_iter_at_location(GTK_TEXT_VIEW(widget), &iter, tx, ty);

	tags = gtk_text_iter_get_tags(&iter);
	for (tagp = tags;  tagp != NULL;  tagp = tagp->next)
	{
		GtkTextTag *tag = tagp->data;
		gchar *name;

		g_object_get(G_OBJECT(tag), "name", &name, NULL);
		if (name != NULL && strcmp("link", name) == 0)
		{
			gchar *target_uri = dict_get_web_query_uri(dd, dd->searched_word);
			gchar *target_uri_escaped = g_markup_escape_text (target_uri, -1);
			gtk_tooltip_set_markup(tooltip, target_uri_escaped);
			g_free(name);
			g_free(target_uri);
			g_free(target_uri_escaped);
			return TRUE;
		}
		g_free(name);
	}
	return FALSE;
}


static void textview_apply_or_remove_tags(GtkTextBuffer *buffer, const gchar *tag,
										  GtkTextIter *start, GtkTextIter *end)
{
	g_return_if_fail(tag != NULL);

	if (*tag == '\0')
		gtk_text_buffer_remove_all_tags(buffer, start, end);
	else
		gtk_text_buffer_apply_tag_by_name(buffer, tag, start, end);
}


/* An empty string as tag name will clear all tags on the given word */
void dict_gui_textview_apply_tag_to_word(GtkTextBuffer *buffer, const gchar *word,
										 GtkTextIter *pos, const gchar *first_tag, ...)
{
	GtkTextIter start, end;
	gchar *s;

	g_return_if_fail(word != NULL);
	g_return_if_fail(first_tag != NULL);

	/* This is a bit ugly but works:
	 * we search the passed word backwards from 'pos' and apply the tag to it. */
	if (gtk_text_iter_backward_search(pos, word, GTK_TEXT_SEARCH_TEXT_ONLY, &start, &end, NULL))
	{
		va_list args;

		textview_apply_or_remove_tags(buffer, first_tag, &start, &end);

		va_start(args, first_tag);
		for (; s = va_arg(args, gchar*), s != NULL;)
		{
			textview_apply_or_remove_tags(buffer, s, &start, &end);
		}
		va_end(args);
	}
}


void dict_gui_set_panel_entry_text(DictData *dd, const gchar *text)
{
	if (dd->panel_entry != NULL)
		gtk_entry_set_text(GTK_ENTRY(dd->panel_entry), text);
}


void dict_gui_status_add(DictData *dd, const gchar *format, ...)
{
	static gchar string[512];
	va_list args;

	string[0] = ' ';
	va_start(args, format);
	g_vsnprintf(string + 1, (sizeof string) - 1, format, args);
	va_end(args);

	gtk_statusbar_pop(GTK_STATUSBAR(dd->statusbar), 1);
	gtk_statusbar_push(GTK_STATUSBAR(dd->statusbar), 1, string);
	if (dd->verbose_mode)
		g_message("%s", string);
}


void dict_gui_clear_text_buffer(DictData *dd)
{
	GtkTextIter end_iter;

	gtk_text_buffer_get_start_iter(dd->main_textbuffer, &dd->textiter);
	gtk_text_buffer_get_end_iter(dd->main_textbuffer, &end_iter);
	gtk_text_buffer_delete(dd->main_textbuffer, &dd->textiter, &end_iter);

	gtk_widget_grab_focus(dd->main_entry);
}


static void entry_activate_cb(GtkEntry *entry, DictData *dd)
{
	const gchar *entered_text = gtk_entry_get_text(GTK_ENTRY(dd->main_entry));

	dict_search_word(dd, entered_text);
}


static void entry_icon_release_cb(GtkEntry *entry, GtkEntryIconPosition icon_pos,
		GdkEventButton *event, DictData *dd)
{
	if (event->button != 1)
		return;

	if (icon_pos == GTK_ENTRY_ICON_PRIMARY)
	{
		entry_activate_cb(NULL, dd);
		gtk_widget_grab_focus(dd->main_entry);
	}
	else if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
	{
		dict_gui_clear_text_buffer(dd);
		gtk_entry_set_text(GTK_ENTRY(dd->main_entry), "");
		dict_gui_set_panel_entry_text(dd, "");
		dict_gui_status_add(dd, _("Ready"));
	}
}


static void combo_changed_cb(GtkComboBox *combo, DictData *dd)
{
	GtkTreeIter iter;

	if (gtk_combo_box_get_active_iter(combo, &iter))
	{
		gchar *text = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
		dict_search_word(dd, text);
		g_free(text);
	}
}


static void entry_changed_cb(GtkEditable *editable, DictData *dd)
{
	entry_is_dirty = TRUE;
}


static void entry_button_clicked_cb(GtkButton *button, DictData *dd)
{
	entry_activate_cb(NULL, dd);
	gtk_widget_grab_focus(dd->main_entry);
}


static gboolean entry_button_press_cb(GtkWidget *widget, GdkEventButton *event, DictData *dd)
{
	if (! entry_is_dirty)
	{
		entry_is_dirty = TRUE;
		if (event->button == 1)
			gtk_entry_set_text(GTK_ENTRY(widget), "");
	}

	return FALSE;
}


static void update_search_button(DictData *dd, GtkWidget *box)
{
	static GtkWidget *button = NULL;
	GtkWidget *image = NULL;

	if (button == NULL)
	{
		button = gtk_button_new_with_mnemonic (_("F_ind"));
		gtk_button_set_image(GTK_BUTTON(button),
			gtk_image_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_BUTTON));
		gtk_widget_show(button);
		gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
		g_signal_connect(button, "clicked", G_CALLBACK(entry_button_clicked_cb), dd);
	}

	switch (dd->mode_in_use)
	{
		case DICTMODE_DICT:
		case DICTMODE_WEB:
		{
			image = gtk_image_new_from_icon_name("edit-find-symbolic", GTK_ICON_SIZE_BUTTON);
			break;
		}
		case DICTMODE_SPELL:
		{
			image = gtk_image_new_from_icon_name("tools-check-spelling-symbolic", GTK_ICON_SIZE_BUTTON);
			break;
		}
		default:
			break;
	}
	if (image != NULL)
	{
		gtk_button_set_image(GTK_BUTTON(button), image);
	}
}


static void search_mode_dict_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode_in_use = DICTMODE_DICT;
		gtk_widget_grab_focus(dd->main_entry);
		update_search_button(dd, NULL);
	}
}


static void search_mode_web_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode_in_use = DICTMODE_WEB;
		gtk_widget_grab_focus(dd->main_entry);
		update_search_button(dd, NULL);
	}
}


static void search_mode_spell_toggled(GtkToggleButton *togglebutton, DictData *dd)
{
	if (gtk_toggle_button_get_active(togglebutton))
	{
		dd->mode_in_use = DICTMODE_SPELL;
		gtk_widget_grab_focus(dd->main_entry);
		update_search_button(dd, NULL);
	}
}


static void speedreader_clicked_cb(GtkButton *button, DictData *dd)
{
	GtkWidget *dialog = xfd_speed_reader_new(GTK_WINDOW(dd->window), dd);
	gtk_widget_show(dialog);
}


static GtkWidget *create_file_menu(DictData *dd)
{
	GtkWidget *icon;
	GtkWidget *menubar, *file, *file_menu, *help, *help_menu, *menu_item;

	GtkAccelGroup *accel_group = gtk_accel_group_new();
	gtk_window_add_accel_group(GTK_WINDOW(dd->window), accel_group);

	menubar = gtk_menu_bar_new();

	/* File Menu */
	file = gtk_menu_item_new_with_mnemonic(_("_File"));

	file_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(file), file_menu);

	/* Speed Reader */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("Speed _Reader"));
	icon = gtk_image_new_from_icon_name ("format-justify-center-symbolic", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), icon);
G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_widget_add_accelerator (menu_item, "activate", accel_group,
                                GDK_KEY_r, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	g_signal_connect(menu_item, "activate", G_CALLBACK(speedreader_clicked_cb), dd);
	gtk_container_add(GTK_CONTAINER(file_menu), menu_item);

	/* Separator */
	gtk_container_add(GTK_CONTAINER(file_menu), gtk_separator_menu_item_new());

	/* Preferences */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	dd->pref_menu_item = gtk_image_menu_item_new_with_mnemonic (_("_Preferences"));
	icon = gtk_image_new_from_icon_name ("preferences-system-symbolic", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (dd->pref_menu_item), icon);
G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_widget_add_accelerator (dd->pref_menu_item, "activate", accel_group,
								GDK_KEY_p, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(file_menu), dd->pref_menu_item);

	/* Separator */
	gtk_container_add(GTK_CONTAINER(file_menu), gtk_separator_menu_item_new());

	/* Close */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	dd->close_menu_item = gtk_image_menu_item_new_with_mnemonic (dd->is_plugin ? _("_Close") : _("_Quit"));
	icon = gtk_image_new_from_icon_name (dd->is_plugin ? "window-close-symbolic" : "application-exit-symbolic", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (dd->close_menu_item), icon);
G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_widget_add_accelerator (dd->close_menu_item, "activate", accel_group,
								GDK_KEY_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
	gtk_container_add(GTK_CONTAINER(file_menu), dd->close_menu_item);

	/* Help Menu*/
	help = gtk_menu_item_new_with_mnemonic(_("_Help"));

	help_menu = gtk_menu_new();
	gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), help_menu);

	/* About */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
	menu_item = gtk_image_menu_item_new_with_mnemonic (_("About"));
	icon = gtk_image_new_from_icon_name ("help-about-symbolic", GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menu_item), icon);
G_GNUC_END_IGNORE_DEPRECATIONS

	gtk_container_add(GTK_CONTAINER(help_menu), menu_item);
	g_signal_connect(menu_item, "activate", G_CALLBACK(dict_gui_about_dialog), dd);

	gtk_container_add(GTK_CONTAINER(menubar), file);
	gtk_container_add(GTK_CONTAINER(menubar), help);

	gtk_widget_show_all(menubar);

	return menubar;
}


void dict_gui_finalize(DictData *dd)
{
	if (hand_cursor)
		g_object_unref (hand_cursor);
	if (regular_cursor)
		g_object_unref (regular_cursor);
}


void dict_gui_create_main_window(DictData *dd)
{
	GtkWidget *main_box, *entry_box, *label_box;
	GtkWidget *sep, *scrolledwindow_results;
	GdkPixbuf *icon;
	GtkWidget *method_chooser, *radio, *label, *button;
	GtkAccelGroup *accel_group = gtk_accel_group_new();

	dd->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(dd->window), _("Dictionary"));
	gtk_window_set_default_size(GTK_WINDOW(dd->window), 580, 360);
	gtk_widget_set_name(dd->window, "Xfce4Dict");

	dict_get_resource();
	icon = gdk_pixbuf_new_from_resource("/org/xfce/dict/icon", NULL);
	gtk_window_set_icon(GTK_WINDOW(dd->window), icon);
	g_object_unref(icon);

	main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_widget_show(main_box);
	gtk_container_add(GTK_CONTAINER(dd->window), main_box);

	gtk_box_pack_start(GTK_BOX(main_box), create_file_menu(dd), FALSE, TRUE, 0);

	/* entry box (label, entry, buttons) */
	entry_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_show(entry_box);
	gtk_container_set_border_width(GTK_CONTAINER(entry_box), 2);
	gtk_box_pack_start(GTK_BOX(main_box), entry_box, FALSE, TRUE, 5);

	label_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 5);
	gtk_widget_show(label_box);
	gtk_box_pack_start(GTK_BOX(entry_box), label_box, TRUE, TRUE, 5);

	dd->main_combo = gtk_combo_box_text_new_with_entry();
	gtk_widget_show(dd->main_combo);
	gtk_box_pack_start(GTK_BOX(label_box), dd->main_combo, TRUE, TRUE, 0);
	g_signal_connect(dd->main_combo, "changed", G_CALLBACK(combo_changed_cb), dd);

	dd->main_entry = gtk_bin_get_child(GTK_BIN(dd->main_combo));
	gtk_entry_set_placeholder_text(GTK_ENTRY(dd->main_entry), _("Search term"));
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(dd->main_entry), GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
	gtk_entry_set_icon_from_icon_name(GTK_ENTRY(dd->main_entry), GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");
	g_signal_connect(dd->main_entry, "changed", G_CALLBACK(entry_changed_cb), dd);
	g_signal_connect(dd->main_entry, "activate", G_CALLBACK(entry_activate_cb), dd);
	g_signal_connect(dd->main_entry, "icon-release", G_CALLBACK(entry_icon_release_cb), dd);
	g_signal_connect(dd->main_entry, "button-press-event", G_CALLBACK(entry_button_press_cb), dd);

	update_search_button(dd, entry_box);

	sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(sep);
	gtk_box_pack_start(GTK_BOX(entry_box), sep, FALSE, FALSE, 2);

	button = gtk_button_new_with_mnemonic(_("Speed _Reader"));
	gtk_button_set_image(GTK_BUTTON(button),
		gtk_image_new_from_icon_name("format-justify-center-symbolic", GTK_ICON_SIZE_MENU));
	g_signal_connect(button, "clicked", G_CALLBACK(speedreader_clicked_cb), dd);
	gtk_widget_show(button);
	gtk_box_pack_start(GTK_BOX(entry_box), button, FALSE, FALSE, 2);

	sep = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
	gtk_widget_show(sep);
	gtk_box_pack_start(GTK_BOX(entry_box), sep, FALSE, FALSE, 2);

	dd->close_button = gtk_button_new_with_mnemonic ((dd->is_plugin) ?
		_("_Close") : _("_Quit"));
	gtk_button_set_image(GTK_BUTTON(dd->close_button),
		gtk_image_new_from_icon_name((dd->is_plugin) ?
			"window-close-symbolic" : "application-exit-symbolic", GTK_ICON_SIZE_BUTTON));
	gtk_widget_show(dd->close_button);
	gtk_box_pack_end(GTK_BOX(entry_box), dd->close_button, FALSE, FALSE, 0);

	/* search method chooser */
	method_chooser = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_widget_show(method_chooser);
	gtk_box_pack_start(GTK_BOX(main_box), method_chooser, FALSE, FALSE, 0);

	label = gtk_label_new(_("Search with:"));
	gtk_widget_show(label);
	gtk_box_pack_start(GTK_BOX(method_chooser), label, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_mnemonic(NULL, _("_Dictionary Server"));
	dd->radio_button_dict = radio;
	gtk_widget_show(radio);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode_in_use == DICTMODE_DICT));
	g_signal_connect(radio, "toggled", G_CALLBACK(search_mode_dict_toggled), dd);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio), _("_Web Service"));
	dd->radio_button_web = radio;
	gtk_widget_set_sensitive(dd->radio_button_web, NZV(dd->web_url));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode_in_use == DICTMODE_WEB));
	g_signal_connect(radio, "toggled", G_CALLBACK(search_mode_web_toggled), dd);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	radio = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(radio), _("_Spell Checker"));
	dd->radio_button_spell = radio;
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio), (dd->mode_in_use == DICTMODE_SPELL));
	g_signal_connect(radio, "toggled", G_CALLBACK(search_mode_spell_toggled), dd);
	gtk_widget_show(radio);
	gtk_box_pack_start(GTK_BOX(method_chooser), radio, FALSE, FALSE, 6);

	/* results area */
	scrolledwindow_results = gtk_scrolled_window_new(NULL, NULL);
	gtk_widget_show(scrolledwindow_results);
	gtk_container_set_border_width(GTK_CONTAINER(scrolledwindow_results), 4);
	gtk_box_pack_start(GTK_BOX(main_box), scrolledwindow_results, TRUE, TRUE, 0);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolledwindow_results),
								GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolledwindow_results),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	/* searched words textview stuff */
	dd->main_textview = gtk_text_view_new();
	gtk_widget_set_name(dd->window, "Xfce4DictTextView");
	gtk_text_view_set_editable(GTK_TEXT_VIEW(dd->main_textview), FALSE);
	gtk_text_view_set_left_margin(GTK_TEXT_VIEW(dd->main_textview), 5);
	gtk_text_view_set_right_margin(GTK_TEXT_VIEW(dd->main_textview), 5);
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(dd->main_textview), GTK_WRAP_WORD);
	dd->main_textbuffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(dd->main_textview));
	gtk_text_buffer_create_tag(dd->main_textbuffer,
			TAG_HEADING,
			"weight", PANGO_WEIGHT_BOLD,
			"pixels-below-lines", 5, NULL);
	gtk_text_buffer_create_tag(dd->main_textbuffer,
			TAG_BOLD,
			"weight", PANGO_WEIGHT_BOLD,
			"indent", 10,
			"style", PANGO_STYLE_ITALIC,
			"pixels-below-lines", 5, NULL);
	dd->error_tag = gtk_text_buffer_create_tag(dd->main_textbuffer,
			TAG_ERROR,
			"style", PANGO_STYLE_ITALIC,
			"foreground-rgba", dd->color_incorrect, NULL);
	dd->success_tag = gtk_text_buffer_create_tag(dd->main_textbuffer,
			TAG_SUCCESS,
			"foreground-rgba", dd->color_correct, NULL);
	dd->phon_tag = gtk_text_buffer_create_tag(dd->main_textbuffer,
			TAG_PHONETIC,
			"style", PANGO_STYLE_ITALIC,
			"foreground-rgba", dd->color_phonetic, NULL);
	dd->link_tag = gtk_text_buffer_create_tag(dd->main_textbuffer,
			TAG_LINK,
			"underline", PANGO_UNDERLINE_SINGLE,
			"foreground-rgba", dd->color_link, NULL);

	/* support for links (cross-references) for dictd responses */
	{
		hand_cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "pointer");
		regular_cursor = gdk_cursor_new_from_name(gdk_display_get_default(), "default");

		g_signal_connect(dd->main_textview, "key-press-event",
			G_CALLBACK(textview_key_press_event), dd);
		g_signal_connect(dd->main_textview, "event-after",
			G_CALLBACK(textview_event_after), dd);
		g_signal_connect(dd->main_textview, "motion-notify-event",
			G_CALLBACK(textview_motion_notify_event), NULL);
		g_signal_connect(dd->main_textview, "visibility-notify-event",
			G_CALLBACK(textview_visibility_notify_event), NULL);
	}
	/* support for 'Search' and 'Copy Link' menu items in the textview popup menu */
	{
		GtkTextIter start;
		gtk_text_buffer_get_bounds(dd->main_textbuffer, &start, &start);
		dd->mark_click = gtk_text_buffer_create_mark(dd->main_textbuffer, NULL, &start, TRUE);

		g_signal_connect(dd->main_textview, "button-press-event",
			G_CALLBACK(textview_button_press_cb), dd);
		g_signal_connect(dd->main_textview, "populate-popup",
			G_CALLBACK(textview_populate_popup_cb), dd);
	}
	/* tooltips */
	gtk_widget_set_has_tooltip(dd->main_textview, TRUE);
	g_signal_connect(dd->main_textview, "query-tooltip", G_CALLBACK(textview_query_tooltip_cb), dd);

	gtk_widget_show(dd->main_textview);
	gtk_container_add(GTK_CONTAINER(scrolledwindow_results), dd->main_textview);

	/* status bar */
	dd->statusbar = gtk_statusbar_new();
	gtk_widget_show(dd->statusbar);
	gtk_box_pack_end(GTK_BOX(main_box), dd->statusbar, FALSE, FALSE, 0);

	/* DnD */
	g_signal_connect(dd->main_entry, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);
	/* TODO: find a way to get this working, the textview doesn't receive anything as long as it isn't
	 * editable. scrolledwindow_results and a surrounding event box as receivers also don't work. */
	g_signal_connect(dd->main_textview, "drag-data-received", G_CALLBACK(dict_drag_data_received), dd);

	/* use the saved window geometry */
	if (dd->geometry[0] != -1)
	{
		gtk_window_move(GTK_WINDOW(dd->window), dd->geometry[0], dd->geometry[1]);
		gtk_window_set_default_size(GTK_WINDOW(dd->window), dd->geometry[2], dd->geometry[3]);
		if (dd->geometry[4] == 1)
			gtk_window_maximize(GTK_WINDOW(dd->window));
	}
	/* quit on Escape */
	gtk_widget_add_accelerator(dd->close_button, "clicked", accel_group, GDK_KEY_Escape, 0, 0);
	gtk_window_add_accel_group(GTK_WINDOW(dd->window), accel_group);
}


void dict_gui_show_main_window(DictData *dd)
{
	gtk_widget_show(dd->window);
	gtk_window_deiconify(GTK_WINDOW(dd->window));
	gtk_window_present(GTK_WINDOW(dd->window));
}


void dict_gui_about_dialog(GtkWidget *widget, DictData *dd)
{
	const gchar *authors[]= { "Enrico Tröger <enrico@xfce.org>",
                            "Harald Judt <hjudt@xfce.org>",
                            "André Miranda <andre42m@gmail.com>",
                            NULL };

	gtk_show_about_dialog(GTK_WINDOW(dd->window),
		"destroy-with-parent", TRUE,
		"authors", authors,
		"comments", _("A client program to query different dictionaries."),
		"copyright", "Copyright \302\251 2006-2023 Xfce Development Team",
		"website", "https://docs.xfce.org/apps/xfce4-dict/start",
		"logo-icon-name", "org.xfce.Dictionary",
		"translator-credits", _("translator-credits"),
		"license", XFCE_LICENSE_GPL,
		"version", PACKAGE_VERSION,
		"program-name", _("Xfce4 Dictionary"),
		  NULL);
}


void dict_gui_query_geometry(DictData *dd)
{
	gtk_window_get_position(GTK_WINDOW(dd->window),	&dd->geometry[0], &dd->geometry[1]);
	gtk_window_get_size(GTK_WINDOW(dd->window),	&dd->geometry[2], &dd->geometry[3]);

	if (gdk_window_get_state(gtk_widget_get_window(dd->window)) & GDK_WINDOW_STATE_MAXIMIZED)
		dd->geometry[4] = 1;
	else
		dd->geometry[4] = 0;
}
