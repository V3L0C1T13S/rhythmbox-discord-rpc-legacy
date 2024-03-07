#pragma once

#include "rb-debug.h"
#include "rb-discord-status.h"
#include "rb-ext-db.h"
#include "rb-plugin-macros.h"
#include "rb-shell-player.h"
#include "rb-shell.h"

#pragma pack(push, 8)
#include "discord_game_sdk/c/discord_game_sdk.h"
#pragma pack(pop)

struct Application {
  struct IDiscordCore *core;
  struct IDiscordUsers *users;
  struct IDiscordApplicationManager *application;
  struct IDiscordActivityManager *activities;
};

typedef struct _RBDiscordPlugin {
  PeasExtensionBase parent;
  RBShellPlayer *shell_player;
  RhythmDB *db;
  RBExtDB *art_store;

  gchar *art_path;

  struct Application app;
  RhythmDBEntry *playing_entry;

  enum EDiscordResult last_discord_error;
  gboolean playing;
} RBDiscordPlugin;
