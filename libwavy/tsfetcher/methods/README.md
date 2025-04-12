# Wavy Fetch Methods (WFM)

The goal for Wavy is for streaming Audio (**AS YOU SEE FIT**). To follow this ideology, obviously having a static class and implementation of
fetching transport streams from **Wavy-Server** is not going to cut it.

So here is **WFM**. It contains all the plugins currently available that should be able to add **YOUR** own implementation of `tsfetch` without a hassle.

Currently, only a basic **AGGRESSIVE** plugin is available.

## Plugins 

1. **AGGRESSIVE:**

AUTHOR: 
**nots1dd** (Siddharth Karanam)

LOGIC:
Loads **ALL** the transport streams into memory (with debug options to store the raw stream AND decoded stream in client's machine) before playback

NOTES:
This is more-or-less, a boilerplate naive implementation of what is possible with `libwavy::fetch::plugin`. There is **NO** ABR implementation in this and it always attempts to fetch the MAX bitrate stream found in the server.

CAVEATS:
For FLAC streams, there is an issue in fetching the streams (due to playlist file name errors)

## Contributing

Want to add more plugins with greater support? Read [plugin docs](https://github.com/oinkognito/wavy/blob/main/libwavy/tsfetcher/plugin/README.md) for more information on how the plugin works and what it expects from you.

It should be fairly easy to understand and get started. Any issues in understanding or in the plugin logic feel free to open an issue [here](https://github.com/oinkognito/wavy/issues)

### What is expected from a plugin?

This section is a **MUST READ** for people who are willing to make their own plugin for Wavy. 

**Coming soon**
