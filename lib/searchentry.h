/*  $Id$
 *
 *  Copyright 2006-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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



#ifndef __SEARCHENTRY_H__
#define __SEARCHENTRY_H__

G_BEGIN_DECLS

#define XFD_SEARCH_ENTRY_TYPE				(xfd_search_entry_get_type())
#define XFD_SEARCH_ENTRY(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			XFD_SEARCH_ENTRY_TYPE, XfdSearchEntry))
#define XFD_SEARCH_ENTRY_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			XFD_SEARCH_ENTRY_TYPE, XfdSearchEntryClass))
#define IS_XFD_SEARCH_ENTRY(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			XFD_SEARCH_ENTRY_TYPE))
#define IS_XFD_SEARCH_ENTRY_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			XFD_SEARCH_ENTRY_TYPE))

typedef struct _XfdSearchEntry				XfdSearchEntry;
typedef struct _XfdSearchEntryClass			XfdSearchEntryClass;

struct _XfdSearchEntry
{
	GtkComboBoxEntry parent;
};

struct _XfdSearchEntryClass
{
	GtkComboBoxEntryClass parent_class;
};

GType		xfd_search_entry_get_type		(void);
GtkWidget*	xfd_search_entry_new			(const gchar *text);
void		xfd_search_entry_prepend_text	(XfdSearchEntry *search_entry, const gchar *text);

G_END_DECLS

#endif /* __SEARCHENTRY_H__ */
