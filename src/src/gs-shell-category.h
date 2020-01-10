/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more category.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __GS_SHELL_CATEGORY_H
#define __GS_SHELL_CATEGORY_H

#include <glib-object.h>
#include <gtk/gtk.h>

#include "gs-category.h"
#include "gs-shell.h"
#include "gs-plugin-loader.h"

G_BEGIN_DECLS

#define GS_TYPE_SHELL_CATEGORY		(gs_shell_category_get_type ())
#define GS_SHELL_CATEGORY(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GS_TYPE_SHELL_CATEGORY, GsShellCategory))
#define GS_SHELL_CATEGORY_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GS_TYPE_SHELL_CATEGORY, GsShellCategoryClass))
#define GS_IS_SHELL_CATEGORY(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GS_TYPE_SHELL_CATEGORY))
#define GS_IS_SHELL_CATEGORY_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GS_TYPE_SHELL_CATEGORY))
#define GS_SHELL_CATEGORY_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GS_TYPE_SHELL_CATEGORY, GsShellCategoryClass))

typedef struct GsShellCategoryPrivate GsShellCategoryPrivate;

typedef struct
{
	 GtkBin				 parent;
	 GsShellCategoryPrivate		*priv;
} GsShellCategory;

typedef struct
{
	GtkBinClass			 parent_class;
} GsShellCategoryClass;

GType		 gs_shell_category_get_type	(void);

GsShellCategory	*gs_shell_category_new		(void);
void		 gs_shell_category_set_category	(GsShellCategory	*shell_category,
						 GsCategory		*category);
GsCategory	*gs_shell_category_get_category (GsShellCategory	*shell_category);
void		 gs_shell_category_switch_to	(GsShellCategory	*shell_category);
void		 gs_shell_category_reload	(GsShellCategory	*shell_category);
void		 gs_shell_category_setup	(GsShellCategory	*shell_category,
						 GsShell		*shell,
						 GsPluginLoader		*plugin_loader,
						 GtkBuilder		*builder,
						 GCancellable		*cancellable);

#endif /* __GS_SHELL_CATEGORY_H */

/* vim: set noexpandtab: */
