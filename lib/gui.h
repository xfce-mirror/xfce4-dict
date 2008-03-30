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


#ifndef GUI_H
#define GUI_H 1


void dict_status_add(DictData *dd, const gchar *format, ...);
void dict_create_main_window(DictData *dd);
void dict_about_dialog(GtkWidget *widget, DictData *dd);
void dict_clear_text_buffer(DictData *dd);
void dict_set_panel_entry_text(DictData *dd, const gchar *text);
const guint8 *dict_get_icon_data(void);


#endif