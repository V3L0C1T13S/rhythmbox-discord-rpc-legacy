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

#include <assert.h>
#include <glib-object.h>
#include <glib.h>
#include <glib/gi18n-lib.h>
#include <gmodule.h>
#include <gtk/gtk.h>
#include <stdio.h>
#include <string.h> /* For strlen */
#include <time.h>

#include "rb-debug.h"
#include "rb-plugin-macros.h"
#include "rb-shell-player.h"
#include "rb-shell.h"

#include "rb-discord-status.h"

#include <pthread.h>
#include <unistd.h>

#pragma pack(push, 8)
#include "discord_game_sdk/c/discord_game_sdk.h"
#pragma pack(pop)

#define RB_TYPE_DISCORD_PLUGIN (rb_discord_plugin_get_type())
#define RB_DISCORD_PLUGIN(o)                                                   \
  (G_TYPE_CHECK_INSTANCE_CAST((o), RB_TYPE_DISCORD_PLUGIN, RBDiscordPlugin))
#define RB_DISCORD_PLUGIN_CLASS(k)                                             \
  (G_TYPE_CHECK_CLASS_CAST((k), RB_TYPE_DISCORD_PLUGIN, RBDiscordPluginClass))
#define RB_IS_DISCORD_PLUGIN(o)                                                \
  (G_TYPE_CHECK_INSTANCE_TYPE((o), RB_TYPE_DISCORD_PLUGIN))
#define RB_IS_DISCORD_PLUGIN_CLASS(k)                                          \
  (G_TYPE_CHECK_CLASS_TYPE((k), RB_TYPE_DISCORD_PLUGIN))
#define RB_DISCORD_PLUGIN_GET_CLASS(o)                                         \
  (G_TYPE_INSTANCE_GET_CLASS((o), RB_TYPE_DISCORD_PLUGIN, RBDiscordPluginClass))

struct Application {
  struct IDiscordCore *core;
  struct IDiscordUsers *users;
  struct IDiscordApplicationManager *application;
  struct IDiscordActivityManager *activities;
};

typedef struct {
  PeasExtensionBase parent;
  RBShellPlayer *shell_player;
  RhythmDB *db;
  struct Application app;
  RhythmDBEntry *playing_entry;
  enum EDiscordResult last_discord_error;
} RBDiscordPlugin;

typedef struct {
  PeasExtensionBaseClass parent_class;
} RBDiscordPluginClass;

pthread_t callback_thid;

enum EDiscordResult get_last_discord_error(RBDiscordPlugin *self) {
  return self->last_discord_error;
}

void set_last_discord_error(RBDiscordPlugin *self, enum EDiscordResult err) {
  self->last_discord_error = err;
}

#define CLIENT_ID 1212075104486301696
#define PLUGIN_NAME "Discord Status"

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module);

static void rb_discord_plugin_init(RBDiscordPlugin *plugin);

RB_DEFINE_PLUGIN(RB_TYPE_DISCORD_PLUGIN, RBDiscordPlugin, rb_discord_plugin, )

#define DISCORD_REQUIRE(x) assert(x == DiscordResult_Ok)

void DISCORD_CALLBACK UpdateActivityCallback(void *data,
                                             enum EDiscordResult result) {
  if (result == DiscordResult_Ok)
    rb_debug("successfully set activity!");
  else
    rb_debug("Failed to set activity! Status: %i", result);
}

void *run_callbacks(void *plugin) {
  RBDiscordPlugin *self = (RBDiscordPlugin *)plugin;

  for (;;) {
    DISCORD_REQUIRE(self->app.core->run_callbacks(self->app.core));

    usleep(16 * 1000);
  }

  pthread_exit(0);
}

EInitDiscordStatus init_discord(RBDiscordPlugin *self) {
  memset(&self->app, 0, sizeof(self->app));

  struct IDiscordActivityEvents activities_events;
  memset(&activities_events, 0, sizeof(activities_events));

  struct DiscordCreateParams params;
  params.client_id = CLIENT_ID;
  params.flags = DiscordCreateFlags_Default;
  params.event_data = &self->app;
  params.activity_events = &activities_events;
  params.flags = DiscordCreateFlags_NoRequireDiscord;

  enum EDiscordResult result =
      DiscordCreate(DISCORD_VERSION, &params, &self->app.core);

  if (result != DiscordResult_Ok) {
    set_last_discord_error(self, result);
    return DiscordRbStatus_Discord_Error;
  }

  self->app.application =
      self->app.core->get_application_manager(self->app.core);
  self->app.activities = self->app.core->get_activity_manager(self->app.core);

  if (pthread_create(&callback_thid, NULL, run_callbacks, self) != 0) {
    perror("pthread_create() error");
    return DiscordRbStatus_Pthread_CreateFail;
  }

  return DiscordRbStatus_Ok;
}

static void rb_discord_plugin_init(RBDiscordPlugin *self) {
  rb_debug("RBDiscordPlugin initialising");

  EInitDiscordStatus status = init_discord(self);

  switch (status) {
  case DiscordRbStatus_Discord_Error: {
    rb_debug("Failed to init discord instance: %i",
             get_last_discord_error(self));
    // exit(1);

    break;
  }
  case DiscordRbStatus_Pthread_CreateFail: {
    rb_debug("failed to create pthread!");
    exit(1);

    break;
  }
  default:
    break;
  }
}

void cleanup_playing_entry(RBDiscordPlugin *self) {
  if (self->playing_entry != NULL) {
    rhythmdb_entry_unref(self->playing_entry);
    self->playing_entry = NULL;
  }
}

void update_activity(RBDiscordPlugin *self, struct DiscordActivity *activity) {
  if (self->app.activities != NULL)
    self->app.activities->update_activity(self->app.activities, activity,
                                          &self->app, UpdateActivityCallback);
}

struct DiscordActivity create_activity(const char *title, const char *artist,
                                       unsigned long *duration, bool playing) {
  struct DiscordActivity activity;
  struct DiscordActivityAssets assets;

  memset(&activity, 0, sizeof(activity));

  if (duration != NULL) {
    struct DiscordActivityTimestamps timestamps;
    time_t current_time = time(NULL);

    timestamps.start = current_time;
    timestamps.end = current_time + *duration;
    activity.timestamps = timestamps;
  }

  if (!playing) {
    sprintf(assets.small_image, "pause");
    sprintf(assets.small_text, "Paused");
  }

  activity.application_id = CLIENT_ID;
  activity.type = DiscordActivityType_Listening;
  sprintf(activity.name, "Rhythmbox");
  sprintf(activity.details, title);
  sprintf(activity.state, "by %s", artist);
  sprintf(assets.large_image, "logo");
  sprintf(assets.large_text, "Rhythmbox");

  activity.assets = assets;

  return activity;
}
static void playing_entry_changed_cb(RBShellPlayer *player,
                                     RhythmDBEntry *entry,
                                     RBDiscordPlugin *self) {
  cleanup_playing_entry(self);

  self->playing_entry = rhythmdb_entry_ref(entry);
  const char *title =
      rhythmdb_entry_get_string(self->playing_entry, RHYTHMDB_PROP_TITLE);
  const char *artist =
      rhythmdb_entry_get_string(self->playing_entry, RHYTHMDB_PROP_ARTIST);
  unsigned long duration =
      rhythmdb_entry_get_ulong(self->playing_entry, RHYTHMDB_PROP_DURATION);

  struct DiscordActivity activity =
      create_activity(title, artist, &duration, true);

  update_activity(self, &activity);
}

static void playing_changed_cb(RBShellPlayer *player, gboolean playing,
                               RBDiscordPlugin *self) {
  if (self->playing_entry != NULL) {
    const char *title =
        rhythmdb_entry_get_string(self->playing_entry, RHYTHMDB_PROP_TITLE);
    const char *artist =
        rhythmdb_entry_get_string(self->playing_entry, RHYTHMDB_PROP_ARTIST);
    unsigned long duration =
        rhythmdb_entry_get_ulong(self->playing_entry, RHYTHMDB_PROP_DURATION);

    struct DiscordActivity activity =
        create_activity(title, artist, (playing ? &duration : NULL), playing);

    update_activity(self, &activity);
  }
}

static void impl_activate(PeasActivatable *bplugin) {
  RBDiscordPlugin *plugin;
  RBShell *shell;

  plugin = RB_DISCORD_PLUGIN(bplugin);
  g_object_get(plugin, "object", &shell, NULL);
  g_object_get(shell, "shell-player", &plugin->shell_player, "db", &plugin->db,
               NULL);

  g_signal_connect_object(plugin->shell_player, "playing-song-changed",
                          G_CALLBACK(playing_entry_changed_cb), plugin, 0);
  g_signal_connect_object(plugin->shell_player, "playing-changed",
                          G_CALLBACK(playing_changed_cb), plugin, 0);

  g_object_unref(shell);
}

static void impl_deactivate(PeasActivatable *bplugin) {
  RBDiscordPlugin *self = RB_DISCORD_PLUGIN(bplugin);

  pthread_cancel(callback_thid);
  cleanup_playing_entry(self);
  if (self->app.core != NULL) {
    self->app.core->destroy(self->app.core);
  }
}

G_MODULE_EXPORT void peas_register_types(PeasObjectModule *module) {
  rb_discord_plugin_register_type(G_TYPE_MODULE(module));
  peas_object_module_register_extension_type(module, PEAS_TYPE_ACTIVATABLE,
                                             RB_TYPE_DISCORD_PLUGIN);
}
