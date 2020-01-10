/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2013-2014 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2013 Matthias Clasen <mclasen@redhat.com>
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

/**
 * SECTION:gs-app
 * @short_description: An application that is either installed or that can be installed
 *
 * This object represents a 1:1 mapping to a .desktop file. The design is such
 * so you can't have different GsApp's for different versions or architectures
 * of a package. This rule really only applies to GsApps of kind GS_APP_KIND_NORMAL
 * and GS_APP_KIND_SYSTEM. We allow GsApps of kind GS_APP_KIND_SYSTEM_UPDATE or
 * GS_APP_KIND_PACKAGE, which don't correspond to desktop files, but instead
 * represent a system update and its individual components.
 *
 * The #GsPluginLoader de-duplicates the GsApp instances that are produced by
 * plugins to ensure that there is a single instance of GsApp for each id, making
 * the id the primary key for this object. This ensures that actions triggered on
 * a GsApp in different parts of gnome-software can be observed by connecting to
 * signals on the GsApp.
 *
 * Information about other #GsApp objects can be stored in this object, for
 * instance in the gs_app_add_related() method or gs_app_get_history().
 */

#include "config.h"

#include <string.h>
#include <gtk/gtk.h>

#include "gs-app.h"
#include "gs-utils.h"

static void	gs_app_finalize	(GObject	*object);

struct GsAppPrivate
{
	gchar			*id;
	gchar			*name;
	GsAppQuality		 name_quality;
	gchar			*icon;
	gchar			*icon_path;
	GPtrArray		*sources;
	GPtrArray		*source_ids;
	gchar			*project_group;
	gchar			*version;
	gchar			*version_ui;
	gchar			*summary;
	GsAppQuality		 summary_quality;
	gchar			*summary_missing;
	gchar			*description;
	GsAppQuality		 description_quality;
	GPtrArray		*screenshots;
	GPtrArray		*categories;
	GPtrArray		*keywords;
	GHashTable		*urls;
	gchar			*licence;
	gchar			*menu_path;
	gchar			*origin;
	gchar			*update_version;
	gchar			*update_version_ui;
	gchar			*update_details;
	gchar			*management_plugin;
	gint			 rating;
	gint			 rating_confidence;
	GsAppRatingKind		 rating_kind;
	guint64			 size;
	GsAppKind		 kind;
	AsIdKind		 id_kind;
	AsAppState		 state;
	guint			 progress;
	GHashTable		*metadata;
	GdkPixbuf		*pixbuf;
	GdkPixbuf		*featured_pixbuf;
	GPtrArray		*addons; /* of GsApp */
	GHashTable		*addons_hash; /* of "id" */
	GPtrArray		*related; /* of GsApp */
	GHashTable		*related_hash; /* of "id-source" */
	GPtrArray		*history; /* of GsApp */
	guint64			 install_date;
	guint64			 kudos;
	gboolean		 to_be_installed;
};

enum {
	PROP_0,
	PROP_ID,
	PROP_NAME,
	PROP_VERSION,
	PROP_SUMMARY,
	PROP_DESCRIPTION,
	PROP_RATING,
	PROP_KIND,
	PROP_STATE,
	PROP_PROGRESS,
	PROP_INSTALL_DATE,
	PROP_LAST
};

G_DEFINE_TYPE_WITH_PRIVATE (GsApp, gs_app, G_TYPE_OBJECT)

/**
 * gs_app_error_quark:
 * Return value: Our personal error quark.
 **/
GQuark
gs_app_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("gs_app_error");
	return quark;
}

/**
 * gs_app_kind_to_string:
 **/
const gchar *
gs_app_kind_to_string (GsAppKind kind)
{
	if (kind == GS_APP_KIND_UNKNOWN)
		return "unknown";
	if (kind == GS_APP_KIND_NORMAL)
		return "normal";
	if (kind == GS_APP_KIND_SYSTEM)
		return "system";
	if (kind == GS_APP_KIND_PACKAGE)
		return "package";
	if (kind == GS_APP_KIND_OS_UPDATE)
		return "os-update";
	if (kind == GS_APP_KIND_MISSING)
		return "missing";
	if (kind == GS_APP_KIND_SOURCE)
		return "source";
	if (kind == GS_APP_KIND_CORE)
		return "core";
	return NULL;
}

/**
 * gs_app_to_string:
 **/
gchar *
gs_app_to_string (GsApp *app)
{
	AsImage *im;
	AsScreenshot *ss;
	GList *keys;
	GList *l;
	GString *str;
	GsAppPrivate *priv = app->priv;
	const gchar *tmp;
	guint i;

	g_return_val_if_fail (GS_IS_APP (app), NULL);

	str = g_string_new ("GsApp:\n");
	g_string_append_printf (str, "\tkind:\t%s\n",
				gs_app_kind_to_string (priv->kind));
	if (priv->id_kind != AS_ID_KIND_UNKNOWN) {
		g_string_append_printf (str, "\tid-kind:\t%s\n",
					as_id_kind_to_string (priv->id_kind));
	}
	g_string_append_printf (str, "\tstate:\t%s\n",
				as_app_state_to_string (priv->state));
	if (priv->progress > 0)
		g_string_append_printf (str, "\tprogress:\t%i%%\n", priv->progress);
	if (priv->id != NULL)
		g_string_append_printf (str, "\tid:\t%s\n", priv->id);
	if ((priv->kudos & GS_APP_KUDO_MY_LANGUAGE) > 0)
		g_string_append (str, "\tkudo:\tmy-language\n");
	if ((priv->kudos & GS_APP_KUDO_RECENT_RELEASE) > 0)
		g_string_append (str, "\tkudo:\trecent-release\n");
	if ((priv->kudos & GS_APP_KUDO_FEATURED_RECOMMENDED) > 0)
		g_string_append (str, "\tkudo:\tfeatured-recommended\n");
	if ((priv->kudos & GS_APP_KUDO_MODERN_TOOLKIT) > 0)
		g_string_append (str, "\tkudo:\tmodern-toolkit\n");
	if ((priv->kudos & GS_APP_KUDO_SEARCH_PROVIDER) > 0)
		g_string_append (str, "\tkudo:\tsearch-provider\n");
	if ((priv->kudos & GS_APP_KUDO_INSTALLS_USER_DOCS) > 0)
		g_string_append (str, "\tkudo:\tinstalls-user-docs\n");
	if ((priv->kudos & GS_APP_KUDO_USES_NOTIFICATIONS) > 0)
		g_string_append (str, "\tkudo:\tuses-notifications\n");
	if ((priv->kudos & GS_APP_KUDO_USES_APP_MENU) > 0)
		g_string_append (str, "\tkudo:\tuses-app-menu\n");
	if ((priv->kudos & GS_APP_KUDO_HAS_KEYWORDS) > 0)
		g_string_append (str, "\tkudo:\thas-keywords\n");
	if ((priv->kudos & GS_APP_KUDO_HAS_SCREENSHOTS) > 0)
		g_string_append (str, "\tkudo:\thas-screenshots\n");
	if ((priv->kudos & GS_APP_KUDO_POPULAR) > 0)
		g_string_append (str, "\tkudo:\tpopular\n");
	if ((priv->kudos & GS_APP_KUDO_IBUS_HAS_SYMBOL) > 0)
		g_string_append (str, "\tkudo:\tibus-has-symbol\n");
	if ((priv->kudos & GS_APP_KUDO_PERFECT_SCREENSHOTS) > 0)
		g_string_append (str, "\tkudo:\tperfect-screenshots\n");
	if ((priv->kudos & GS_APP_KUDO_HIGH_CONTRAST) > 0)
		g_string_append (str, "\tkudo:\thigh-contrast\n");
	g_string_append_printf (str, "\tkudo-percentage:\t%i\n",
				gs_app_get_kudos_percentage (app));
	if (priv->name != NULL)
		g_string_append_printf (str, "\tname:\t%s\n", priv->name);
	if (priv->icon != NULL)
		g_string_append_printf (str, "\ticon:\t%s\n", priv->icon);
	if (priv->icon_path != NULL)
		g_string_append_printf (str, "\ticon-path:\t%s\n", priv->icon_path);
	if (priv->version != NULL)
		g_string_append_printf (str, "\tversion:\t%s\n", priv->version);
	if (priv->version_ui != NULL)
		g_string_append_printf (str, "\tversion-ui:\t%s\n", priv->version_ui);
	if (priv->update_version != NULL)
		g_string_append_printf (str, "\tupdate-version:\t%s\n", priv->update_version);
	if (priv->update_version_ui != NULL)
		g_string_append_printf (str, "\tupdate-version-ui:\t%s\n", priv->update_version_ui);
	if (priv->update_details != NULL) {
		g_string_append_printf (str, "\tupdate-details:\t%s\n",
					priv->update_details);
	}
	if (priv->summary != NULL)
		g_string_append_printf (str, "\tsummary:\t%s\n", priv->summary);
	if (priv->description != NULL)
		g_string_append_printf (str, "\tdescription:\t%s\n", priv->description);
	for (i = 0; i < priv->screenshots->len; i++) {
		ss = g_ptr_array_index (priv->screenshots, i);
		tmp = as_screenshot_get_caption (ss, NULL);
		im = as_screenshot_get_image (ss, 0, 0);
		if (im == NULL)
			continue;
		g_string_append_printf (str, "\tscreenshot-%02i:\t%s [%s]\n",
					i, as_image_get_url (im),
					tmp != NULL ? tmp : "<none>");
	}
	for (i = 0; i < priv->sources->len; i++) {
		tmp = g_ptr_array_index (priv->sources, i);
		g_string_append_printf (str, "\tsource-%02i:\t%s\n", i, tmp);
	}
	for (i = 0; i < priv->source_ids->len; i++) {
		tmp = g_ptr_array_index (priv->source_ids, i);
		g_string_append_printf (str, "\tsource-id-%02i:\t%s\n", i, tmp);
	}
	tmp = g_hash_table_lookup (priv->urls, as_url_kind_to_string (AS_URL_KIND_HOMEPAGE));
	if (tmp != NULL)
		g_string_append_printf (str, "\turl{homepage}:\t%s\n", tmp);
	if (priv->licence != NULL)
		g_string_append_printf (str, "\tlicence:\t%s\n", priv->licence);
	if (priv->summary_missing != NULL)
		g_string_append_printf (str, "\tsummary-missing:\t%s\n", priv->summary_missing);
	if (priv->menu_path != NULL && priv->menu_path[0] != '\0')
		g_string_append_printf (str, "\tmenu-path:\t%s\n", priv->menu_path);
	if (priv->origin != NULL && priv->origin[0] != '\0')
		g_string_append_printf (str, "\torigin:\t%s\n", priv->origin);
	if (priv->rating != -1)
		g_string_append_printf (str, "\trating:\t%i\n", priv->rating);
	if (priv->rating_confidence != -1)
		g_string_append_printf (str, "\trating-confidence:\t%i\n", priv->rating_confidence);
	if (priv->rating_kind != GS_APP_RATING_KIND_UNKNOWN)
		g_string_append_printf (str, "\trating-kind:\t%s\n",
					priv->rating_kind == GS_APP_RATING_KIND_USER ?
						"user" : "system");
	if (priv->pixbuf != NULL)
		g_string_append_printf (str, "\tpixbuf:\t%p\n", priv->pixbuf);
	if (priv->featured_pixbuf != NULL)
		g_string_append_printf (str, "\tfeatured-pixbuf:\t%p\n", priv->featured_pixbuf);
	if (priv->install_date != 0) {
		g_string_append_printf (str, "\tinstall-date:\t%"
					G_GUINT64_FORMAT "\n",
					priv->install_date);
	}
	if (priv->size != 0) {
		g_string_append_printf (str, "\tsize:\t%" G_GUINT64_FORMAT "k\n",
					priv->size / 1024);
	}
	if (priv->related->len > 0)
		g_string_append_printf (str, "\trelated:\t%i\n", priv->related->len);
	if (priv->history->len > 0)
		g_string_append_printf (str, "\thistory:\t%i\n", priv->history->len);
	for (i = 0; i < priv->categories->len; i++) {
		tmp = g_ptr_array_index (priv->categories, i);
		g_string_append_printf (str, "\tcategory:\t%s\n", tmp);
	}
	keys = g_hash_table_get_keys (priv->metadata);
	for (l = keys; l != NULL; l = l->next) {
		tmp = g_hash_table_lookup (priv->metadata, l->data);
		g_string_append_printf (str, "\t{%s}:\t%s\n",
					(const gchar *) l->data, tmp);
	}
	g_list_free (keys);
	return g_string_free (str, FALSE);
}

typedef struct {
	GsApp *app;
	gchar *property_name;
} AppNotifyData;

static gboolean
notify_idle_cb (gpointer data)
{
	AppNotifyData *notify_data = data;

	g_object_notify (G_OBJECT (notify_data->app),
	                 notify_data->property_name);

	g_object_unref (notify_data->app);
	g_free (notify_data->property_name);
	g_free (notify_data);

	return G_SOURCE_REMOVE;
}

static void
gs_app_queue_notify (GsApp *app, const gchar *property_name)
{
	AppNotifyData *notify_data;

	notify_data = g_new (AppNotifyData, 1);
	notify_data->app = g_object_ref (app);
	notify_data->property_name = g_strdup (property_name);

	g_idle_add (notify_idle_cb, notify_data);
}

/**
 * gs_app_get_id:
 **/
const gchar *
gs_app_get_id (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->id;
}

/**
 * gs_app_set_id:
 */
void
gs_app_set_id (GsApp *app, const gchar *id)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->id);
	app->priv->id = g_strdup (id);
}

/**
 * gs_app_get_state:
 */
AsAppState
gs_app_get_state (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), AS_APP_STATE_UNKNOWN);
	return app->priv->state;
}

/**
 * gs_app_get_progress:
 */
guint
gs_app_get_progress (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), 0);
	return app->priv->progress;
}

/**
 * gs_app_set_state_internal:
 */
static gboolean
gs_app_set_state_internal (GsApp *app, AsAppState state)
{
	gboolean state_change_ok = FALSE;
	GsAppPrivate *priv = app->priv;

	if (priv->state == state)
		return FALSE;

	/* check the state change is allowed */
	switch (priv->state) {
	case AS_APP_STATE_UNKNOWN:
		/* unknown has to go into one of the stable states */
		if (state == AS_APP_STATE_INSTALLED ||
		    state == AS_APP_STATE_QUEUED_FOR_INSTALL ||
		    state == AS_APP_STATE_AVAILABLE ||
		    state == AS_APP_STATE_AVAILABLE_LOCAL ||
		    state == AS_APP_STATE_UPDATABLE ||
		    state == AS_APP_STATE_UNAVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_INSTALLED:
		/* installed has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_REMOVING)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_QUEUED_FOR_INSTALL:
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_INSTALLING ||
		    state == AS_APP_STATE_AVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_AVAILABLE:
		/* available has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_QUEUED_FOR_INSTALL ||
		    state == AS_APP_STATE_INSTALLING)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_INSTALLING:
		/* installing has to go into an stable state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_INSTALLED ||
		    state == AS_APP_STATE_AVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_REMOVING:
		/* removing has to go into an stable state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_AVAILABLE ||
		    state == AS_APP_STATE_INSTALLED)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_UPDATABLE:
		/* updatable has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_REMOVING)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_UNAVAILABLE:
		/* updatable has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_AVAILABLE)
			state_change_ok = TRUE;
		break;
	case AS_APP_STATE_AVAILABLE_LOCAL:
		/* local has to go into an action state */
		if (state == AS_APP_STATE_UNKNOWN ||
		    state == AS_APP_STATE_INSTALLING)
			state_change_ok = TRUE;
		break;
	default:
		g_warning ("state %s unhandled",
			   as_app_state_to_string (priv->state));
		g_assert_not_reached ();
	}

	/* this state change was unexpected */
	if (!state_change_ok) {
		g_warning ("State change on %s from %s to %s is not OK",
			   priv->id,
			   as_app_state_to_string (priv->state),
			   as_app_state_to_string (state));
		return FALSE;
	}

	priv->state = state;

	if (state == AS_APP_STATE_UNKNOWN ||
	    state == AS_APP_STATE_AVAILABLE_LOCAL ||
	    state == AS_APP_STATE_AVAILABLE)
		app->priv->install_date = 0;

	return TRUE;
}

/**
 * gs_app_set_progress:
 *
 * This sets the progress completion of the application.
 */
void
gs_app_set_progress (GsApp *app, guint percentage)
{
	GsAppPrivate *priv = app->priv;
	g_return_if_fail (GS_IS_APP (app));
	if (priv->progress == percentage)
		return;
	priv->progress = percentage;
	gs_app_queue_notify (app, "progress");
}

/**
 * gs_app_set_state:
 *
 * This sets the state of the application. The following state diagram explains
 * the typical states. All applications start in state %AS_APP_STATE_UNKNOWN,
 * but the frontend is not supposed to see GsApps with this state, ever.
 * Backend plugins are reponsible for changing the state to one of the other
 * states before the GsApp is passed to the frontend. This is enforced by the
 * #GsPluginLoader.
 *
 * UPDATABLE --> INSTALLING --> INSTALLED
 * UPDATABLE --> REMOVING   --> AVAILABLE
 * INSTALLED --> REMOVING   --> AVAILABLE
 * AVAILABLE --> INSTALLING --> INSTALLED
 * AVAILABLE <--> QUEUED --> INSTALLING --> INSTALLED
 * UNKNOWN   --> UNAVAILABLE
 */
void
gs_app_set_state (GsApp *app, AsAppState state)
{
	g_return_if_fail (GS_IS_APP (app));

	if (gs_app_set_state_internal (app, state))
		gs_app_queue_notify (app, "state");
}

/**
 * gs_app_get_kind:
 */
GsAppKind
gs_app_get_kind (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), GS_APP_KIND_UNKNOWN);
	return app->priv->kind;
}

/**
 * gs_app_set_kind:
 *
 * This sets the kind of the application. The following state diagram explains
 * the typical states. All applications start with kind %GS_APP_KIND_UNKNOWN.
 *
 * PACKAGE --> NORMAL
 * PACKAGE --> SYSTEM
 * NORMAL  --> SYSTEM
 */
void
gs_app_set_kind (GsApp *app, GsAppKind kind)
{
	gboolean state_change_ok = FALSE;
	GsAppPrivate *priv = app->priv;

	g_return_if_fail (GS_IS_APP (app));
	if (priv->kind == kind)
		return;

	/* check the state change is allowed */
	switch (priv->kind) {
	case GS_APP_KIND_UNKNOWN:
		/* unknown can go into any state */
		state_change_ok = TRUE;
		break;
	case GS_APP_KIND_PACKAGE:
		/* package can become either normal or a system application */
		if (kind == GS_APP_KIND_NORMAL ||
		    kind == GS_APP_KIND_SYSTEM ||
		    kind == GS_APP_KIND_CORE ||
		    kind == GS_APP_KIND_SOURCE ||
		    kind == GS_APP_KIND_UNKNOWN)
			state_change_ok = TRUE;
		break;
	case GS_APP_KIND_NORMAL:
		/* normal can only be promoted to system */
		if (kind == GS_APP_KIND_SYSTEM ||
		    kind == GS_APP_KIND_UNKNOWN)
			state_change_ok = TRUE;
		break;
	case GS_APP_KIND_SYSTEM:
	case GS_APP_KIND_OS_UPDATE:
	case GS_APP_KIND_CORE:
	case GS_APP_KIND_SOURCE:
	case GS_APP_KIND_MISSING:
		/* this can never change state */
		break;
	default:
		g_warning ("kind %s unhandled",
			   gs_app_kind_to_string (priv->kind));
		g_assert_not_reached ();
	}

	/* this state change was unexpected */
	if (!state_change_ok) {
		g_warning ("Kind change on %s from %s to %s is not OK",
			   priv->id,
			   gs_app_kind_to_string (priv->kind),
			   gs_app_kind_to_string (kind));
		return;
	}

	priv->kind = kind;
	gs_app_queue_notify (app, "kind");
}

/**
 * gs_app_get_id_kind:
 */
AsIdKind
gs_app_get_id_kind (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), GS_APP_KIND_UNKNOWN);
	return app->priv->id_kind;
}

/**
 * gs_app_set_id_kind:
 */
void
gs_app_set_id_kind (GsApp *app, AsIdKind id_kind)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->id_kind = id_kind;
}

/**
 * gs_app_get_name:
 */
const gchar *
gs_app_get_name (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->name;
}

/**
 * gs_app_set_name:
 * @app:	A #GsApp instance
 * @quality:	A data quality, e.g. %GS_APP_QUALITY_LOWEST
 * @name:	The short localized name, e.g. "Calculator"
 */
void
gs_app_set_name (GsApp *app, GsAppQuality quality, const gchar *name)
{
	g_return_if_fail (GS_IS_APP (app));

	/* only save this if the data is sufficiently high quality */
	if (quality < app->priv->name_quality)
		return;
	app->priv->name_quality = quality;

	g_free (app->priv->name);
	app->priv->name = g_strdup (name);
}

/**
 * gs_app_get_source_default:
 */
const gchar *
gs_app_get_source_default (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	if (app->priv->sources->len == 0)
		return NULL;
	return g_ptr_array_index (app->priv->sources, 0);
}

/**
 * gs_app_add_source:
 */
void
gs_app_add_source (GsApp *app, const gchar *source)
{
	const gchar *tmp;
	guint i;

	g_return_if_fail (GS_IS_APP (app));

	/* check source doesn't already exist */
	for (i = 0; i < app->priv->sources->len; i++) {
		tmp = g_ptr_array_index (app->priv->sources, i);
		if (g_strcmp0 (tmp, source) == 0)
			return;
	}
	g_ptr_array_add (app->priv->sources, g_strdup (source));
}

/**
 * gs_app_get_sources:
 */
GPtrArray *
gs_app_get_sources (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->sources;
}

/**
 * gs_app_set_sources:
 * @app:	A #GsApp instance
 * @source:	The non-localized short names, e.g. ["gnome-calculator"]
 *
 * This name is used for the update page if the application is collected into
 * the 'OS Updates' group. It is typically the package names, although this
 * should not be relied upon.
 */
void
gs_app_set_sources (GsApp *app, GPtrArray *sources)
{
	g_return_if_fail (GS_IS_APP (app));
	if (app->priv->sources != NULL)
		g_ptr_array_unref (app->priv->sources);
	app->priv->sources = g_ptr_array_ref (sources);
}

/**
 * gs_app_get_source_id_default:
 */
const gchar *
gs_app_get_source_id_default (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	if (app->priv->source_ids->len == 0)
		return NULL;
	return g_ptr_array_index (app->priv->source_ids, 0);
}

/**
 * gs_app_get_source_ids:
 */
GPtrArray *
gs_app_get_source_ids (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->source_ids;
}

/**
 * gs_app_clear_source_ids:
 */
void
gs_app_clear_source_ids (GsApp *app)
{
	g_return_if_fail (GS_IS_APP (app));
	g_ptr_array_set_size (app->priv->source_ids, 0);
}

/**
 * gs_app_set_source_ids:
 * @app:	A #GsApp instance
 * @source_id:	The source-id, e.g. ["gnome-calculator;0.134;fedora"]
 * 		or ["/home/hughsie/.local/share/applications/0ad.desktop"]
 *
 * This ID is used internally to the controlling plugin.
 */
void
gs_app_set_source_ids (GsApp *app, GPtrArray *source_ids)
{
	g_return_if_fail (GS_IS_APP (app));
	if (app->priv->source_ids != NULL)
		g_ptr_array_unref (app->priv->source_ids);
	app->priv->source_ids = g_ptr_array_ref (source_ids);
}

/**
 * gs_app_add_source_id:
 */
void
gs_app_add_source_id (GsApp *app, const gchar *source_id)
{
	const gchar *tmp;
	guint i;

	g_return_if_fail (GS_IS_APP (app));

	/* only add if not already present */
	for (i = 0; i < app->priv->source_ids->len; i++) {
		tmp = g_ptr_array_index (app->priv->source_ids, i);
		if (g_strcmp0 (tmp, source_id) == 0)
			return;
	}
	g_ptr_array_add (app->priv->source_ids, g_strdup (source_id));
}

/**
 * gs_app_get_project_group:
 */
const gchar *
gs_app_get_project_group (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->project_group;
}

/**
 * gs_app_set_project_group:
 * @app:	A #GsApp instance
 * @project_group:	The non-localized project group, e.g. "GNOME" or "KDE"
 */
void
gs_app_set_project_group (GsApp *app, const gchar *project_group)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->project_group);
	app->priv->project_group = g_strdup (project_group);
}

/**
 * gs_app_get_pixbuf:
 */
GdkPixbuf *
gs_app_get_pixbuf (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->pixbuf;
}

/**
 * gs_app_get_icon:
 */
const gchar *
gs_app_get_icon (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->icon;
}

/**
 * gs_app_set_icon:
 */
void
gs_app_set_icon (GsApp *app, const gchar *icon)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (icon != NULL);

	/* save icon */
	g_free (app->priv->icon);
	app->priv->icon = g_strdup (icon);
}

/**
 * gs_app_get_icon_path:
 */
const gchar *
gs_app_get_icon_path (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->icon_path;
}

/**
 * gs_app_set_icon_path:
 */
void
gs_app_set_icon_path (GsApp *app, const gchar *icon_path)
{
	g_return_if_fail (GS_IS_APP (app));

	/* save theme path */
	g_free (app->priv->icon_path);
	app->priv->icon_path = g_strdup (icon_path);
}

/**
 * gs_app_load_icon:
 */
gboolean
gs_app_load_icon (GsApp *app, gint scale, GError **error)
{
	GdkPixbuf *pixbuf = NULL;
	gboolean ret = TRUE;

	g_return_val_if_fail (GS_IS_APP (app), FALSE);
	g_return_val_if_fail (app->priv->icon != NULL, FALSE);

	/* either load from the theme or from a file */
	pixbuf = gs_pixbuf_load (app->priv->icon, app->priv->icon_path,
				 64 * scale, error);
	if (pixbuf == NULL) {
		ret = FALSE;
		goto out;
	}
	gs_app_set_pixbuf (app, pixbuf);
out:
	if (pixbuf != NULL)
		g_object_unref (pixbuf);
	return ret;
}

/**
 * gs_app_set_pixbuf:
 */
void
gs_app_set_pixbuf (GsApp *app, GdkPixbuf *pixbuf)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
	if (app->priv->pixbuf != NULL)
		g_object_unref (app->priv->pixbuf);
	app->priv->pixbuf = g_object_ref (pixbuf);
}

/**
 * gs_app_get_featured_pixbuf:
 */
GdkPixbuf *
gs_app_get_featured_pixbuf (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->featured_pixbuf;
}

/**
 * gs_app_set_featured_pixbuf:
 */
void
gs_app_set_featured_pixbuf (GsApp *app, GdkPixbuf *pixbuf)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (app->priv->featured_pixbuf == NULL);
	app->priv->featured_pixbuf = g_object_ref (pixbuf);
}

typedef enum {
	GS_APP_VERSION_FIXUP_RELEASE		= 1,
	GS_APP_VERSION_FIXUP_DISTRO_SUFFIX	= 2,
	GS_APP_VERSION_FIXUP_GIT_SUFFIX		= 4,
	GS_APP_VERSION_FIXUP_LAST,
} GsAppVersionFixup;

/**
 * gs_app_get_ui_version:
 *
 * convert 1:1.6.2-7.fc17 into "Version 1.6.2"
 **/
static gchar *
gs_app_get_ui_version (const gchar *version, guint64 flags)
{
	guint i;
	gchar *new = NULL;
	gchar *f;

	/* nothing set */
	if (version == NULL)
		goto out;

	/* first remove any epoch */
	for (i = 0; version[i] != '\0'; i++) {
		if (version[i] == ':') {
			version = &version[i+1];
			break;
		}
		if (!g_ascii_isdigit (version[i]))
			break;
	}

	/* then remove any distro suffix */
	new = g_strdup (version);
	if ((flags & GS_APP_VERSION_FIXUP_DISTRO_SUFFIX) > 0) {
		f = g_strstr_len (new, -1, ".fc");
		if (f != NULL)
			*f= '\0';
		f = g_strstr_len (new, -1, ".el");
		if (f != NULL)
			*f= '\0';
	}

	/* then remove any release */
	if ((flags & GS_APP_VERSION_FIXUP_RELEASE) > 0) {
		f = g_strrstr_len (new, -1, "-");
		if (f != NULL)
			*f= '\0';
	}

	/* then remove any git suffix */
	if ((flags & GS_APP_VERSION_FIXUP_GIT_SUFFIX) > 0) {
		f = g_strrstr_len (new, -1, ".2012");
		if (f != NULL)
			*f= '\0';
		f = g_strrstr_len (new, -1, ".2013");
		if (f != NULL)
			*f= '\0';
	}
out:
	return new;
}

/**
 * gs_app_ui_versions_invalidate:
 */
static void
gs_app_ui_versions_invalidate (GsApp *app)
{
	GsAppPrivate *priv = app->priv;
	g_free (priv->version_ui);
	g_free (priv->update_version_ui);
	priv->version_ui = NULL;
	priv->update_version_ui = NULL;
}

/**
 * gs_app_ui_versions_populate:
 */
static void
gs_app_ui_versions_populate (GsApp *app)
{
	GsAppPrivate *priv = app->priv;
	guint i;
	guint64 flags[] = { GS_APP_VERSION_FIXUP_RELEASE |
			    GS_APP_VERSION_FIXUP_DISTRO_SUFFIX |
			    GS_APP_VERSION_FIXUP_GIT_SUFFIX,
			    GS_APP_VERSION_FIXUP_DISTRO_SUFFIX |
			    GS_APP_VERSION_FIXUP_GIT_SUFFIX,
			    GS_APP_VERSION_FIXUP_DISTRO_SUFFIX,
			    0 };

	/* try each set of bitfields in order */
	for (i = 0; flags[i] != 0; i++) {
		priv->version_ui = gs_app_get_ui_version (priv->version, flags[i]);
		priv->update_version_ui = gs_app_get_ui_version (priv->update_version, flags[i]);
		if (g_strcmp0 (priv->version_ui, priv->update_version_ui) != 0) {
			gs_app_queue_notify (app, "version");
			return;
		}
		gs_app_ui_versions_invalidate (app);
	}

	/* we tried, but failed */
	priv->version_ui = g_strdup (priv->version);
	priv->update_version_ui = g_strdup (priv->update_version);
}

/**
 * gs_app_get_version:
 */
const gchar *
gs_app_get_version (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->version;
}

/**
 * gs_app_get_version_ui:
 */
const gchar *
gs_app_get_version_ui (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);

	/* work out the two version numbers */
	if (app->priv->version != NULL &&
	    app->priv->version_ui == NULL) {
		gs_app_ui_versions_populate (app);
	}

	return app->priv->version_ui;
}

/**
 * gs_app_set_version:
 * @app:	A #GsApp instance
 * @version:	The version, e.g. "2:1.2.3.fc19"
 *
 * This saves the version after stripping out any non-friendly parts, such as
 * distro tags, git revisions and that kind of thing.
 */
void
gs_app_set_version (GsApp *app, const gchar *version)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->version);
	app->priv->version = g_strdup (version);
	gs_app_ui_versions_invalidate (app);
	gs_app_queue_notify (app, "version");
}

/**
 * gs_app_get_summary:
 */
const gchar *
gs_app_get_summary (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->summary;
}

/**
 * gs_app_set_summary:
 * @app:	A #GsApp instance
 * @quality:	A data quality, e.g. %GS_APP_QUALITY_LOWEST
 * @summary:	The medium length localized name, e.g. "A graphical calculator for GNOME"
 */
void
gs_app_set_summary (GsApp *app, GsAppQuality quality, const gchar *summary)
{
	g_return_if_fail (GS_IS_APP (app));

	/* only save this if the data is sufficiently high quality */
	if (quality < app->priv->summary_quality)
		return;
	app->priv->summary_quality = quality;

	g_free (app->priv->summary);
	app->priv->summary = g_strdup (summary);
}

/**
 * gs_app_get_description:
 */
const gchar *
gs_app_get_description (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->description;
}

/**
 * gs_app_set_description:
 * @app:	A #GsApp instance
 * @quality:	A data quality, e.g. %GS_APP_QUALITY_LOWEST
 * @summary:	The multiline localized description, e.g. "GNOME Calculator is a graphical calculator for GNOME....."
 */
void
gs_app_set_description (GsApp *app, GsAppQuality quality, const gchar *description)
{
	g_return_if_fail (GS_IS_APP (app));

	/* only save this if the data is sufficiently high quality */
	if (quality < app->priv->description_quality)
		return;
	app->priv->description_quality = quality;

	g_free (app->priv->description);
	app->priv->description = g_strdup (description);
}

/**
 * gs_app_get_url:
 */
const gchar *
gs_app_get_url (GsApp *app, AsUrlKind kind)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return g_hash_table_lookup (app->priv->urls, as_url_kind_to_string (kind));
}

/**
 * gs_app_set_url:
 */
void
gs_app_set_url (GsApp *app, AsUrlKind kind, const gchar *url)
{
	g_return_if_fail (GS_IS_APP (app));
	g_hash_table_insert (app->priv->urls,
			     g_strdup (as_url_kind_to_string (kind)),
			     g_strdup (url));
}

/**
 * gs_app_get_licence:
 */
const gchar *
gs_app_get_licence (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->licence;
}

/**
 * gs_app_set_licence:
 */
void
gs_app_set_licence (GsApp *app, const gchar *licence)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->licence);
	app->priv->licence = g_strdup (licence);
}

/**
 * gs_app_get_summary_missing:
 */
const gchar *
gs_app_get_summary_missing (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->summary_missing;
}

/**
 * gs_app_set_summary_missing:
 */
void
gs_app_set_summary_missing (GsApp *app, const gchar *summary_missing)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->summary_missing);
	app->priv->summary_missing = g_strdup (summary_missing);
}

/**
 * gs_app_get_menu_path:
 */
const gchar *
gs_app_get_menu_path (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->menu_path;
}

/**
 * gs_app_set_menu_path:
 */
void
gs_app_set_menu_path (GsApp *app, const gchar *menu_path)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->menu_path);
	app->priv->menu_path = g_strdup (menu_path);
}

/**
 * gs_app_get_origin:
 */
const gchar *
gs_app_get_origin (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->origin;
}

/**
 * gs_app_set_origin:
 *
 * The origin is the original source of the application to show in the UI,
 * e.g. "Fedora"
 */
void
gs_app_set_origin (GsApp *app, const gchar *origin)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->origin);
	app->priv->origin = g_strdup (origin);
}

/**
 * gs_app_add_screenshot:
 */
void
gs_app_add_screenshot (GsApp *app, AsScreenshot *screenshot)
{
	g_return_if_fail (GS_IS_APP (app));
	g_ptr_array_add (app->priv->screenshots, g_object_ref (screenshot));
}

/**
 * gs_app_get_screenshots:
 */
GPtrArray *
gs_app_get_screenshots (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->screenshots;
}

/**
 * gs_app_get_update_version:
 */
const gchar *
gs_app_get_update_version (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->update_version;
}

/**
 * gs_app_get_update_version_ui:
 */
const gchar *
gs_app_get_update_version_ui (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);

	/* work out the two version numbers */
	if (app->priv->update_version != NULL &&
	    app->priv->update_version_ui == NULL) {
		gs_app_ui_versions_populate (app);
	}

	return app->priv->update_version_ui;
}

/**
 * gs_app_set_update_version_internal:
 */
static void
gs_app_set_update_version_internal (GsApp *app, const gchar *update_version)
{
	g_free (app->priv->update_version);
	app->priv->update_version = g_strdup (update_version);
	gs_app_ui_versions_invalidate (app);
}

/**
 * gs_app_set_update_version:
 */
void
gs_app_set_update_version (GsApp *app, const gchar *update_version)
{
	g_return_if_fail (GS_IS_APP (app));
	gs_app_set_update_version_internal (app, update_version);
	gs_app_queue_notify (app, "version");
}

/**
 * gs_app_get_update_details:
 */
const gchar *
gs_app_get_update_details (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->update_details;
}

/**
 * gs_app_set_update_details:
 */
void
gs_app_set_update_details (GsApp *app, const gchar *update_details)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->update_details);
	app->priv->update_details = g_strdup (update_details);
}

/**
 * gs_app_get_management_plugin:
 */
const gchar *
gs_app_get_management_plugin (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->management_plugin;
}

/**
 * gs_app_set_management_plugin:
 *
 * The management plugin is the plugin that can handle doing install and remove
 * operations on the #GsApp. Typical values include "PackageKit" and "jhbuild"
 */
void
gs_app_set_management_plugin (GsApp *app, const gchar *management_plugin)
{
	g_return_if_fail (GS_IS_APP (app));
	g_free (app->priv->management_plugin);
	app->priv->management_plugin = g_strdup (management_plugin);
}

/**
 * gs_app_get_rating:
 */
gint
gs_app_get_rating (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), -1);
	return app->priv->rating;
}

/**
 * gs_app_set_rating:
 */
void
gs_app_set_rating (GsApp *app, gint rating)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->rating = rating;
	gs_app_queue_notify (app, "rating");
}

/**
 * gs_app_get_rating_confidence:
 * @app:	A #GsApp instance
 *
 * Return value: a predictor from 0 to 100, or -1 for unknown or invalid
 */
gint
gs_app_get_rating_confidence (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), -1);
	return app->priv->rating_confidence;
}

/**
 * gs_app_set_rating_confidence:
 * @app:	A #GsApp instance
 * @rating_confidence:	a predictor from 0 to 100, or -1 for unknown or invalid
 *
 * This is how confident the rating is statistically valid, expressed as a
 * percentage.
 * Applications with a high confidence typically have a large number of samples
 * and can be trusted, but low confidence could mean that only one person has
 * rated the application.
 */
void
gs_app_set_rating_confidence (GsApp *app, gint rating_confidence)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->rating_confidence = rating_confidence;
}

/**
 * gs_app_get_rating_kind:
 */
GsAppRatingKind
gs_app_get_rating_kind (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), -1);
	return app->priv->rating_kind;
}

/**
 * gs_app_set_rating_kind:
 */
void
gs_app_set_rating_kind (GsApp *app, GsAppRatingKind rating_kind)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->rating_kind = rating_kind;
	gs_app_queue_notify (app, "rating");
}

/**
 * gs_app_get_size:
 */
guint64
gs_app_get_size (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), G_MAXUINT64);
	return app->priv->size;
}

/**
 * gs_app_set_size:
 */
void
gs_app_set_size (GsApp *app, guint64 size)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->size = size;
}

/**
 * gs_app_get_metadata_item:
 */
const gchar *
gs_app_get_metadata_item (GsApp *app, const gchar *key)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return g_hash_table_lookup (app->priv->metadata, key);
}

/**
 * gs_app_set_metadata:
 */
void
gs_app_set_metadata (GsApp *app, const gchar *key, const gchar *value)
{
	const gchar *found;

	g_return_if_fail (GS_IS_APP (app));

	/* if no value, then remove the key */
	if (value == NULL) {
		g_hash_table_remove (app->priv->metadata, key);
		return;
	}

	/* check we're not overwriting */
	found = g_hash_table_lookup (app->priv->metadata, key);
	if (found != NULL) {
		if (g_strcmp0 (found, value) == 0)
			return;
		g_warning ("tried overwriting key %s from %s to %s",
			   key, found, value);
		return;
	}
	g_hash_table_insert (app->priv->metadata,
			     g_strdup (key),
			     g_strdup (value));
}

/**
 * gs_app_get_addons:
 */
GPtrArray *
gs_app_get_addons (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->addons;
}

/**
 * gs_app_add_addon:
 */
void
gs_app_add_addon (GsApp *app, GsApp *addon)
{
	gpointer found;
	const gchar *id;

	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GS_IS_APP (addon));

	id = gs_app_get_id (addon);
	found = g_hash_table_lookup (app->priv->addons_hash, id);
	if (found != NULL) {
		g_debug ("Already added %s as an addon", id);
		return;
	}
	g_hash_table_insert (app->priv->addons_hash, g_strdup (id), GINT_TO_POINTER (1));

	g_ptr_array_add (app->priv->addons, g_object_ref (addon));
}

/**
 * gs_app_get_related:
 */
GPtrArray *
gs_app_get_related (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->related;
}

/**
 * gs_app_add_related:
 */
void
gs_app_add_related (GsApp *app, GsApp *app2)
{
	gchar *key;
	gpointer found;

	g_return_if_fail (GS_IS_APP (app));

	key = g_strdup_printf ("%s-%s",
			       gs_app_get_id (app2),
			       gs_app_get_source_default (app2));
	found = g_hash_table_lookup (app->priv->related_hash, key);
	if (found != NULL) {
		g_debug ("Already added %s as a related item", key);
		g_free (key);
		return;
	}
	g_hash_table_insert (app->priv->related_hash, key, GINT_TO_POINTER (1));
	g_ptr_array_add (app->priv->related, g_object_ref (app2));
}

/**
 * gs_app_get_history:
 */
GPtrArray *
gs_app_get_history (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->history;
}

/**
 * gs_app_add_history:
 */
void
gs_app_add_history (GsApp *app, GsApp *app2)
{
	g_return_if_fail (GS_IS_APP (app));
	g_ptr_array_add (app->priv->history, g_object_ref (app2));
}

guint64
gs_app_get_install_date (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), 0);
	return app->priv->install_date;
}

void
gs_app_set_install_date (GsApp *app, guint64 install_date)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->install_date = install_date;
}

/**
 * gs_app_get_categories:
 */
GPtrArray *
gs_app_get_categories (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->categories;
}

/**
 * gs_app_has_category:
 */
gboolean
gs_app_has_category (GsApp *app, const gchar *category)
{
	const gchar *tmp;
	guint i;

	g_return_val_if_fail (GS_IS_APP (app), FALSE);

	/* find the category */
	for (i = 0; i < app->priv->categories->len; i++) {
		tmp = g_ptr_array_index (app->priv->categories, i);
		if (g_strcmp0 (tmp, category) == 0)
			return TRUE;
	}
	return FALSE;
}

/**
 * gs_app_set_categories:
 */
void
gs_app_set_categories (GsApp *app, GPtrArray *categories)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (categories != NULL);
	if (app->priv->categories != NULL)
		g_ptr_array_unref (app->priv->categories);
	app->priv->categories = g_ptr_array_ref (categories);
}

/**
 * gs_app_add_category:
 */
void
gs_app_add_category (GsApp *app, const gchar *category)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (category != NULL);
	g_ptr_array_add (app->priv->categories, g_strdup (category));
}

/**
 * gs_app_get_keywords:
 */
GPtrArray *
gs_app_get_keywords (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), NULL);
	return app->priv->keywords;
}

/**
 * gs_app_set_keywords:
 */
void
gs_app_set_keywords (GsApp *app, GPtrArray *keywords)
{
	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (keywords != NULL);
	if (app->priv->keywords != NULL)
		g_ptr_array_unref (app->priv->keywords);
	app->priv->keywords = g_ptr_array_ref (keywords);
}

/**
 * gs_app_add_kudo:
 */
void
gs_app_add_kudo (GsApp *app, GsAppKudo kudo)
{
	g_return_if_fail (GS_IS_APP (app));
	app->priv->kudos |= kudo;
}

/**
 * gs_app_get_kudos:
 */
guint64
gs_app_get_kudos (GsApp *app)
{
	g_return_val_if_fail (GS_IS_APP (app), 0);
	return app->priv->kudos;
}

/**
 * gs_app_get_kudos_weight:
 */
guint
gs_app_get_kudos_weight (GsApp *app)
{
	guint32 tmp = app->priv->kudos;
	tmp = tmp - ((tmp >> 1) & 0x55555555);
	tmp = (tmp & 0x33333333) + ((tmp >> 2) & 0x33333333);
	return (((tmp + (tmp >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * gs_app_get_kudos_percentage:
 */
guint
gs_app_get_kudos_percentage (GsApp *app)
{
	guint percentage = 0;

	g_return_val_if_fail (GS_IS_APP (app), 0);

	if ((app->priv->kudos & GS_APP_KUDO_MY_LANGUAGE) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_RECENT_RELEASE) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_FEATURED_RECOMMENDED) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_MODERN_TOOLKIT) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_SEARCH_PROVIDER) > 0)
		percentage += 10;
	if ((app->priv->kudos & GS_APP_KUDO_INSTALLS_USER_DOCS) > 0)
		percentage += 10;
	if ((app->priv->kudos & GS_APP_KUDO_USES_NOTIFICATIONS) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_HAS_KEYWORDS) > 0)
		percentage += 5;
	if ((app->priv->kudos & GS_APP_KUDO_USES_APP_MENU) > 0)
		percentage += 10;
	if ((app->priv->kudos & GS_APP_KUDO_HAS_SCREENSHOTS) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_IBUS_HAS_SYMBOL) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_PERFECT_SCREENSHOTS) > 0)
		percentage += 20;
	if ((app->priv->kudos & GS_APP_KUDO_HIGH_CONTRAST) > 0)
		percentage += 20;

	/* popular apps should be at *least* 50% */
	if ((app->priv->kudos & GS_APP_KUDO_POPULAR) > 0)
		percentage = MAX (percentage, 50);

	return MIN (percentage, 100);
}

/**
 * gs_app_get_to_be_installed:
 */
gboolean
gs_app_get_to_be_installed (GsApp *app)
{
	GsAppPrivate *priv = app->priv;
	return priv->to_be_installed;
}

/**
 * gs_app_set_to_be_installed:
 */
void
gs_app_set_to_be_installed (GsApp *app, gboolean to_be_installed)
{
	GsAppPrivate *priv = app->priv;
	priv->to_be_installed = to_be_installed;
}

/**
 * gs_app_subsume:
 *
 * Imports all the useful data from @other into @app.
 *
 * IMPORTANT: This method can be called from a thread as the notify signals
 * are not sent.
 **/
void
gs_app_subsume (GsApp *app, GsApp *other)
{
	const gchar *tmp;
	GList *keys;
	GList *l;
	GsApp *app_tmp;
	GsAppPrivate *priv2 = other->priv;
	GsAppPrivate *priv = app->priv;
	guint i;

	g_return_if_fail (GS_IS_APP (app));
	g_return_if_fail (GS_IS_APP (other));
	g_return_if_fail (app != other);

	/* an [updatable] installable package is more information than
	 * just the fact that something is installed */
	if (priv2->state == AS_APP_STATE_UPDATABLE &&
	    priv->state == AS_APP_STATE_INSTALLED) {
		/* we have to do the little dance to appease the
		 * angry gnome controlling the state-machine */
		gs_app_set_state_internal (app, AS_APP_STATE_UNKNOWN);
		gs_app_set_state_internal (app, AS_APP_STATE_UPDATABLE);
	}

	/* save any properties we already know */
	if (priv2->sources->len > 0)
		gs_app_set_sources (app, priv2->sources);
	if (priv2->project_group != NULL)
		gs_app_set_project_group (app, priv2->project_group);
	if (priv2->name != NULL)
		gs_app_set_name (app, priv2->name_quality, priv2->name);
	if (priv2->summary != NULL)
		gs_app_set_summary (app, priv2->summary_quality, priv2->summary);
	if (priv2->description != NULL)
		gs_app_set_description (app, priv2->description_quality, priv2->description);
	if (priv2->update_details != NULL)
		gs_app_set_update_details (app, priv2->update_details);
	if (priv2->update_version != NULL)
		gs_app_set_update_version_internal (app, priv2->update_version);
	if (priv2->pixbuf != NULL)
		gs_app_set_pixbuf (app, priv2->pixbuf);
	if (priv->categories != priv2->categories) {
		for (i = 0; i < priv2->categories->len; i++) {
			tmp = g_ptr_array_index (priv2->categories, i);
			gs_app_add_category (app, tmp);
		}
	}
	for (i = 0; i < priv2->related->len; i++) {
		app_tmp = g_ptr_array_index (priv2->related, i);
		gs_app_add_related (app, app_tmp);
	}
	priv->kudos |= priv2->kudos;

	/* copy metadata from @other to @app unless the app already has a key
	 * of that name */
	keys = g_hash_table_get_keys (priv2->metadata);
	for (l = keys; l != NULL; l = l->next) {
		tmp = g_hash_table_lookup (priv->metadata, l->data);
		if (tmp != NULL)
			continue;
		tmp = g_hash_table_lookup (priv2->metadata, l->data);
		gs_app_set_metadata (app, l->data, tmp);
	}
	g_list_free (keys);
}

/**
 * gs_app_set_search_sort_key:
 */
void
gs_app_set_search_sort_key (GsApp *app, guint match_value)
{
	gchar md_value[4];
	g_snprintf (md_value, 4, "%03i", match_value);
	gs_app_set_metadata (app, "SearchMatch", md_value);
}

/**
 * gs_app_get_search_sort_key:
 */
const gchar *
gs_app_get_search_sort_key (GsApp *app)
{
	return gs_app_get_metadata_item (app, "SearchMatch");
}

/**
 * gs_app_get_property:
 */
static void
gs_app_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	GsApp *app = GS_APP (object);
	GsAppPrivate *priv = app->priv;

	switch (prop_id) {
	case PROP_ID:
		g_value_set_string (value, priv->id);
		break;
	case PROP_NAME:
		g_value_set_string (value, priv->name);
		break;
	case PROP_VERSION:
		g_value_set_string (value, priv->version);
		break;
	case PROP_SUMMARY:
		g_value_set_string (value, priv->summary);
		break;
	case PROP_DESCRIPTION:
		g_value_set_string (value, priv->description);
		break;
	case PROP_RATING:
		g_value_set_uint (value, priv->rating);
		break;
	case PROP_KIND:
		g_value_set_uint (value, priv->kind);
		break;
	case PROP_STATE:
		g_value_set_uint (value, priv->state);
		break;
	case PROP_PROGRESS:
		g_value_set_uint (value, priv->progress);
		break;
	case PROP_INSTALL_DATE:
		g_value_set_uint64 (value, priv->install_date);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gs_app_set_property:
 */
static void
gs_app_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	GsApp *app = GS_APP (object);
	GsAppPrivate *priv = app->priv;

	switch (prop_id) {
	case PROP_ID:
		gs_app_set_id (app, g_value_get_string (value));
		break;
	case PROP_NAME:
		gs_app_set_name (app,
				 GS_APP_QUALITY_UNKNOWN,
				 g_value_get_string (value));
		break;
	case PROP_VERSION:
		gs_app_set_version (app, g_value_get_string (value));
		break;
	case PROP_SUMMARY:
		gs_app_set_summary (app,
				    GS_APP_QUALITY_UNKNOWN,
				    g_value_get_string (value));
		break;
	case PROP_DESCRIPTION:
		gs_app_set_description (app,
					GS_APP_QUALITY_UNKNOWN,
					g_value_get_string (value));
		break;
	case PROP_RATING:
		gs_app_set_rating (app, g_value_get_int (value));
		break;
	case PROP_KIND:
		gs_app_set_kind (app, g_value_get_uint (value));
		break;
	case PROP_STATE:
		gs_app_set_state_internal (app, g_value_get_uint (value));
		break;
	case PROP_PROGRESS:
		priv->progress = g_value_get_uint (value);
		break;
	case PROP_INSTALL_DATE:
		gs_app_set_install_date (app, g_value_get_uint64 (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/**
 * gs_app_class_init:
 * @klass: The GsAppClass
 **/
static void
gs_app_class_init (GsAppClass *klass)
{
	GParamSpec *pspec;
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = gs_app_finalize;
	object_class->get_property = gs_app_get_property;
	object_class->set_property = gs_app_set_property;

	/**
	 * GsApp:id:
	 */
	pspec = g_param_spec_string ("id", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_ID, pspec);

	/**
	 * GsApp:name:
	 */
	pspec = g_param_spec_string ("name", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_NAME, pspec);

	/**
	 * GsApp:version:
	 */
	pspec = g_param_spec_string ("version", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_VERSION, pspec);

	/**
	 * GsApp:summary:
	 */
	pspec = g_param_spec_string ("summary", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_SUMMARY, pspec);

	pspec = g_param_spec_string ("description", NULL, NULL,
				     NULL,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_DESCRIPTION, pspec);

	/**
	 * GsApp:rating:
	 */
	pspec = g_param_spec_int ("rating", NULL, NULL,
				  -1, 100, -1,
				  G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_RATING, pspec);

	/**
	 * GsApp:kind:
	 */
	pspec = g_param_spec_uint ("kind", NULL, NULL,
				   GS_APP_KIND_UNKNOWN,
				   GS_APP_KIND_LAST,
				   GS_APP_KIND_UNKNOWN,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_KIND, pspec);

	/**
	 * GsApp:state:
	 */
	pspec = g_param_spec_uint ("state", NULL, NULL,
				   AS_APP_STATE_UNKNOWN,
				   AS_APP_STATE_LAST,
				   AS_APP_STATE_UNKNOWN,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_STATE, pspec);

	/**
	 * GsApp:progress:
	 */
	pspec = g_param_spec_uint ("progress", NULL, NULL, 0, 100, 0,
				   G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_PROGRESS, pspec);

	pspec = g_param_spec_uint64 ("install-date", NULL, NULL,
				     0, G_MAXUINT64, 0,
				     G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (object_class, PROP_INSTALL_DATE, pspec);
}

/**
 * gs_app_init:
 **/
static void
gs_app_init (GsApp *app)
{
	app->priv = gs_app_get_instance_private (app);
	app->priv->rating = -1;
	app->priv->rating_confidence = -1;
	app->priv->rating_kind = GS_APP_RATING_KIND_UNKNOWN;
	app->priv->sources = g_ptr_array_new_with_free_func (g_free);
	app->priv->source_ids = g_ptr_array_new_with_free_func (g_free);
	app->priv->categories = g_ptr_array_new_with_free_func (g_free);
	app->priv->addons = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->related = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->history = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->screenshots = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	app->priv->metadata = g_hash_table_new_full (g_str_hash,
						     g_str_equal,
						     g_free,
						     g_free);
	app->priv->addons_hash = g_hash_table_new_full (g_str_hash,
	                                                g_str_equal,
	                                                g_free,
	                                                NULL);
	app->priv->related_hash = g_hash_table_new_full (g_str_hash,
							 g_str_equal,
							 g_free,
							 NULL);
	app->priv->urls = g_hash_table_new_full (g_str_hash,
						 g_str_equal,
						 g_free,
						 g_free);
}

/**
 * gs_app_finalize:
 * @object: The object to finalize
 **/
static void
gs_app_finalize (GObject *object)
{
	GsApp *app = GS_APP (object);
	GsAppPrivate *priv = app->priv;

	g_free (priv->id);
	g_free (priv->name);
	g_hash_table_unref (priv->urls);
	g_free (priv->icon);
	g_free (priv->icon_path);
	g_free (priv->licence);
	g_free (priv->menu_path);
	g_free (priv->origin);
	g_ptr_array_unref (priv->sources);
	g_ptr_array_unref (priv->source_ids);
	g_free (priv->project_group);
	g_free (priv->version);
	g_free (priv->version_ui);
	g_free (priv->summary);
	g_free (priv->summary_missing);
	g_free (priv->description);
	g_ptr_array_unref (priv->screenshots);
	g_free (priv->update_version);
	g_free (priv->update_version_ui);
	g_free (priv->update_details);
	g_free (priv->management_plugin);
	g_hash_table_unref (priv->metadata);
	g_hash_table_unref (priv->addons_hash);
	g_ptr_array_unref (priv->addons);
	g_hash_table_unref (priv->related_hash);
	g_ptr_array_unref (priv->related);
	g_ptr_array_unref (priv->history);
	if (priv->pixbuf != NULL)
		g_object_unref (priv->pixbuf);
	if (priv->featured_pixbuf != NULL)
		g_object_unref (priv->featured_pixbuf);
	g_ptr_array_unref (priv->categories);
	if (priv->keywords != NULL)
		g_ptr_array_unref (priv->keywords);

	G_OBJECT_CLASS (gs_app_parent_class)->finalize (object);
}

/**
 * gs_app_new:
 *
 * Return value: a new GsApp object.
 **/
GsApp *
gs_app_new (const gchar *id)
{
	GsApp *app;
	app = g_object_new (GS_TYPE_APP,
			    "id", id,
			    NULL);
	return GS_APP (app);
}

/* vim: set noexpandtab: */
