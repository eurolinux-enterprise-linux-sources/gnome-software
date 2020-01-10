/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
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
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"

#include <glib.h>

#include "gs-plugin.h"

#define GS_PLUGIN_OS_RELEASE_FN		"/etc/os-release"

/**
 * gs_plugin_status_to_string:
 */
const gchar *
gs_plugin_status_to_string (GsPluginStatus status)
{
	if (status == GS_PLUGIN_STATUS_WAITING)
		return "waiting";
	if (status == GS_PLUGIN_STATUS_FINISHED)
		return "finished";
	if (status == GS_PLUGIN_STATUS_SETUP)
		return "setup";
	if (status == GS_PLUGIN_STATUS_DOWNLOADING)
		return "downloading";
	if (status == GS_PLUGIN_STATUS_QUERYING)
		return "querying";
	if (status == GS_PLUGIN_STATUS_INSTALLING)
		return "installing";
	if (status == GS_PLUGIN_STATUS_REMOVING)
		return "removing";
	return "unknown";
}

/**
 * gs_plugin_set_enabled:
 **/
void
gs_plugin_set_enabled (GsPlugin *plugin, gboolean enabled)
{
	plugin->enabled = enabled;
}

/**
 * gs_plugin_check_distro_id:
 **/
gboolean
gs_plugin_check_distro_id (GsPlugin *plugin, const gchar *distro_id)
{
	GError *error = NULL;
	gboolean ret;
	gchar *data = NULL;
	gchar *search = NULL;

	/* check that we are running on Fedora */
	ret = g_file_get_contents (GS_PLUGIN_OS_RELEASE_FN,
				   &data, NULL, &error);
	if (!ret) {
		g_warning ("%s could not be read: %s",
			   GS_PLUGIN_OS_RELEASE_FN,
			   error->message);
		g_error_free (error);
		goto out;
	}
	search = g_strdup_printf ("ID=%s\n", distro_id);
	if (g_strstr_len (data, -1, search) == NULL) {
		ret = FALSE;
		goto out;
	}
out:
	g_free (data);
	g_free (search);
	return ret;
}

/**
 * gs_plugin_add_app:
 **/
void
gs_plugin_add_app (GList **list, GsApp *app)
{
	g_return_if_fail (list != NULL);
	g_return_if_fail (GS_IS_APP (app));
	*list = g_list_prepend (*list, g_object_ref (app));
}

/**
 * gs_plugin_list_free:
 **/
void
gs_plugin_list_free (GList *list)
{
	g_list_free_full (list, (GDestroyNotify) g_object_unref);
}

/**
 * gs_plugin_list_filter:
 *
 * If func() returns TRUE for the GsApp, then the app is kept.
 **/
void
gs_plugin_list_filter (GList **list, GsPluginListFilter func, gpointer user_data)
{
	GList *l;
	GList *new = NULL;
	GsApp *app;

	g_return_if_fail (list != NULL);
	g_return_if_fail (func != NULL);

	/* see if any of the apps need filtering */
	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		if (func (app, user_data))
			gs_plugin_add_app (&new, app);
	}

	/* replace the list */
	gs_plugin_list_free (*list);
	*list = new;
}

/**
 * gs_plugin_list_randomize_cb:
 */
static gint
gs_plugin_list_randomize_cb (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const gchar *k1;
	const gchar *k2;
	gchar *key;

	key = g_strdup_printf ("Plugin::sort-key[%p]", user_data);
	k1 = gs_app_get_metadata_item (GS_APP (a), key);
	k2 = gs_app_get_metadata_item (GS_APP (b), key);
	g_free (key);
	return g_strcmp0 (k1, k2);
}

/**
 * gs_plugin_list_randomize:
 *
 * Randomize the order of the list, but don't change the order until the next day
 **/
void
gs_plugin_list_randomize (GList **list)
{
	GDateTime *date;
	GList *l;
	GRand *rand;
	GsApp *app;
	gchar *key;
	gchar sort_key[] = { '\0', '\0', '\0', '\0' };

	key = g_strdup_printf ("Plugin::sort-key[%p]", list);
	rand = g_rand_new ();
	date = g_date_time_new_now_utc ();
	g_rand_set_seed (rand, g_date_time_get_day_of_year (date));
	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		sort_key[0] = g_rand_int_range (rand, (gint32) 'A', (gint32) 'Z');
		sort_key[1] = g_rand_int_range (rand, (gint32) 'A', (gint32) 'Z');
		sort_key[2] = g_rand_int_range (rand, (gint32) 'A', (gint32) 'Z');
		gs_app_set_metadata (app, key, sort_key);
	}
	*list = g_list_sort_with_data (*list, gs_plugin_list_randomize_cb, list);
	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		gs_app_set_metadata (app, key, NULL);
	}
	g_free (key);
	g_rand_free (rand);
	g_date_time_unref (date);
}

/**
 * gs_plugin_list_filter_duplicates:
 **/
void
gs_plugin_list_filter_duplicates (GList **list)
{
	GHashTable *hash;
	GList *l;
	GList *new = NULL;
	GsApp *app;
	GsApp *found;
	const gchar *id;

	g_return_if_fail (list != NULL);

	/* create a new list with just the unique items */
	hash = g_hash_table_new (g_str_hash, g_str_equal);
	for (l = *list; l != NULL; l = l->next) {
		app = GS_APP (l->data);
		id = gs_app_get_id (app);
		if (id == NULL) {
			gs_plugin_add_app (&new, app);
			continue;
		}
		found = g_hash_table_lookup (hash, id);
		if (found == NULL) {
			gs_plugin_add_app (&new, app);
			g_hash_table_insert (hash, (gpointer) id,
					     GUINT_TO_POINTER (1));
			continue;
		}
		g_debug ("ignoring duplicate %s", id);
	}
	g_hash_table_unref (hash);

	/* replace the list */
	gs_plugin_list_free (*list);
	*list = new;
}

/**
 * gs_plugin_list_copy:
 **/
GList *
gs_plugin_list_copy (GList *list)
{
	return g_list_copy_deep (list, (GCopyFunc) g_object_ref, NULL);
}

typedef struct {
	GsPlugin	*plugin;
	GsApp		*app;
	GsPluginStatus	 status;
	guint		 percentage;
} GsPluginStatusHelper;

/**
 * gs_plugin_status_update_cb:
 **/
static gboolean
gs_plugin_status_update_cb (gpointer user_data)
{
	GsPluginStatusHelper *helper = (GsPluginStatusHelper *) user_data;

	/* call back into the loader */
	helper->plugin->status_update_fn (helper->plugin,
					  helper->app,
					  helper->status,
					  helper->plugin->status_update_user_data);
	if (helper->app != NULL)
		g_object_unref (helper->app);
	g_slice_free (GsPluginStatusHelper, helper);
	return FALSE;
}

/**
 * gs_plugin_status_update:
 **/
void
gs_plugin_status_update (GsPlugin *plugin, GsApp *app, GsPluginStatus status)
{
	GsPluginStatusHelper *helper;

	helper = g_slice_new0 (GsPluginStatusHelper);
	helper->plugin = plugin;
	helper->status = status;
	if (app != NULL)
		helper->app = g_object_ref (app);
	g_idle_add (gs_plugin_status_update_cb, helper);
}

/**
 * gs_plugin_progress_update_cb:
 **/
static gboolean
gs_plugin_progress_update_cb (gpointer user_data)
{
	GsPluginStatusHelper *helper = (GsPluginStatusHelper *) user_data;

	gs_app_set_progress (helper->app, helper->percentage);
	g_object_unref (helper->app);
	g_slice_free (GsPluginStatusHelper, helper);
	return FALSE;
}

/**
 * gs_plugin_progress_update:
 **/
void
gs_plugin_progress_update (GsPlugin *plugin, GsApp *app, guint percentage)
{
	GsPluginStatusHelper *helper;

	if (app == NULL)
		return;

	helper = g_slice_new0 (GsPluginStatusHelper);
	helper->plugin = plugin;
	helper->percentage = percentage;
	helper->app = g_object_ref (app);
	g_idle_add (gs_plugin_progress_update_cb, helper);
}

/**
 * gs_plugin_updates_changed_cb:
 **/
static gboolean
gs_plugin_updates_changed_cb (gpointer user_data)
{
	GsPlugin *plugin = GS_PLUGIN (user_data);
	plugin->updates_changed_fn (plugin, plugin->updates_changed_user_data);
	return FALSE;
}

/**
 * gs_plugin_updates_changed:
 **/
void
gs_plugin_updates_changed (GsPlugin *plugin)
{
	g_idle_add (gs_plugin_updates_changed_cb, plugin);
}

/* vim: set noexpandtab: */
