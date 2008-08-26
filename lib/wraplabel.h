/*
 *      wraplabel.h
 *
 *      Copyright 2008 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
 *      Copyright 2008 Nick Treleaven <nick(dot)treleaven(at)btinternet(dot)com>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  $Id$
 */

#ifndef __WRAP_LABEL_H__
#define __WRAP_LABEL_H__

G_BEGIN_DECLS


#define WRAP_LABEL_TYPE				(wrap_label_get_type())
#define WRAP_LABEL(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj), WRAP_LABEL_TYPE, WrapLabel))
#define WRAP_LABEL_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass), WRAP_LABEL_TYPE, WrapLabelClass))
#define IS_WRAP_LABEL(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj), WRAP_LABEL_TYPE))
#define IS_WRAP_LABEL_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass), WRAP_LABEL_TYPE))


typedef struct _WrapLabel       WrapLabel;
typedef struct _WrapLabelClass  WrapLabelClass;

GType			wrap_label_get_type			(void);
GtkWidget*		wrap_label_new				(const gchar *text);
void			wrap_label_set_text			(GtkLabel *label, const gchar *text);


G_END_DECLS

#endif /* __WRAP_LABEL_H__ */
