# momentaryupdates — Mantra SonoBus

## 13.11.2026 (cloud session, from vm-android)

> Now we need to develop Mantra Sonobus. You need to take the source code of Sonobus and make it very simple user interface. Through Tailscale I am connected to all of my machines and I need to have ability to simply send audio in any directions between my machines. User interface will be only VU meter at the bottom of the screen and when I click on a VU meter I can choose audio connections between my computers. I need to be able to do any combination of recordings, listening, sending and this Emulate Audio Driver similar as my CQ 10 mixer. Mixer, it has two modes, multitrack or stereo and then we work like this. So anything, it's like a virtual driver like a black hole. If I send in multi-channel mode from my Ableton Live audio on each channel of my Mantra Sono bus then I can choose that computer in my virtual machine and I can also say receive I choose this as input driver and I can record from one to the other. Please build something like this. So this is a Swift app and then it's an Android app and any platform I'm using. I'm using Android and I'm using Linux and I'm using MacOS. So three releases. Mantra, Sono, Bus, working through Tailscale. That's it. Similar reasons Mantra, Remote Viewer.

> (channels) add scaling options for channels 2, 4, 8, 16, 24, 32 and then adapt bandwidth to the count of the channels

**Status (13.11.2026):** v1 built and tested on Linux (two machines in network namespaces: meeting point, SEND,
RECEIVE, saved links, 16-channel channel-for-channel, stereo mixdown). Mac app + Mantra Bus drivers and Android APK
are built by GitHub Actions once the repo exists. One codebase (C++/JUCE, SonoBus' own) for all three, not Swift:
SonoBus is JUCE, and one codebase keeps the three releases identical.
