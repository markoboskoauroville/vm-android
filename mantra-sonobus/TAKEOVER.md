# TAKEOVER: Mantra SonoBus

**State 13.11.2026, v1 (first build, not yet released):** Mantra SonoBus = SonoBus' engine + one new screen
(`src/MantraEditor.*`). Tested on Linux in a cloud session, two "machines" in two network namespaces with dummy
JACK sound cards (alpha 10.9.0.1 = meeting point, beta 10.9.0.2):
- the strip (SEND left, HEAR right, one bar per channel, captions with machine names) at the bottom of the screen;
  click opens the panel; drag moves it;
- alpha hosts the meeting point inside the app, beta joins group `mantra-sonobus`, each lists the other (green);
- SEND on alpha → beta hears; RECEIVE on beta (saved) → alpha starts sending by itself after a restart;
- 16 channels: a click on alpha's input 5 arrives on beta's channel 5 only; beta at 2 channels hears it centred
  on L and R (mixdown).

**Not yet seen:** the macOS app and Mantra Bus drivers (built only by GitHub Actions, never run), the Android APK
(same), real Tailscale between real machines, the PipeWire bus on a real desktop, REC TO FILES, Tailscale discovery
(`tailscale status --json`, desktop only).

**Next:** Marko creates the empty public repo markoboskoauroville/MANTRA_SONOBUS → push → Actions builds v1 →
install on the Mac (install-mac.sh), chromium-vm (install-linux.sh), the Nothing Phone (APK) → first real link.

## How it works

- `scripts/prepare.py <sonobus>`: copies `src/` into `Source/mantra/`, makes `createEditor()` return
  `MantraEditor`, hands it the device manager (SonoStandaloneFilterWindow.h), renames to "Mantra SonoBus" /
  `com.mantra.sonobus` / `~/.config/mantra-sonobus`, builds Standalone only, replaces `-march=native` with
  `armv8.2-a` (GitHub's ARM runner is newer than Oracle's Ampere A1), and gives Android its id, signing, version.
  Every patch must match exactly: if SonoBus moves, the build stops with the line that no longer matches.
- Pinned upstream: SonoBus 35f1062 (1.7.2, Dec 2023, JUCE 7.0.8), BlackHole 6008d29.
- Engine settings applied by MantraEditor: one input group of N channels, send channels = "match inputs"
  (no mono/stereo folding), input monitor 0, codec per link from the table in `getKbpsPerChannel()`,
  every received peer = one channel group, dest channels 0..N straight through (N = 2: stereo pan / mixdown).
- Ports: peers UDP 11000 (fixed, so a direct link to any 100.x address works); meeting point server 10999.
- New peer: Mantra stops SonoBus' automatic sending at once (tracked by address); 1.5 s later, when the name is
  known, the saved `send.<name>` / `recv.<name>` come back. RECEIVE = withdraw + invite again 400 ms later.
- Settings: `mantra.settings` (name, channels, quality, meetingPoint, machines, send.*, recv.*, input/outputDevice)
  in `~/.config/mantra-sonobus` (Linux), `~/Library/Application Support/Mantra SonoBus` (Mac).
- Mantra Bus: Mac = two BlackHole builds (`mac/build-driver.sh`), Linux = two PipeWire null sinks
  (`linux/mantra-bus.conf`) or PulseAudio null sinks; the app reaches them on Linux through PipeWire's JACK
  (`pw-jack`, the launcher does it).
- Release: `.github/workflows/release.yml`, assets `mantra-sonobus-v<N>-{linux-x86_64,linux-arm64}.tar.gz`,
  `-macos.zip`, `-android.apk`. Bump `VERSION` for a new release.

## Local test rig (Linux)

    ninja -C build SonoBus_Standalone          # in a prepared sonobus checkout, ~2 min incremental
    # two machines: ip netns a/b + veth 10.9.0.1/2, Xvfb :78/:79, jackd -d dummy per netns,
    # HOME per machine with mantra.settings + "Mantra SonoBus.settings" (audioSetup deviceType JACK, device "system")
    # jack_metro into "Mantra SonoBus:in_N" on alpha; screenshots with `import -window root`, clicks with xdotool.
