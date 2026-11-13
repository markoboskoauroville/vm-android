// Mantra SonoBus: the whole user interface.
// A strip of VU meters (what this machine SENDS on the left, what it HEARS on the right).
// A click on the strip opens the panel: channels (2..32), quality, input and output devices,
// and every machine on the tailnet with SEND and RECEIVE switches.
// The engine underneath is SonoBus (SonobusAudioProcessor), unchanged.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "JuceHeader.h"
#include <map>

class SonobusAudioProcessor;
class MantraPanel;

class MantraEditor : public juce::AudioProcessorEditor,
                     private juce::Timer
{
public:
    explicit MantraEditor (SonobusAudioProcessor&);
    ~MantraEditor() override;

    // Set by the standalone window (patched SonoStandaloneFilterWindow.h).
    std::function<juce::AudioDeviceManager*()> getAudioDeviceManager;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    //== used by the panel ==================================================
    static constexpr int kChannelChoices[] = { 2, 4, 8, 16, 24, 32 };
    enum Quality { QualityLight = 0, QualityAuto, QualityHigh, QualityLossless };

    struct Machine {
        juce::String name;      // Tailscale / computer name, the key
        juce::String host;      // Tailscale IP (or name)
        int  peerIndex = -1;    // index in the engine, -1 = not linked
        bool online = false;    // seen on Tailscale
        bool manual = false;    // added by hand
    };

    SonobusAudioProcessor& getProcessor() { return proc; }
    juce::PropertiesFile& getSettings() { return *settings; }

    int  getChannels() const { return channels; }
    void setChannels (int n);
    int  getQuality() const { return quality; }
    void setQuality (int q);
    int  getKbpsPerChannel() const;          // 0 for lossless
    juce::String describeBandwidth() const;  // "64 kbps/ch x 16 = 1.0 Mbit/s"

    juce::Array<Machine> getMachines() const { return machines; }
    void connectMachine (const Machine&);
    void forgetMachine (const juce::String& name);
    void addManualMachine (const juce::String& hostOrName);
    bool getSend (const Machine&) const;
    bool getRecv (const Machine&) const;
    void setSend (const Machine&, bool);
    void setRecv (const Machine&, bool);
    float getPeerRecvLevel (int peerIndex) const;

    juce::String getMyName() const { return myName; }
    void setMyName (const juce::String&);
    juce::String getMeetingPoint() const { return meetingPoint; }
    void setMeetingPoint (const juce::String&);
    juce::String getMeetingStatus() const { return meetingStatus; }

    void applyAudioSetup();   // device channels = current channel count
    bool isRecording() const;
    void toggleRecording();
    juce::String getLastRecording() const { return lastRecording; }

    void closePanel();

    static constexpr int kPeerPort    = 11000;  // every Mantra SonoBus listens here (UDP)
    static constexpr int kServerPort  = 10999;  // SonoBus' built-in meeting server
    static constexpr const char* kGroup = "mantra-sonobus";
    static constexpr const char* kDefaultMeetingPoint = "100.102.86.30";  // chromium-vm on Tailscale

private:
    void timerCallback() override;
    void housekeeping();
    void applyEngine();
    void applyCodecToPeer (int index);
    void enforceReceiveLayout (int index);
    void refreshTailscale();
    void rebuildMachines();
    void ensureMeetingPoint();
    void makeStrip();
    juce::String peerKey (int index) const;
    int  findPeer (const juce::String& hostColonPort) const;
    void requestReceive (int index);
    int  formatIndexFor (int ch, int q) const;
    bool isLocalAddress (const juce::String& host) const;
    void togglePanel();

    struct Listener;
    std::unique_ptr<Listener> listener;

    SonobusAudioProcessor& proc;
    std::unique_ptr<juce::PropertiesFile> settings;
    std::unique_ptr<MantraPanel> panel;
    std::unique_ptr<juce::DocumentWindow> panelWindow;

    int channels = 2;
    int quality = QualityAuto;
    juce::String myName, meetingPoint, meetingStatus { "not connected" };
    juce::String lastRecording;

    juce::Array<Machine> machines;
    juce::StringArray tailscaleNames, tailscaleHosts;   // online peers from `tailscale status --json`
    juce::StringArray knownPeerKeys;                    // peers (host:port) already seen
    std::map<juce::String, juce::uint32> firstSeen;     // peer key -> when it appeared (0 = settled)
    std::map<juce::String, juce::uint32> lastDial;      // machine -> last direct call

    bool stripMade = false;
    bool serverHosting = false;
    bool joinRequested = false;
    juce::uint32 lastServerAttempt = 0;
    int tick = 0;

    juce::Point<int> dragStart, dragTopStart;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MantraEditor)
};
