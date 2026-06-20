# Discord Rich Presence Integration

![from_profile.png](https://i.imgur.com/k8p9GJT.png)

This is a component for the [foobar2000](https://www.foobar2000.org) audio player, which displays currently played track data via Discord Rich Presence. It supports displaying artwork through third-party hosting services with your own upload script, and has progress bar and ActivityType `Listening` support. 

This is a fork of [s0hv's artwork-focused foo_discord_rich fork](https://github.com/s0hv/foo_discord_rich) of the original [component by TheQwertiest](https://github.com/TheQwertiest/foo_discord_rich), porting changes to the original codebase by [SuperN64 and others in this fork](https://github.com/supern64/foo_discord_rich).

## Additional features
- **Artwork upload also works directly from archives!**
- 3 title formatting fields: details (1st line), state (2nd line), large cover image (3rd line, cover hover).
- Artwork upload mode: `normal`, `blurred`, `hidden`. You can set the artwork policy for any album, and if the receiving uploader script respects it, have it blurred to the percentage set in the preferences. There's also an hidden mode to completely skip upload for those and optionally use a fallback URL in the settings to use a custom image (or GIF) for them.
- 3 field references: `%drp_artwork_url%`, `%drp_artwork_mode%`, `%drp_blurred_artwork_url%`. This lets you filter albums with covers that have not been uploaded, match some substring, and most importantly what the policy is for them, e.g. `%drp_artwork_mode% IS hidden`
- Artwork URL override
- Automatic upload toggle, turned on by default (Library -> Discord Rich Presence -> Automatic Artwork Upload)
- Bundled [artwork uploader](https://github.com/wsnrq/drp_artwork_uploader) for imgur (API key), catbox
- `{filepath}` and `{url}` placeholders for custom command uploader script: pass the currently stored URL to the upload script that can determine whether the resource is still up, and if not, conveniently reupload it. If no placeholders are set by the user, it keeps the old behaviour of passing only {filepath} to stdin.

Releases are in multiarch format (x32 and x64). Component works as expected on WINE x64 and presumably does on other platforms too.