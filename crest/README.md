# crest

The desktop shell of [Fiordland Linux](https://fiordlandlinux.org), built on [Quickshell](https://quickshell.org).

crest is a fork of [Caelestia shell](https://github.com/caelestia-dots/shell) v2.5.0 by the Caelestia contributors, licensed under GPL-3.0. Thanks to them for the foundation.

## Changes from Caelestia

- Fiordland branding and wallpaper
- Grey and baby blue `fiordland` colour scheme
- Inter as the interface font
- Optional fastfetch when a terminal opens (Settings → Apps)
- `crest:` global shortcuts and `crest-` layer namespaces
- Fiordland defaults for apps and the calculator
- A Bluetooth pairing agent: pairing codes, PINs and connection requests show up as a prompt, so keyboards and phones pair from Settings → Connected devices without `bluetoothctl`

## Running

```sh
qs -c crest
```

Configuration lives in `~/.config/caelestia/shell.json` and uses the same options as Caelestia, so the upstream documentation applies.

## License

GPL-3.0-only, see [LICENSE](LICENSE).
