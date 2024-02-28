fs = import('fs')

sample_plugin_dir = sampleplugindir + '/rhythmbox-discord'

sample_sources = [
  'rb-discord.c'
]

discord_lib = 'discord_game_sdk'
discord_lib_dir = '/usr/lib'
discord_header_dir = './discord_game_sdk/c'

cc = meson.get_compiler('c')
discord_dep = cc.find_library(discord_lib, dirs: [discord_lib_dir])
inc_discord = include_directories(discord_header_dir)

#discord_dep = declare_dependency(
#   link_args : ['-L' + discord_lib_dir, '-l' + discord_lib],
#     include_directories : include_directories(discord_header_dir))

#discord_dep = dependency('discord-game-sdk',version: '>= 3.0.0',  required: true)

fs.copyfile('./discord_game_sdk/lib/x86_64/discord_game_sdk.so', 'discord_game_sdk.so')

shared_module('rhythmbox-discord',
  sample_sources,
  dependencies: [rhythmbox_core_dep, discord_dep],
  include_directories: [inc_discord],
  install_dir: sample_plugin_dir,
  install: true)

sample_plugin_descriptor = custom_target('rhythmbox-discord-plugin-descriptor',
  input: 'rhythmbox-discord.plugin.desktop.in',
  output: 'rhythmbox-discord.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: sample_plugin_dir)
