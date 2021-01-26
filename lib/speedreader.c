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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <libxfce4ui/libxfce4ui.h>

#include "common.h"
#include "speedreader.h"


typedef struct _XfdSpeedReaderPrivate			XfdSpeedReaderPrivate;

struct _XfdSpeedReaderPrivate
{
	GtkWidget *first_page;
	GtkWidget *second_page;

	GtkWidget *button_start;
	GtkWidget *button_stop;
	GtkWidget *button_pause;

	GtkWidget *spin_wpm;
	GtkWidget *spin_grouping;
	GtkWidget *button_font;
	GtkWidget *check_mark_paragraphs;
	GtkWidget *display_label;
	GtkTextBuffer *buffer;

	guint timer_id;
	guint word_idx;
	gsize words_len;
	gchar **words;

	GString *group;
	gsize group_size;

	gboolean paused;

	DictData *dd;
};

enum
{
	RESPONSE_START,
	RESPONSE_STOP,
	RESPONSE_PAUSE
};

enum
{
	XSR_STATE_INITIAL,
	XSR_STATE_RUNNING,
	XSR_STATE_FINISHED
};

#define XFD_TITLE_PAUSE _("P_ause")
#define XFD_TITLE_RESUME _("_Resume")


G_DEFINE_TYPE_WITH_PRIVATE(XfdSpeedReader, xfd_speed_reader, GTK_TYPE_DIALOG);

static void sr_stop(XfdSpeedReader *dialog);
static void sr_stop_timer(XfdSpeedReader *dialog);
static void sr_pause(XfdSpeedReader *dialog, gboolean paused);


static void xfd_speed_reader_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(IS_XFD_SPEED_READER(object));

	sr_stop_timer(XFD_SPEED_READER(object));

	G_OBJECT_CLASS(xfd_speed_reader_parent_class)->finalize(object);
}


static void xfd_speed_reader_class_init(XfdSpeedReaderClass *klass)
{
	GObjectClass *g_object_class;

	g_object_class = G_OBJECT_CLASS(klass);
	g_object_class->finalize = xfd_speed_reader_finalize;
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


static gchar *sr_replace_unicode_characters(const gchar *text, gboolean mark_paragraphs)
{
	GString *str;
	gchar *result;
	gunichar c = 0, last_c, x;
	gboolean last_line_was_empty = FALSE;

	g_return_val_if_fail(text != NULL, NULL);

	if (! g_utf8_validate(text, -1, NULL))
		return g_strdup(text);

	str = g_string_new(NULL);

	while (*text)
	{
		last_c = c;
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
				if (mark_paragraphs)
					g_string_append_unichar(str, 182); /* 182 = ¶ */
				/* intended fall-through */
				G_GNUC_FALLTHROUGH;
			case G_UNICODE_LINE_SEPARATOR:
				g_string_append_c(str, '\n');
				break;
			case G_UNICODE_CONTROL:
			{
				/* if not \n or \r, add it literally */
				if (! mark_paragraphs || strchr("\n\r", c) == NULL)
				{
					g_string_append_unichar(str, c);
				}
				else /* add pilcrows for paragraphs */
				{
					if ((last_c == '\r' && c == '\n') || /* CRLF */
						(last_c == '\r' && c != '\n') || /* CR */
						(last_c != '\r' && c == '\n'))   /* LF */
					{
						if (c == '\n')
							/* skip to the next character */
							x = g_utf8_get_char(g_utf8_next_char(text));
						else
							x = c;

						if (strchr("\r\n", x))
						{
							last_line_was_empty = TRUE;
						}
						else if (last_line_was_empty)
						{
							last_line_was_empty = FALSE;
							g_string_append(str, "¶\n");
						}
					}
					g_string_append_unichar(str, c);
				}
				break;
			}
			default:
				g_string_append_unichar(str, c);
		}
		text = g_utf8_next_char(text);
	}

	result = g_string_free(str, (str->len == 0));

	return (result) ? result : g_strdup(text);
}


static void xfd_speed_reader_set_window_title(XfdSpeedReader *dialog, gint state)
{
	gchar *title, *state_str, *name;
	const gchar *button_label = _("S_top");
	const gchar *button_image = "media-playback-stop";
	gboolean pausable = TRUE;
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	switch (state)
	{
		case XSR_STATE_RUNNING:
			state_str = _("Running");
			break;
		case XSR_STATE_FINISHED:
			state_str = _("Finished");
			button_label = _("_Back");
			button_image = "go-previous";
			pausable = FALSE;
			break;
		default:
			state_str = "";
	}

	name = _("Speed Reader");
	title = g_strdup_printf("%s%s%s", name, (NZV(state_str)) ? " - " : "", state_str);

	gtk_window_set_title(GTK_WINDOW(dialog), title);
	gtk_button_set_label(GTK_BUTTON(priv->button_stop), button_label);
	gtk_button_set_image(GTK_BUTTON(priv->button_stop),
		gtk_image_new_from_icon_name(button_image, GTK_ICON_SIZE_MENU));
	gtk_widget_set_sensitive(priv->button_pause, pausable);

	g_free(title);
}


static void sr_set_label_text(XfdSpeedReader *dialog)
{
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	if (NZV(priv->group->str))
		gtk_label_set_text(GTK_LABEL(priv->display_label), priv->group->str);
	g_string_erase(priv->group, 0, -1);
}


static gboolean sr_timer(gpointer data)
{
	gsize i;
	XfdSpeedReader *dialog = XFD_SPEED_READER(data);
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	if (priv->paused)
		return TRUE;

	if (priv->word_idx >= priv->words_len)
	{
		sr_stop(dialog);
		xfd_speed_reader_set_window_title(dialog, XSR_STATE_FINISHED);
		return FALSE;
	}

	for (i = 0; (i < priv->group_size) && (priv->word_idx < priv->words_len); i++)
	{
		/* skip empty elements */
		while (priv->word_idx < priv->words_len && ! NZV(priv->words[priv->word_idx]))
			priv->word_idx++;

		if (priv->word_idx < priv->words_len)
		{
			if (g_utf8_get_char(priv->words[priv->word_idx]) == 182)
			{	/* paragraph sign inside the group */
				g_string_append_unichar(priv->group, 182);
				sr_set_label_text(data);
				priv->word_idx++;
				return TRUE;
			}
			if ((priv->word_idx + 1) < priv->words_len &&
				g_utf8_get_char(priv->words[priv->word_idx + 1]) == 182)
			{	/* paragraph sign in the next group, so move it to this group */
				g_string_append(priv->group, priv->words[priv->word_idx]);
				g_string_append_unichar(priv->group, 182);
				sr_set_label_text(data);
				priv->word_idx += 2;
				return TRUE;
			}
			else
			{
				g_string_append(priv->group, priv->words[priv->word_idx]);
				if (i < (priv->group_size - 1))
					g_string_append_c(priv->group, ' ');
			}
		}
		priv->word_idx++;
	}
	sr_set_label_text(data);

	return TRUE;
}


static void sr_start(XfdSpeedReader *dialog)
{
	gint wpm, grouping;
	gint interval;
	gchar *fontname;
	gchar *text, *cleaned_text;
	GtkTextIter start, end;
	gchar *css;
	GtkCssProvider *provider;
	PangoFontDescription *font;

	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	/* clear the label text */
	gtk_label_set_text(GTK_LABEL(priv->display_label), NULL);

	/* get the text */
	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(priv->buffer), &start);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(priv->buffer), &end);
	text = gtk_text_buffer_get_text(GTK_TEXT_BUFFER(priv->buffer), &start, &end, FALSE);
	if (! NZV(text))
	{
		gtk_dialog_response(GTK_DIALOG(dialog), RESPONSE_STOP);
		dict_show_msgbox(priv->dd, GTK_MESSAGE_ERROR, _("You must enter a text."));
		return;
	}

	xfd_speed_reader_set_window_title(dialog, XSR_STATE_RUNNING);

	/* mark paragraphs? */
	priv->dd->speedreader_mark_paragraphs = gtk_toggle_button_get_active(
		GTK_TOGGLE_BUTTON(priv->check_mark_paragraphs));

	/* set the font */
	fontname = gtk_font_chooser_get_font(GTK_FONT_CHOOSER(priv->button_font));
	font = pango_font_description_from_string(fontname);

	if (G_LIKELY (font))
	{
		css = g_strdup_printf("label { font-family: %s; font-size: %dpt; font-style: %s; font-weight: %s }",
													pango_font_description_get_family (font),
													pango_font_description_get_size (font) / PANGO_SCALE,
													(pango_font_description_get_style(font) == PANGO_STYLE_ITALIC ||
													pango_font_description_get_style(font) == PANGO_STYLE_OBLIQUE) ? "italic" : "normal",
													(pango_font_description_get_weight(font) >= PANGO_WEIGHT_BOLD) ? "bold" : "normal");
		pango_font_description_free (font);
	}
	else
		css = g_strdup_printf("* { font: %s; }", fontname);

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_data (provider, css, -1, NULL);
	gtk_style_context_add_provider (
			GTK_STYLE_CONTEXT (gtk_widget_get_style_context (GTK_WIDGET (priv->display_label))),
			GTK_STYLE_PROVIDER(provider),
			GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
	g_free(css);

	/* word grouping */
	grouping = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spin_grouping));
	if (grouping >= 1 && grouping < 100) /* paranoia */
		priv->group_size = grouping;

	/* calculate the rate */
	wpm = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(priv->spin_wpm));
	if (wpm < 1)
		wpm = 400;
	interval = 60000 / wpm;

	/* save the settings */
	priv->dd->speedreader_wpm = wpm;
	priv->dd->speedreader_grouping = grouping;
	g_free(priv->dd->speedreader_font);
	priv->dd->speedreader_font = g_strdup(fontname);

	/* prepare word list and start the timer */
	priv->word_idx = 0;
	priv->group = g_string_new(NULL);
	/* replace Unicode dashes and spaces and mark paragraphs */
	cleaned_text = sr_replace_unicode_characters(text, priv->dd->speedreader_mark_paragraphs);
	priv->words = sr_strsplit_set(cleaned_text, " -_=\t\n\r");
	priv->words_len = g_strv_length(priv->words);

	priv->timer_id = g_timeout_add(interval, sr_timer, dialog);
	sr_pause(dialog, FALSE);

	g_free(text);
	g_free(cleaned_text);
	g_free(fontname);
}


static void sr_stop_timer(XfdSpeedReader *dialog)
{
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	if (priv->timer_id > 0)
	{
		g_source_remove(priv->timer_id);
		priv->timer_id = 0;

		g_string_free(priv->group, TRUE);
		priv->group = NULL;
		g_strfreev(priv->words);
		priv->words = NULL;
	}
}


static void sr_stop(XfdSpeedReader *dialog)
{
	sr_stop_timer(dialog);
	sr_pause(dialog, FALSE);
	xfd_speed_reader_set_window_title(dialog, XSR_STATE_INITIAL);
}


static void sr_pause(XfdSpeedReader *dialog, gboolean paused)
{
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	if (paused)
	{
		gtk_button_set_image(GTK_BUTTON(priv->button_pause),
			gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_MENU));
		gtk_button_set_label(GTK_BUTTON(priv->button_pause), XFD_TITLE_RESUME);
	}
	else
	{
		gtk_button_set_image(GTK_BUTTON(priv->button_pause),
			gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_MENU));
		gtk_button_set_label(GTK_BUTTON(priv->button_pause), XFD_TITLE_PAUSE);
	}
	/* set the new value */
	priv->paused = paused;
}


static void xfd_speed_reader_response_cb(XfdSpeedReader *dialog, gint response, gpointer data)
{
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	if (response == GTK_RESPONSE_CLOSE || response == GTK_RESPONSE_DELETE_EVENT)
	{
		gtk_widget_destroy(GTK_WIDGET(dialog));
	}
	else if (response == RESPONSE_START)
	{
		gtk_widget_hide(priv->button_start);
		gtk_widget_show(priv->button_stop);
		gtk_widget_show(priv->button_pause);

		gtk_widget_hide(priv->first_page);
		gtk_widget_show(priv->second_page);

		sr_start(dialog);
	}
	else if (response == RESPONSE_STOP)
	{
		gtk_widget_hide(priv->button_stop);
		gtk_widget_hide(priv->button_pause);
		gtk_widget_show(priv->button_start);

		gtk_widget_hide(priv->second_page);
		gtk_widget_show(priv->first_page);

		sr_stop(dialog);
	}
	else if (response == RESPONSE_PAUSE)
	{
		/* update the GUI */
		sr_pause(dialog, ! priv->paused);
	}
}


static void sr_open_clicked_cb(GtkButton *button, XfdSpeedReader *window)
{
	GtkWidget *dialog;

	dialog = gtk_file_chooser_dialog_new(_("Choose a file to load"),
		GTK_WINDOW(window),
		GTK_FILE_CHOOSER_ACTION_OPEN,
		_("_Cancel"), GTK_RESPONSE_CANCEL,
		_("_Open"), GTK_RESPONSE_ACCEPT,
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
		XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(window);

		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		if (g_file_get_contents(filename, &text, &len, NULL))
		{
			gtk_text_buffer_set_text(GTK_TEXT_BUFFER(priv->buffer), text, len);
			g_free(text);
		}
		else
			dict_show_msgbox(priv->dd, GTK_MESSAGE_ERROR,
				_("The file '%s' could not be loaded."), filename);

		g_free(filename);
	}
	gtk_widget_destroy(dialog);
}


static void sr_clear_clicked_cb(GtkButton *button, GtkTextBuffer *buffer)
{
	gtk_text_buffer_set_text(buffer, "", 0);
}


static void sr_paste_clicked_cb(GtkButton *button, GtkTextBuffer *buffer)
{
	GtkClipboard *clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
 	gtk_text_buffer_set_text(buffer, "", 0);
	gtk_text_buffer_paste_clipboard(buffer, clipboard, NULL, TRUE);
}


static void sr_spin_grouping_changed_cb(GtkSpinButton *button, GtkLabel *label)
{
	gint count = gtk_spin_button_get_value_as_int(button);
	gchar *text = g_strdup_printf(ngettext(
		"(display %d word at a time)",
		"(display %d words at a time)", count),
		count);

	gtk_label_set_text(label, text);
	g_free(text);
}


static void xfd_speed_reader_init(XfdSpeedReader *dialog)
{
	GtkWidget *label_intro, *label_words, *label_font, *label_grouping, *label_grouping_desc;
	GtkWidget *vbox, *hbox_words, *hbox_font, *hbox_grouping, *swin, *textview;
	GtkWidget *vbox_text_buttons, *hbox_text, *button_clear, *button_paste, *button_open, *button_close;
	GtkSizeGroup *sizegroup;
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(dialog);

	gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
	gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 330);
	gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CLOSE);
	gtk_widget_set_name(GTK_WIDGET(dialog), "Xfce4Dict");

	/* First page */
	label_intro = gtk_label_new(
		_("This is an easy speed reading utility to help train you to read faster. "
		  "It does this by flashing words at a rapid rate on the screen."));
	gtk_label_set_line_wrap (GTK_LABEL (label_intro), TRUE);
	gtk_label_set_line_wrap ( GTK_LABEL (label_intro), TRUE);

	label_words = gtk_label_new_with_mnemonic(_("_Words per Minute:"));
	gtk_widget_set_halign(label_words, GTK_ALIGN_END);
	gtk_widget_set_valign(label_words, GTK_ALIGN_CENTER);

	priv->spin_wpm = gtk_spin_button_new_with_range(5.0, 10000.0, 5);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_words), priv->spin_wpm);

	priv->check_mark_paragraphs = gtk_check_button_new_with_mnemonic(_("_Mark Paragraphs"));

	hbox_words = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox_words), label_words, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_words), priv->spin_wpm, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_words), priv->check_mark_paragraphs, FALSE, FALSE, 12);

	label_grouping = gtk_label_new_with_mnemonic(_("Word _Grouping:"));
	gtk_widget_set_halign(label_grouping, GTK_ALIGN_END);
	gtk_widget_set_valign(label_grouping, GTK_ALIGN_CENTER);

	label_grouping_desc = gtk_label_new(NULL);

	priv->spin_grouping = gtk_spin_button_new_with_range(1.0, 100.0, 1);
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_grouping), priv->spin_grouping);
	g_signal_connect(priv->spin_grouping, "value-changed",
		G_CALLBACK(sr_spin_grouping_changed_cb), label_grouping_desc);
	sr_spin_grouping_changed_cb(GTK_SPIN_BUTTON(priv->spin_grouping), GTK_LABEL(label_grouping_desc));

	hbox_grouping = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox_grouping), label_grouping, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_grouping), priv->spin_grouping, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_grouping), label_grouping_desc, FALSE, FALSE, 6);

	label_font = gtk_label_new_with_mnemonic(_("_Font Size:"));
	gtk_widget_set_halign(label_font, GTK_ALIGN_END);
	gtk_widget_set_valign(label_font, GTK_ALIGN_CENTER);

	priv->button_font = gtk_font_button_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label_font), priv->button_font);

	hbox_font = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox_font), label_font, FALSE, FALSE, 6);
	gtk_box_pack_start(GTK_BOX(hbox_font), priv->button_font, FALSE, FALSE, 6);

	sizegroup = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	gtk_size_group_add_widget(sizegroup, label_words);
	gtk_size_group_add_widget(sizegroup, label_grouping);
	gtk_size_group_add_widget(sizegroup, label_font);
	g_object_unref(G_OBJECT(sizegroup));

	textview = gtk_text_view_new();
	gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(textview), GTK_WRAP_WORD);
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

	button_open = gtk_button_new_from_icon_name("document-open", GTK_ICON_SIZE_MENU);
	g_signal_connect(button_open, "clicked", G_CALLBACK(sr_open_clicked_cb), dialog);
	gtk_widget_set_tooltip_text(button_open, _("Load the contents of a file"));

	button_paste = gtk_button_new_from_icon_name("edit-paste", GTK_ICON_SIZE_MENU);
	g_signal_connect(button_paste, "clicked", G_CALLBACK(sr_paste_clicked_cb), priv->buffer);
	gtk_widget_set_tooltip_text(button_paste,
		_("Clear the contents of the text field and paste the contents of the clipboard"));

	button_clear = gtk_button_new_from_icon_name("edit-clear", GTK_ICON_SIZE_MENU);
	g_signal_connect(button_clear, "clicked", G_CALLBACK(sr_clear_clicked_cb), priv->buffer);
	gtk_widget_set_tooltip_text(button_clear, _("Clear the contents of the text field"));

	vbox_text_buttons = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox_text_buttons), button_open, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_text_buttons), button_paste, FALSE, FALSE, 0);
	gtk_box_pack_start(GTK_BOX(vbox_text_buttons), button_clear, FALSE, FALSE, 0);

	hbox_text = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(hbox_text), swin, TRUE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX(hbox_text), vbox_text_buttons, FALSE, FALSE, 3);

	priv->button_pause = gtk_dialog_add_button(GTK_DIALOG(dialog), _("P_ause"), RESPONSE_PAUSE);
	priv->button_start = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Start"), RESPONSE_START);
	priv->button_stop = gtk_dialog_add_button(GTK_DIALOG(dialog), _("S_top"), RESPONSE_STOP);
	button_close = gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Close"), GTK_RESPONSE_CLOSE);

	gtk_widget_hide(priv->button_pause);
	gtk_widget_hide(priv->button_stop);

	gtk_button_set_image(GTK_BUTTON(priv->button_pause),
		gtk_image_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_MENU));
	gtk_button_set_image(GTK_BUTTON(priv->button_start),
		gtk_image_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_MENU));
	gtk_button_set_image(GTK_BUTTON(priv->button_stop),
		gtk_image_new_from_icon_name("media-playback-stop", GTK_ICON_SIZE_MENU));
	gtk_button_set_image(GTK_BUTTON(button_close),
		gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU));

	g_signal_connect(dialog, "response", G_CALLBACK(xfd_speed_reader_response_cb), NULL);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), label_intro, FALSE, FALSE, 5);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_words, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_grouping, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_font, FALSE, FALSE, 3);
	gtk_box_pack_start(GTK_BOX(vbox), hbox_text, TRUE, TRUE, 0);

	priv->first_page = vbox;

	/* Second page */
	priv->display_label = gtk_label_new(NULL);
	gtk_widget_show(priv->display_label);

	vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	gtk_box_pack_start(GTK_BOX(vbox), priv->display_label, TRUE, TRUE, 6);

	priv->second_page = vbox;

	gtk_widget_show_all(priv->first_page);

	gtk_widget_grab_focus(textview);

	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), priv->first_page, TRUE, TRUE, 6);
	gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), priv->second_page, TRUE, TRUE, 6);

	xfd_speed_reader_set_window_title(dialog, XSR_STATE_INITIAL);
}


GtkWidget *xfd_speed_reader_new(GtkWindow *parent, DictData *dd)
{
	GtkWidget *dialog = g_object_new(XFD_SPEED_READER_TYPE, "transient-for", parent, NULL);
	XfdSpeedReaderPrivate *priv = xfd_speed_reader_get_instance_private(XFD_SPEED_READER (dialog));

	gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spin_wpm), dd->speedreader_wpm);
	gtk_spin_button_set_value(GTK_SPIN_BUTTON(priv->spin_grouping), dd->speedreader_grouping);
	gtk_font_chooser_set_font(GTK_FONT_CHOOSER(priv->button_font), dd->speedreader_font);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(priv->check_mark_paragraphs),
		dd->speedreader_mark_paragraphs);

	priv->dd = dd;

	return dialog;
}
