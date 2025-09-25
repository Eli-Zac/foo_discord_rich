# Discord Rich Presence Integration

![from_profile.png](https://i.imgur.com/k8p9GJT.png)

This is a component for the [foobar2000](https://www.foobar2000.org) audio player, which displays currently played track data via Discord Rich Presence. It supports displaying artwork through third-party hosting services with your own upload script, and has progress bar and ActivityType `Listening` support.

This is a fork of [s0hv's artwork-focused foo_discord_rich fork](https://github.com/s0hv/foo_discord_rich) of the original [component by TheQwertiest](https://github.com/TheQwertiest/foo_discord_rich), porting changes to the original codebase by [SuperN64 and others in this fork](https://github.com/supern64/foo_discord_rich), following [Discord's API changes](https://github.com/discord/discord-api-docs/pull/7033) allowing ActivityType to be set and displayed similar to Spotify.

In addition to the changes above, this fork adds support to pass named arguments to the upload command being executed, such as API keys, and the {filepath} and {url} placeholders. The latter lets you pass the currently stored URL to the upload script that can determine whether the resource is still up, and if not, conveniently reupload it. If no placeholders are set by the user, it keeps the old behaviour of passing only {filepath} to stdin.

This component has been tested in both x32 and win64, the former of which in WINE also, and seems to work as expected. The user should still exercise caution due to the precarious state of the dependencies (including the long-deprectated discord-rpc library and its own outdated deps) especially with rpc bridges.