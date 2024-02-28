# Rhythmbox Discord RPC

Display the music you're listening to on Rhythmbox in Discord

![image](https://github.com/V3L0C1T13S/rhythmbox-discord-rpc/assets/51764975/2418200e-c2e7-4a51-80e6-1bd6a034a660)

## Building

1. Configure Meson

```sh
meson setup build
```

2. Compile
```sh
ninja -C build
```

## Installing

Manual installation is required as of now. To install the plugin, run the following command:

```sh
cp -r build /usr/lib/rhythmbox/plugins/rhythmbox-discord
```

Or use a symlink if you're going to frequently modify your build, and don't mind the slightly reduced security:

```sh
ln -s $PWD/build /usr/lib/rhythmbox/plugins/rhythmbox-discord
```

Note that your Rhythmbox installation location may vary based on distribution. If your install location is different from `/usr/lib`, please create an issue and specify your distribution so it can be added to the list.
