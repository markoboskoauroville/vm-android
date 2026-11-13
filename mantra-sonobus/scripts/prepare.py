#!/usr/bin/env python3
"""Turn a SonoBus checkout into Mantra SonoBus.

    python3 scripts/prepare.py <sonobus-dir> [--version N]

Copies src/ into <sonobus-dir>/Source/mantra/ and patches the upstream files in place:
the editor becomes MantraEditor, names and bundle ids become Mantra's (so it lives beside a real
SonoBus), the Linux ARM build stops using -march=native, the Android project gets Mantra's id,
signing and icon. Every patch must apply exactly once: a miss stops the build (upstream moved).
"""
import pathlib, re, shutil, sys

ROOT = pathlib.Path(__file__).resolve().parent.parent


def patch(path, old, new, count=1, regex=False):
    """count=-1: at least once."""
    text = path.read_text()
    if regex:
        new_text, n = re.subn(old, new, text)
    else:
        n = text.count(old)
        new_text = text.replace(old, new)
    if (count >= 0 and n != count) or (count < 0 and n == 0):
        sys.exit(f"prepare: patch for {path} expected {count} match(es) of {old!r}, found {n}")
    path.write_text(new_text)


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    sb = pathlib.Path(sys.argv[1]).resolve()
    version = "1"
    if "--version" in sys.argv:
        version = sys.argv[sys.argv.index("--version") + 1]
    else:
        vf = ROOT / "VERSION"
        if vf.exists():
            version = vf.read_text().strip()

    src = sb / "Source"
    dest = src / "mantra"
    if dest.exists():
        shutil.rmtree(dest)
    shutil.copytree(ROOT / "src", dest)

    # 1. the editor
    p = src / "SonobusPluginProcessor.cpp"
    patch(p, '#include "SonobusPluginEditor.h"', '#include "SonobusPluginEditor.h"\n#include "mantra/MantraEditor.h"')
    patch(p, "return new SonobusAudioProcessorEditor (*this);", "return new MantraEditor (*this);")

    # 2. the standalone window hands the device manager to MantraEditor
    p = src / "SonoStandaloneFilterWindow.h"
    patch(p, '#include "SonobusPluginEditor.h"', '#include "SonobusPluginEditor.h"\n#include "mantra/MantraEditor.h"')
    patch(p, "                // hack to allow editor to get devicemanager\n",
          "                if (auto * mantra = dynamic_cast<MantraEditor*>(editor.get()))\n"
          "                    mantra->getAudioDeviceManager = [this]() { return &owner.getDeviceManager(); };\n"
          "                // hack to allow editor to get devicemanager\n")

    # 3. settings beside, not inside, a real SonoBus
    p = src / "SonoStandaloneFilterApp.cpp"
    patch(p, 'options.osxLibrarySubFolder = "Application Support/SonoBus";',
          'options.osxLibrarySubFolder = "Application Support/Mantra SonoBus";')
    patch(p, 'options.folderName          = "~/.config/sonobus";',
          'options.folderName          = "~/.config/mantra-sonobus";')

    # 4. CMake: our source, our names, standalone only, portable ARM code
    p = sb / "CMakeLists.txt"
    patch(p, "    set(SourceFiles\n", "    set(SourceFiles\n        Source/mantra/MantraEditor.cpp\n        Source/mantra/MantraEditor.h\n")
    patch(p, 'COMPANY_NAME "Sonosaurus"', 'COMPANY_NAME "Mantra"')
    patch(p, 'BUNDLE_ID "com.Sonosaurus.SonoBus"', 'BUNDLE_ID "com.mantra.sonobus"')
    patch(p, 'DESCRIPTION "SonoBus - Network Audio"', 'DESCRIPTION "Mantra SonoBus - audio between your machines over Tailscale"')
    patch(p, "PLUGIN_MANUFACTURER_CODE Sono", "PLUGIN_MANUFACTURER_CODE Mntr")
    patch(p, 'project(SonoBus VERSION 1.7.2)', f'project(SonoBus VERSION 1.7.{version})')
    patch(p, 'sono_add_custom_plugin_target(SonoBus SonoBus "${FormatsToBuild}" FALSE  "NBus")',
          'sono_add_custom_plugin_target(SonoBus "Mantra SonoBus" "Standalone" FALSE  "MBus")')
    patch(p, 'sono_add_custom_plugin_target(SonoBusInst "SonoBusInstrument" "VST3" TRUE  "IBus")',
          '# (Mantra: no instrument plugin)')
    patch(p, "-march=native", "-march=armv8.2-a", count=-1)
    patch(p, "OUTPUT_NAME ${tmptargname}", "OUTPUT_NAME mantra-sonobus")

    # 5. Android
    a = sb / "mobile" / "Builds" / "Android"
    p = a / "app" / "CMakeLists.txt"
    patch(p, '    "../../../../Source/SonobusPluginEditor.cpp"\n',
          '    "../../../../Source/SonobusPluginEditor.cpp"\n'
          '    "../../../../Source/mantra/MantraEditor.cpp"\n'
          '    "../../../../Source/mantra/MantraEditor.h"\n')
    p = a / "app" / "build.gradle"
    patch(p, 'applicationId "com.sonosaurus.sonobus"', 'applicationId "com.mantra.sonobus"\n        versionCode ' + version + '\n        versionName "' + version + '"')
    patch(p, r"(?s)signingConfigs \{.*?\n    \}\n", (
        "signingConfigs {\n"
        "        mantra {\n"
        "            storeFile     file(System.getenv('DEBUG_KEYSTORE') ?: \"${System.properties['user.home']}/.android/debug.keystore\")\n"
        "            storePassword \"android\"\n"
        "            keyAlias      \"androiddebugkey\"\n"
        "            keyPassword   \"android\"\n"
        "        }\n"
        "    }\n"), regex=True)
    patch(p, "signingConfig signingConfigs.juceSigning", "signingConfig signingConfigs.mantra", count=2)
    patch(p, '\nndkVersion "22.0.7026061"\n', "\n")
    patch(p, '        release_ {\n            externalNativeBuild {',
          '        release_ {\n            ndk {\n                abiFilters "arm64-v8a", "armeabi-v7a"\n            }\n            externalNativeBuild {')
    p = a / "app" / "src" / "main" / "AndroidManifest.xml"
    patch(p, 'android:authorities="com.sonosaurus.sonobus.sharingcontentprovider"',
          'android:authorities="com.mantra.sonobus.sharingcontentprovider"')
    patch(p, r'android:versionCode="\d+"\s+android:versionName="[^"]*"',
          f'android:versionCode="{version}" android:versionName="{version}"', regex=True)
    for flavour in ("debug", "release"):
        patch(a / "app" / "src" / flavour / "res" / "values" / "string.xml",
              '<string name="app_name">SonoBus</string>', '<string name="app_name">Mantra SonoBus</string>')

    print(f"prepare: {sb} is now Mantra SonoBus v{version}")


if __name__ == "__main__":
    main()
