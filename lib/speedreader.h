/*  $Id$
 *
 *  Copyright 2009-2010 Enrico Tröger <enrico(dot)troeger(at)uvena(dot)de>
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


#ifndef __SPEEDREADER_H__
#define __SPEEDREADER_H__

G_BEGIN_DECLS

#define XFD_SPEED_READER_TYPE				(xfd_speed_reader_get_type())
#define XFD_SPEED_READER(obj)				(G_TYPE_CHECK_INSTANCE_CAST((obj),\
			XFD_SPEED_READER_TYPE, XfdSpeedReader))
#define XFD_SPEED_READER_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),\
			XFD_SPEED_READER_TYPE, XfdSpeedReaderClass))
#define IS_XFD_SPEED_READER(obj)			(G_TYPE_CHECK_INSTANCE_TYPE((obj),\
			XFD_SPEED_READER_TYPE))
#define IS_XFD_SPEED_READER_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),\
			XFD_SPEED_READER_TYPE))

typedef struct _XfdSpeedReader			XfdSpeedReader;
typedef struct _XfdSpeedReaderClass		XfdSpeedReaderClass;

struct _XfdSpeedReader
{
	GtkDialog parent;
};

struct _XfdSpeedReaderClass
{
	GtkDialogClass parent_class;
};

GType		xfd_speed_reader_get_type		(void);
GtkWidget*	xfd_speed_reader_new			(GtkWindow *parent, DictData *dd);

G_END_DECLS

#endif /* __SPEEDREADER_H__ */
