# LESSONS: Mantra SonoBus

- **An AOO source ignores an invite while the sink is still in its list** (aoo source.cpp handle_invite:
  "sink already added"). `setRemotePeerSendActive(false)` only stops the source, the sink stays: a later RECEIVE
  from the other side then does nothing. Withdraw first (uninvite removes the sink), invite again ~400 ms later.
- **A SonoBus peer's name arrives after the peer.** Keyed by name, a peer looked new twice (IP, then name), and the
  second "new peer: mute" cancelled the other side's invite. Track peers by host:port; apply saved links by name.
- **SonoBus starts sending to every new peer** (`sendActive = !mainSendMute`). Muting globally would also refuse
  invites (sendAllow); Mantra stops each new peer once, right when it appears.
- **SonoBus connected to a server only accepts invites from peers in its group**; without a server it accepts anyone.
- **SonoBus builds ARM Linux with -march=native**: on GitHub's ARM runner (Cobalt 100, armv9) that would not run on
  Oracle's Ampere A1 (Neoverse N1, armv8.2). prepare.py pins armv8.2-a.
- **SonoBus' built-in server listens on 10999**, so peers use 11000.
- Channel groups: dest count 2 = stereo pan (mixdown), any other count = straight through channel for channel
  (ChannelGroup::processPan).
- `jack_lsp | grep` in a for loop splits "Mantra SonoBus:in_1" at the space; quote port names.
