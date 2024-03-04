project('rhythmbox-discord-rpc', 'c')

fs = import('fs')

sample_plugin_dir = 'rhythmbox-discord'

sample_sources = [
  'rb-discord.c'
]

discord_lib = 'discord_game_sdk'
discord_lib_dir = '/usr/lib'
discord_header_dir = './discord_game_sdk/c'

cc = meson.get_compiler('c')
cdata = configuration_data()

discord_dep = cc.find_library(discord_lib, dirs: [discord_lib_dir])
inc_discord = include_directories(discord_header_dir)

libpeas = dependency('libpeas-1.0', version: '>= 0.7.3', required: true)
libpeas_gtk = dependency('libpeas-gtk-1.0', version: '>= 0.7.3', required: true)

rhythmbox_dep = dependency('rhythmbox', required: true)

rhythmboxinc = include_directories('/usr/include/rhythmbox')
pluginsinc = include_directories('/usr/include/rhythmbox/plugins')
libinc = include_directories('/usr/include/rhythmbox/lib')
shellinc = include_directories('/usr/include/rhythmbox/shell')
metadatainc = include_directories('/usr/include/rhythmbox/metadata')


fs.copyfile('./discord_game_sdk/lib/x86_64/discord_game_sdk.so', 'discord_game_sdk.so')

po_dir = meson.project_source_root() / 'po'
# FIXME: Move custom_target to i18n.merge_file when gettext acquires plugin support.
#        http://lists.gnu.org/archive/html/bug-gettext/2017-06/msg00001.html
msgfmt_plugin_cmd = [
  find_program('msgfmt'),
  '--desktop',
  '--keyword=Name',
  '--keyword=Description',
  '--template=@INPUT@',
  '-d', po_dir,
  '--output=@OUTPUT@'
]

cdata.set('GLIB_VERSION_MIN_REQUIRED', 'GLIB_VERSION_2_66')
cdata.set('GLIB_VERSION_MAX_ALLOWED', 'GLIB_VERSION_2_66')

cdata.set('GETTEXT_PACKAGE', '"rhythmbox"')
cdata.set('PACKAGE', '"rhythmbox"')
cdata.set('VERSION', '@0@'.format(meson.project_version()))

configure_file(input : 'config.h.meson',
  output : 'config.h',
configuration : cdata)

shared_module('rhythmbox-discord',
  sample_sources,
  dependencies: [rhythmbox_dep, discord_dep, libpeas, libpeas_gtk],
  include_directories: [inc_discord, libinc, pluginsinc, shellinc, rhythmboxinc, metadatainc],
  install_dir: sample_plugin_dir,
  install: true)

sample_plugin_descriptor = custom_target('rhythmbox-discord-plugin-descriptor',
  input: 'rhythmbox-discord.plugin.desktop.in',
  output: 'rhythmbox-discord.plugin',
  command: msgfmt_plugin_cmd,
  install: true,
  install_dir: sample_plugin_dir)
