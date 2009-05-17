/*  $Id$
 *
 *  Copyright 2009 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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

#include "common.h"
#include "wraplabel.h"
#include "speedreader.h"


typedef struct _XfdSpeedReaderPrivate			XfdSpeedReaderPrivate;

#define XFD_SPEED_READER_GET_PRIVATE(obj)		(G_TYPE_INSTANCE_GET_PRIVATE((obj),\
			XFD_SPEED_READER_TYPE, XfdSpeedReaderPrivate))

struct _XfdSpeedReaderPrivate
{
	GtkWidget *first_page;
	GtkWidget *second_page;

	GtkWidget *button_start;
	GtkWidget *button_stop;

	GtkWidget *spin_wpm;
	GtkWidget *button_font;
	GtkWidget *display_label;
	GtkTextBuffer *buffer;

	guint timer_id;
	gint word_idx;
	gint words_len;
	gchar **words;

	DictData *dd;
};

enum
{
	RESPONSE_START,
	RESPONSE_STOP,
};


G_DEFINE_TYPE(XfdSpeedReader, xfd_speed_reader, GTK_TYPE_DIALOG);

static void sr_stop(XfdSpeedReader *dialog);


static void xfd_speed_reader_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_XFD_SPEED_READER(object));

	sr_stop(XFD_SPEED_READER(object));

	G_OBJECT_CLASS(xfd_speed_reader_parent_class)->finalize(object);
}


static void xfd_speed_reader_class_init(XfdSpeedReaderClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = xfd_speed_reader_finalize;

	g_type_class_add_private((gpointer)klass, sizeof(XfdSpeedReaderPrivate));
}


/* Based on GLib's g_strsplit_set() but slightly modified to split exactly what we need for
 * speed reading (e.g. splitting but not removing dashes). */
static gchar **sr_strsplit_set(const gchar *string, const gchar *delimiters)
{
	gboolean delim_table[256];
	GSList *tokens, *list;
	gint n_tokens;
	guint x;
	const gchar *s;
	const gchar *current;
	gchar *token;
	gchar **result;

	g_return_val_if_fail(string != NULL, NULL);
	g_return_val_if_fail(delimiters != NULL, NULL);

	if (*string == '\0')
	{
		result = g_new(char *, 1);
		result[0] = NULL;
		return result;
	}

	memset(delim_table, FALSE, sizeof (delim_table));
	for (s = delimiters; *s != '\0'; ++s)
		delim_table[*(guchar *)s] = TRUE;

	tokens = NULL;
	n_tokens = 0;

	s = current = string;
	while (*s != '\0')
	{
		if (delim_table[*(guchar *)s])
		{
			x = (*s == '-') ? 1 : 0;
			token = g_strndup(current, s - current + x);
			tokens = g_slist_prepend(tokens, token);
			++n_tokens;

			current = s + 1;
		}
		++s;
	}

	token = g_strndup(current, s - current);
	tokens = g_slist_prepend(tokens, token);
	++n_tokens;

	result = g_new(gchar *, n_tokens + 1);

	result[n_tokens] = NULL;
	for (list = tokens; list != NULL; list = list->next)
		result[--n_tokens] = list->data;

	g_slist_free(tokens);

	return result;
}


static gchar *sr_replace_unicode_charatcers(const gchar *text)
{
	GString *str;
	gchar *result;
	gunichar c;

	if (! g_utf8_validate(text, -1, NULL))
		return g_strdup(text);

	str = g_string_new(NULL);

	while (*text)
	{
		c = g_utf8_get_char(text);

		switch (g_unichar_type(c))
		{
			case G_UNICODE_DASH_PUNCTUATION:
				g_string_append_c(str, '-');
				break;
			case G_UNICODE_SPACE_SEPARATOR:
				g_string_append_c(str, ' ');
				break;
			case G_UNICODE_PARAGRAPH_SEPARATOR:
			case G_UNICODE_LINE_SEPARATOR:
				g_string_append_c(str, '\n');
				break;
			default:
				g_string_append_unichar(str, c);
		}
		text = g_utf8_next_char(text);
	}

	result = g_string_free(str, (str->len == 0));

	return (result) ? result : g_strdup(text);
}


static gboolean sr_timer(gpointer data)
{
	XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(data);

	if (priv->word_idx >= priv->words_len)
	{
		sr_stop(XFD_SPEED_READER(data));
		return FALSE;
	}

	/* skip empty elements */
	while (priv->word_idx < priv->words_len && ! NZV(priv->words[priv->word_idx]))
		priv->word_idx++;

	if (priv->word_idx < priv->words_len && NZV(priv->words[priv->word_idx]))
	{
		gtk_label_set_text(GTK_LABEL(priv->display_label), priv->words[priv->word_idx]);
	}

	priv->word_idx++;

	return TRUE;
}


static void sr_start(XfdSpeedReader *dialog)
{
	XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(dialog);
	gint wpm;
	gint interval;
	const gchar *fontname;
	PangoFontDescription *pfd;
	gchar *text, *cleaned_text;
	GtkTextIter start, end;

	/* clear the label text */
	gtk_label_set_text(GTK_LABEL(priv->display_label), NULL);

	/* get the text */
	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(priv->buffer), &start);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(priv->buffer), &end);
	text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(priv->buffer), &start, &end, FALSE);
	if (! NZV(text))
	{
		gtk_dialog_response(GTK_DIALOG(dialog), RESPONSE_STOP);
		/* FIXME use an own error dialog implementation */
		xfce_err(_("You must enter a text."));
		return;
	}

	/* set the font */
	fontname = gtk_font_button_get_font_name(GTK_FONT_BUTTON(priv->button_font));
	pfd = pango_font_description_from_string(fontname);
	gtk_widget_modify_font(priv->display_label, pfd);
	pango_font_description_free(pfd);

	/* calculate the rate */
	wpm = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spin_wpm));
	interval = 60000 / wpm;

	/* save the settings */
	priv->dd->speedreader_wpm = wpm;
	g_free(priv->dd->speedreader_font);
	priv->dd->speedreader_font = g_strdup(fontname);

	/* prepare word list and start the timer */
	priv->word_idx = 0;
	cleaned_text = sr_replace_unicode_charatcers(text); /* replace Unicode dashes and spaces */
	priv->words = sr_strsplit_set(cleaned_text, " -_=\"\t\n\r");
	priv->words_len = g_strv_length(priv->words);

	priv->timer_id = g_timeout_add(interval, sr_timer, dialog);

	g_free(text);
	g_free(cleaned_text);
}


static void sr_stop(XfdSpeedReader *dialog)
{
	XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(dialog);

	if (priv->timer_id > 0)
	{
		g_source_remove(priv->timer_id);
		priv->timer_id = 0;

		g_strfreev(priv->words);
		priv->words = NULL;
	}
}


static void xfd_speed_reader_response_cb(XfdSpeedReader *dialog, gint response, gpointer data)
{
	XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(dialog);

	if (response == GTK_RESPONSE_CLOSE)
	{
		gtk_widget_destroy(GTK_WIDGET(dialog));
	}
	else if  (response == RESPONSE_START)
	{
		gtk_widget_hide(priv->button_start);
		gtk_widget_show(priv->button_stop);

		gtk_widget_hide(priv->first_page);
		gtk_widget_show(priv->second_page);

		sr_start(dialog);
	}
	else if  (response == RESPONSE_STOP)
	{
		gtk_widget_hide(priv->button_stop);
		gtk_widget_show(priv->button_start);

		gtk_widget_hide(priv->second_page);
		gtk_widget_show(priv->first_page);

		sr_stop(dialog);
	}
}


static void sr_open_clicked_cb(GtkButton *button, XfdSpeedReader *window)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Choose a file to load"),
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
		GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
		NULL);

	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_ACCEPT);
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_skip_taskbar_hint(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_type_hint(GTK_WINDOW(dialog), GDK_WINDOW_TYPE_HINT_DIALOG);
	gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(dialog), FALSE);
	gtk_file_chooser_set_local_only(GTK_FILE_CHOOSER(dialog), FALSE);

	if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
	{
		gchar *filename;
		gchar *text;
		gsize len;
		XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(window);

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (g_file_get_contents(filename, &text, &len, NULL))
		{
			gtk_text_buffer_set_text(GTK_TEXT_BUFFER(priv->buffer), text, len);
			g_free(text);
		}
		else
			xfce_err(_("The file '%s' could not be loaded."), filename);

		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}


static void sr_clear_clicked_cb(GtkButton *button, GtkTextBuffer *buffer)
{
	gtk_text_buffer_set_text(buffer, "", 0);
}


static void xfd_speed_reader_init(XfdSpeedReader *dialog)
{
	GtkWidget *label_intro, *label_words, *label_font;
	GtkWidget *vbox, *hbox_words, *hbox_font, *swin, *textview;
	GtkWidget *vbox_text_buttons, *hbox_text, *button_clear, *button_open;
	GtkSizeGroup *sizegroup;
	XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(dialog);

	gtk_window_set_title(GTK_WINDOW(dialog), _("Speed Reader"));
	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 300);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
	gtk_widget_set_name(GTK_WIDGET(dialog), "Xfce4Dict");

	/* First page */
	label_intro = xfd_wrap_label_new(
		_("This is an easy speed reading utility to help train you to read faster. "
		  "It does this by flashing words at a rapid rate on the screen."));

	label_words = gtk_label_new_with_mnemonic(_("_Words per minute:"));
	gtk_misc_set_alignment(GTK_MISC(label_words), 1, 0.5);

	priv->spin_wpm = gtk_spin_button_new_with_range(5.0, 10000.0, 5);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_words), priv->spin_wpm);

	hbox_words = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_words), label_words, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_words), priv->spin_wpm, FALSE, FALSE, 6);

	label_font = gtk_label_new_with_mnemonic(_("_Font Size:"));
	gtk_misc_set_alignment(GTK_MISC(label_font), 1, 0.5);

	priv->button_font = gtk_font_button_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_font), priv->button_font);

	hbox_font = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_font), label_font, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_font), priv->button_font, FALSE, FALSE, 6);

	sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget(sizegroup, label_words);
	gtk_size_group_add_widget(sizegroup, label_font);
	g_object_unref(G_OBJECT(sizegroup));

	textview = gtk_text_view_new();
	priv->buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(textview));
	gtk_text_buffer_set_text(priv->buffer,
		_("Enter some text here you would like to read.\n\n"
		  "Be relaxed and make yourself comfortable, then "
		  "press Start to begin speed reading."),
		-1);

	swin = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(swin), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(swin),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add(GTK_CONTAINER(swin), textview);

	button_open = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(button_open),
		gtk_image_new_from_stock(GTK_STOCK_OPEN, GTK_ICON_SIZE_MENU));
	g_signal_connect(button_open, "clicked", G_CALLBACK(sr_open_clicked_cb), dialog);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(button_open, _("Load the contents of a file"));
#endif

	button_clear = gtk_button_new();
	gtk_button_set_image(GTK_BUTTON(button_clear),
		gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
	g_signal_connect(button_clear, "clicked", G_CALLBACK(sr_clear_clicked_cb), priv->buffer);
#if GTK_CHECK_VERSION(2, 12, 0)
	gtk_widget_set_tooltip_text(button_clear, _("Clear the contents of the text field"));
#endif

	vbox_text_buttons = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox_text_buttons), button_open, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_text_buttons), button_clear, FALSE, FALSE, 0);

	hbox_text = gtk_hbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_text), swin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_text), vbox_text_buttons, FALSE, FALSE, 3);

	priv->button_start = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Start"), RESPONSE_START);
	priv->button_stop = gtk_dialog_add_button(GTK_DIALOG(dialog), _("St_op"), RESPONSE_STOP);
	gtk_dialog_add_button(GTK_DIALOG(dialog), GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

	gtk_widget_hide(priv->button_stop);

	gtk_button_set_image(GTK_BUTTON(priv->button_start),
		gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_MENU));
	gtk_button_set_image(GTK_BUTTON(priv->button_stop),
		gtk_image_new_from_stock(GTK_STOCK_MEDIA_STOP, GTK_ICON_SIZE_MENU));

	gtk_widget_grab_focus(textview);

	g_signal_connect(dialog, "response", G_CALLBACK(xfd_speed_reader_response_cb), NULL);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), label_intro, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_words, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_font, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_text, TRUE, TRUE, 0);

	priv->first_page = vbox;

	/* Second page */
	priv->display_label = gtk_label_new(NULL);
	gtk_widget_show(priv->display_label);

	vbox = gtk_vbox_new(FALSE, 6);
	gtk_box_pack_start(GTK_BOX(vbox), priv->display_label, TRUE, TRUE, 6);

	priv->second_page = vbox;

	gtk_widget_show_all(priv->first_page);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), priv->first_page, TRUE, TRUE, 6);
	gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), priv->second_page, TRUE, TRUE, 6);
}


GtkWidget *xfd_speed_reader_new(GtkWindow *parent, DictData *dd)
{
	GtkWidget *dialog = g_object_new(XFD_SPEED_READER_TYPE, "transient-for", parent, NULL);
	XfdSpeedReaderPrivate *priv = XFD_SPEED_READER_GET_PRIVATE(dialog);

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spin_wpm), dd->speedreader_wpm);
	gtk_font_button_set_font_name(GTK_FONT_BUTTON(priv->button_font), dd->speedreader_font);

	priv->dd = dd;

	return dialog;
}

