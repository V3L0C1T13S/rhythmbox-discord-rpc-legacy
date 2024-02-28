/*
 * rb-sample-plugin.h
 * 
 * Copyright (C) 2002-2005 - Paolo Maggi
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * The Rhythmbox authors hereby grant permission for non-GPL compatible
 * GStreamer plugins to be used and distributed together with GStreamer
 * and Rhythmbox. This permission is above and beyond the permissions granted
 * by the GPL license by which Rhythmbox is covered. If you modify this code
 * you may extend this exception to your version of the code, but you are not
 * obligated to do so. If you do not wish to do so, delete this exception
 * statement from your version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 */

#include <config.h>

#include <string.h> /* For strlen */
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <glib-object.h>
#include <stdio.h>
#include <assert.h>
#include <time.h>

#include "rb-plugin-macros.h"
#include "rb-debug.h"
#include "rb-shell.h"
#include "rb-dialog.h"
#include "rb-shell-player.h"

#include <pthread.h>
#include <unistd.h>

#pragma pack(push, 8)
#include "discord_game_sdk/c/discord_game_sdk.h"
#pragma pack(pop)

#define RB_TYPE_DISCORD_PLUGIN		(rb_sample_plugin_get_type ())
#define RB_DISCORD_PLUGIN(o)			(G_TYPE_CHECK_INSTANCE_CAST ((o), RB_TYPE_DISCORD_PLUGIN, RBDiscordPlugin))
#define RB_DISCORD_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_CAST((k), RB_TYPE_DISCORD_PLUGIN, RBDiscordPluginClass))
#define RB_IS_DISCORD_PLUGIN(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), RB_TYPE_DISCORD_PLUGIN))
#define RB_IS_DISCORD_PLUGIN_CLASS(k)		(G_TYPE_CHECK_CLASS_TYPE ((k), RB_TYPE_DISCORD_PLUGIN))
#define RB_DISCORD_PLUGIN_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), RB_TYPE_DISCORD_PLUGIN, RBDiscordPluginClass))

struct Application {
    struct IDiscordCore* core;
    struct IDiscordUsers* users;
	struct IDiscordApplicationManager* application;
	struct IDiscordActivityManager* activities;
};

typedef struct
{
	PeasExtensionBase parent;
	RBShellPlayer *shell_player;
	RhythmDB *db;
	struct Application app;
	RhythmDBEntry* playing_entry;
} RBDiscordPlugin;

typedef struct
{
	PeasExtensionBaseClass parent_class;
} RBDiscordPluginClass;

pthread_t callback_thid;

#define CLIENT_ID 1212075104486301696
#define PLUGIN_NAME "Discord Status"

G_MODULE_EXPORT void peas_register_types (PeasObjectModule *module);

static void rb_sample_plugin_init (RBDiscordPlugin *plugin);

RB_DEFINE_PLUGIN(RB_TYPE_DISCORD_PLUGIN, RBDiscordPlugin, rb_sample_plugin,)

#define DISCORD_REQUIRE(x) assert(x == DiscordResult_Ok)

void DISCORD_CALLBACK UpdateActivityCallback(void* data, enum EDiscordResult result)
{
    if (result == DiscordResult_Ok)
		rb_debug("successfully set activity!");
	else
		rb_debug("Failed to set activity! Status: %i", result);
}

void* run_callbacks(void* plugin) {
	RBDiscordPlugin *self = (RBDiscordPlugin *)plugin;

	for (;;) {
		DISCORD_REQUIRE(self->app.core->run_callbacks(self->app.core));

		usleep(16 * 1000);
	}

	pthread_exit(0);
}

static void
rb_sample_plugin_init (RBDiscordPlugin *self)
{
	rb_debug ("RBDiscordPlugin initialising");

    memset(&self->app, 0, sizeof(self->app));

	struct IDiscordActivityEvents activities_events;
    memset(&activities_events, 0, sizeof(activities_events));

	struct DiscordCreateParams params;
    params.client_id = CLIENT_ID;
    params.flags = DiscordCreateFlags_Default;
    params.event_data = &self->app;
	params.activity_events = &activities_events;

    DISCORD_REQUIRE(DiscordCreate(DISCORD_VERSION, &params, &self->app.core));

	self->app.application = self->app.core->get_application_manager(self->app.core);
	self->app.activities = self->app.core->get_activity_manager(self->app.core);

	if (pthread_create(&callback_thid, NULL, run_callbacks, self) != 0) {
    	perror("pthread_create() error");
    	exit(1);
  	}
}

void cleanup_playing_entry(RBDiscordPlugin *self) {
	if (self->playing_entry != NULL) {
		rhythmdb_entry_unref(self->playing_entry);
		self->playing_entry = NULL;
	}
}

static void playing_entry_changed_cb(RBShellPlayer *player,
			  RhythmDBEntry *entry,
			  RBDiscordPlugin *self) {
	cleanup_playing_entry(self);

	self->playing_entry = rhythmdb_entry_ref(entry);
	const char* title = rhythmdb_entry_get_string(self->playing_entry, RHYTHMDB_PROP_TITLE), MAX_TITLE_LENGTH;
	const char* artist = rhythmdb_entry_get_string(self->playing_entry, RHYTHMDB_PROP_ARTIST), MAX_DISCORD_STRING_LENGTH;
	unsigned long duration = rhythmdb_entry_get_ulong(self->playing_entry, RHYTHMDB_PROP_DURATION);

	rb_debug("entry changed! now listening to %s\n", title);

	time_t current_time = time(NULL);
	struct DiscordActivityTimestamps timestamps;
	timestamps.start = current_time;
	timestamps.end = current_time + duration;

	struct DiscordActivityAssets assets;

	struct DiscordActivity activity;
    memset(&activity, 0, sizeof(activity));
	activity.application_id = CLIENT_ID;
	activity.type = DiscordActivityType_Listening;
	activity.timestamps = timestamps;
	// activity.assets = assets;

	sprintf(activity.name, "Rhythmbox");
	sprintf(activity.details, title);
    sprintf(activity.state, "by %s", artist);

	self->app.activities->update_activity(self->app.activities, &activity, &self->app, UpdateActivityCallback);
}

static void
impl_activate (PeasActivatable *bplugin)
{
	RBDiscordPlugin *plugin;
	RBShell *shell;

	plugin = RB_DISCORD_PLUGIN (bplugin);
	g_object_get (plugin, "object", &shell, NULL);
	g_object_get (shell,
		      "shell-player", &plugin->shell_player,
		      "db", &plugin->db,
		      NULL);

	g_signal_connect_object(plugin->shell_player, "playing-song-changed", G_CALLBACK (playing_entry_changed_cb), plugin, 0);
	
	g_object_unref (shell);
}

static void
impl_deactivate	(PeasActivatable *bplugin)
{
	RBDiscordPlugin *self = RB_DISCORD_PLUGIN (bplugin);

	pthread_cancel(callback_thid);

	cleanup_playing_entry(self);
}

G_MODULE_EXPORT void
peas_register_types (PeasObjectModule *module)
{
	rb_sample_plugin_register_type (G_TYPE_MODULE (module));
	peas_object_module_register_extension_type (module,
						    PEAS_TYPE_ACTIVATABLE,
						    RB_TYPE_DISCORD_PLUGIN);
}
