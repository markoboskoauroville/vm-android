// Mantra SonoBus: the whole user interface (see MantraEditor.h).
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MantraEditor.h"
#include "../SonobusPluginProcessor.h"
#include <limits>

using namespace juce;

#if JUCE_ANDROID || JUCE_IOS
 #define MANTRA_MOBILE 1
#else
 #define MANTRA_MOBILE 0
#endif

namespace MantraColours
{
    static const Colour bg       { 0xff0b0d10 };
    static const Colour panel    { 0xff15181d };
    static const Colour line     { 0xff262b33 };
    static const Colour text     { 0xffd8dde4 };
    static const Colour dim      { 0xff7d8794 };
    static const Colour send     { 0xff3fb6ff };   // what leaves this machine
    static const Colour hear     { 0xff4ade80 };   // what arrives
    static const Colour warn     { 0xfffacc15 };
    static const Colour clip     { 0xffef4444 };
    static const Colour on       { 0xff2563eb };
}

static float levelToY (float gain)   // 0..1 for -60..0 dB
{
    const float db = Decibels::gainToDecibels (gain, -60.0f);
    return jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
}

static String shortName (const String& s)
{
    // "chromium-vm.tail1234.ts.net." -> "chromium-vm"
    return s.upToFirstOccurrenceOf (".", false, false).trim();
}

//==============================================================================
// Server callbacks arrive on network threads: hop to the message thread.
struct MantraEditor::Listener : public SonobusAudioProcessor::ClientListener
{
    explicit Listener (MantraEditor& e) : owner (&e) {}

    void aooClientConnected (SonobusAudioProcessor*, bool success, const String& err) override
    {
        post ([success, err] (MantraEditor& ed) {
            if (success) {
                ed.meetingStatus = "met, joining group";
                ed.proc.setAutoconnectToGroupPeers (true);
                ed.proc.joinServerGroup (MantraEditor::kGroup, "", false);
                ed.joinRequested = true;
            } else {
                ed.meetingStatus = "no meeting point (" + err + ")";
            }
        });
    }
    void aooClientDisconnected (SonobusAudioProcessor*, bool, const String&) override
    {
        post ([] (MantraEditor& ed) { ed.meetingStatus = "meeting point lost"; ed.joinRequested = false; });
    }
    void aooClientGroupJoined (SonobusAudioProcessor*, bool success, const String& group, const String& err) override
    {
        post ([success, group, err] (MantraEditor& ed) {
            ed.meetingStatus = success ? ("in group " + group) : ("group refused: " + err);
        });
    }
    void aooClientError (SonobusAudioProcessor*, const String& err) override
    {
        post ([err] (MantraEditor& ed) { ed.meetingStatus = "error: " + err; });
    }

    void post (std::function<void (MantraEditor&)> fn)
    {
        auto safe = owner;
        MessageManager::callAsync ([safe, fn] { if (auto* e = safe.getComponent()) fn (*e); });
    }

    Component::SafePointer<MantraEditor> owner;
};

//==============================================================================
// The panel: everything you can choose, in one column.
class MantraPanel : public Component, private Timer
{
public:
    explicit MantraPanel (MantraEditor& e) : ed (e)
    {
        auto styleLabel = [this] (Label& l, const String& t, float size, Colour c) {
            l.setText (t, dontSendNotification);
            l.setFont (Font (size, Font::bold));
            l.setColour (Label::textColourId, c);
            addAndMakeVisible (l);
        };

        styleLabel (title, "MANTRA SONOBUS", 18.0f, MantraColours::text);
        styleLabel (nameLabel, "THIS MACHINE", 11.0f, MantraColours::dim);
        styleLabel (chanLabel, "CHANNELS", 11.0f, MantraColours::dim);
        styleLabel (qualLabel, "QUALITY", 11.0f, MantraColours::dim);
        styleLabel (bwLabel, "", 12.0f, MantraColours::warn);
        styleLabel (inLabel, "SEND FROM (input)", 11.0f, MantraColours::send);
        styleLabel (outLabel, "HEAR / RECORD TO (output)", 11.0f, MantraColours::hear);
        styleLabel (machLabel, "MACHINES ON TAILSCALE", 11.0f, MantraColours::dim);
        styleLabel (meetLabel, "MEETING POINT", 11.0f, MantraColours::dim);
        styleLabel (meetStatus, "", 11.0f, MantraColours::dim);
        styleLabel (recInfo, "", 11.0f, MantraColours::dim);

        setupEdit (nameEdit, ed.getMyName());
        nameEdit.onReturnKey = nameEdit.onFocusLost = [this] { ed.setMyName (nameEdit.getText()); };

        for (int i = 0; i < 6; ++i) {
            const int n = MantraEditor::kChannelChoices[i];
            auto* b = chanButtons.add (new TextButton (n == 2 ? "2 ST" : String (n)));
            b->setClickingTogglesState (false);
            b->onClick = [this, n] { ed.setChannels (n); refresh(); };
            addAndMakeVisible (b);
        }
        const char* qnames[] = { "LIGHT", "AUTO", "HIGH", "LOSSLESS" };
        for (int i = 0; i < 4; ++i) {
            auto* b = qualButtons.add (new TextButton (qnames[i]));
            b->onClick = [this, i] { ed.setQuality (i); refresh(); };
            addAndMakeVisible (b);
        }

        addAndMakeVisible (typeBox);
        addAndMakeVisible (inBox);
        addAndMakeVisible (outBox);
        typeBox.onChange = [this] { chooseType(); };
        inBox.onChange   = [this] { chooseDevice (true); };
        outBox.onChange  = [this] { chooseDevice (false); };

        addAndMakeVisible (viewport);
        viewport.setViewedComponent (&list, false);
        viewport.setScrollBarsShown (true, false);

        setupEdit (addEdit, "");
        addEdit.setTextToShowWhenEmpty ("add machine: tailscale name or 100.x.y.z", MantraColours::dim);
        addEdit.onReturnKey = [this] { addMachine(); };
        addButton.setButtonText ("ADD");
        addButton.onClick = [this] { addMachine(); };
        addAndMakeVisible (addButton);

        setupEdit (meetEdit, ed.getMeetingPoint());
        meetEdit.setTextToShowWhenEmpty ("none (direct links only)", MantraColours::dim);
        meetEdit.onReturnKey = meetEdit.onFocusLost = [this] { ed.setMeetingPoint (meetEdit.getText()); };

        recButton.onClick = [this] { ed.toggleRecording(); refresh(); };
        addAndMakeVisible (recButton);
        closeButton.setButtonText ("CLOSE");
        closeButton.onClick = [this] { ed.closePanel(); };
        addAndMakeVisible (closeButton);
       #if ! MANTRA_MOBILE
        quitButton.setButtonText ("QUIT");
        quitButton.onClick = [] { JUCEApplicationBase::quit(); };
        addAndMakeVisible (quitButton);
       #endif

        refreshDevices();
        refresh();
        startTimerHz (8);
    }

    void paint (Graphics& g) override
    {
        g.fillAll (MantraColours::panel);
        g.setColour (MantraColours::line);
        for (auto y : separators)
            g.drawHorizontalLine (y, 12.0f, (float) getWidth() - 12.0f);
    }

    void resized() override
    {
        separators.clearQuick();
        auto r = getLocalBounds().reduced (12, 10);
        const int rowH = MANTRA_MOBILE ? 40 : 28;
        const int lab = 16;

        auto top = r.removeFromTop (rowH);
        closeButton.setBounds (top.removeFromRight (70).reduced (0, 2));
        title.setBounds (top);
        r.removeFromTop (6);

        nameLabel.setBounds (r.removeFromTop (lab));
        nameEdit.setBounds (r.removeFromTop (rowH).reduced (0, 2));
        sep (r);

        chanLabel.setBounds (r.removeFromTop (lab));
        {
            auto row = r.removeFromTop (rowH);
            const int w = row.getWidth() / 6;
            for (auto* b : chanButtons) b->setBounds (row.removeFromLeft (w).reduced (2, 2));
        }
        qualLabel.setBounds (r.removeFromTop (lab));
        {
            auto row = r.removeFromTop (rowH);
            const int w = row.getWidth() / 4;
            for (auto* b : qualButtons) b->setBounds (row.removeFromLeft (w).reduced (2, 2));
        }
        bwLabel.setBounds (r.removeFromTop (lab + 2));
        sep (r);

        if (typeBox.getNumItems() > 1) {
            typeBox.setVisible (true);
            typeBox.setBounds (r.removeFromTop (rowH).reduced (0, 2));
        } else {
            typeBox.setVisible (false);
        }
        inLabel.setBounds (r.removeFromTop (lab));
        inBox.setBounds (r.removeFromTop (rowH).reduced (0, 2));
        outLabel.setBounds (r.removeFromTop (lab));
        outBox.setBounds (r.removeFromTop (rowH).reduced (0, 2));
        sep (r);

        // bottom part first, the machine list takes what is left
        auto bottom = r.removeFromBottom (rowH);
        recButton.setBounds (bottom.removeFromLeft (bottom.getWidth() / 2).reduced (2, 2));
        if (quitButton.isVisible()) quitButton.setBounds (bottom.reduced (2, 2));
        recInfo.setBounds (r.removeFromBottom (lab));
        meetStatus.setBounds (r.removeFromBottom (lab));
        meetEdit.setBounds (r.removeFromBottom (rowH).reduced (0, 2));
        meetLabel.setBounds (r.removeFromBottom (lab));
        r.removeFromBottom (6);
        {
            auto row = r.removeFromBottom (rowH);
            addButton.setBounds (row.removeFromRight (60).reduced (2, 2));
            addEdit.setBounds (row.reduced (0, 2));
        }

        machLabel.setBounds (r.removeFromTop (lab));
        viewport.setBounds (r);
        layoutList();
    }

    void refresh()
    {
        const int n = ed.getChannels();
        for (int i = 0; i < chanButtons.size(); ++i)
            paintToggle (*chanButtons[i], MantraEditor::kChannelChoices[i] == n);
        for (int i = 0; i < qualButtons.size(); ++i)
            paintToggle (*qualButtons[i], ed.getQuality() == i);
        bwLabel.setText (ed.describeBandwidth(), dontSendNotification);

        const bool rec = ed.isRecording();
        recButton.setButtonText (rec ? "STOP RECORDING" : "REC TO FILES");
        paintToggle (recButton, rec, MantraColours::clip);
        recInfo.setText (ed.getLastRecording(), dontSendNotification);
        meetStatus.setText (ed.getMeetingStatus(), dontSendNotification);
    }

private:
    //== one machine ========================================================
    struct Row : public Component
    {
        Row (MantraPanel& p, const MantraEditor::Machine& m) : panel (p), machine (m)
        {
            name.setText (m.name, dontSendNotification);
            name.setFont (Font (14.0f, Font::bold));
            name.setColour (Label::textColourId, MantraColours::text);
            name.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (name);

            host.setText (m.host == m.name ? String() : m.host, dontSendNotification);
            host.setFont (Font (11.0f));
            host.setColour (Label::textColourId, MantraColours::dim);
            host.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (host);

            send.setButtonText ("SEND >");
            recv.setButtonText ("< RECEIVE");
            send.onClick = [this] { panel.ed.setSend (machine, ! panel.ed.getSend (machine)); };
            recv.onClick = [this] { panel.ed.setRecv (machine, ! panel.ed.getRecv (machine)); };
            addAndMakeVisible (send);
            addAndMakeVisible (recv);

            link.setButtonText (m.peerIndex >= 0 ? "x" : "LINK");
            link.setTooltip (m.peerIndex >= 0 ? "unlink" : "link to this machine");
            link.onClick = [this] {
                if (machine.peerIndex >= 0) panel.ed.forgetMachine (machine.name);
                else panel.ed.connectMachine (machine);
            };
            addAndMakeVisible (link);
            update();
        }

        void update()
        {
            const bool linked = machine.peerIndex >= 0;
            send.setEnabled (linked);
            recv.setEnabled (linked);
            panel.paintToggle (send, linked && panel.ed.getSend (machine), MantraColours::send.darker (0.3f));
            panel.paintToggle (recv, linked && panel.ed.getRecv (machine), MantraColours::hear.darker (0.4f));
            level = linked ? panel.ed.getPeerRecvLevel (machine.peerIndex) : 0.0f;
            repaint();
        }

        void paint (Graphics& g) override
        {
            auto r = getLocalBounds().reduced (0, 2);
            g.setColour (MantraColours::bg);
            g.fillRoundedRectangle (r.toFloat(), 4.0f);

            // status dot: green linked, blue online, grey known
            const auto dotC = machine.peerIndex >= 0 ? MantraColours::hear
                            : machine.online ? MantraColours::send : MantraColours::dim;
            g.setColour (dotC);
            g.fillEllipse ((float) r.getX() + 8.0f, (float) r.getCentreY() - 4.0f, 8.0f, 8.0f);

            // incoming level under the name
            if (level > 0.0f) {
                g.setColour (MantraColours::hear.withAlpha (0.7f));
                g.fillRect (Rectangle<float> ((float) r.getX() + 24.0f, (float) r.getBottom() - 4.0f,
                                              (float) (nameW - 30) * levelToY (level), 2.0f));
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (4, 4);
            link.setBounds (r.removeFromRight (44).reduced (1, 1));
            recv.setBounds (r.removeFromRight (MANTRA_MOBILE ? 96 : 82).reduced (1, 1));
            send.setBounds (r.removeFromRight (MANTRA_MOBILE ? 80 : 66).reduced (1, 1));
            nameW = r.getWidth();
            r.removeFromLeft (20);
            name.setBounds (r.removeFromTop (r.getHeight() * 3 / 5));
            host.setBounds (r);
        }

        MantraPanel& panel;
        MantraEditor::Machine machine;
        Label name, host;
        TextButton send, recv, link;
        float level = 0.0f;
        int nameW = 100;
    };

    void timerCallback() override
    {
        auto machines = ed.getMachines();
        String sig;
        for (auto& m : machines) sig << m.name << "|" << m.host << "|" << m.peerIndex << "|" << (int) m.online << ";";
        if (sig != lastSig) {
            lastSig = sig;
            rows.clear();
            for (auto& m : machines) {
                auto* row = rows.add (new Row (*this, m));
                list.addAndMakeVisible (row);
            }
            layoutList();
        }
        for (auto* row : rows) row->update();
        if (! meetEdit.hasKeyboardFocus (true)) meetEdit.setText (ed.getMeetingPoint(), false);
        refresh();
        if (++ticks % 16 == 0) refreshDevices();
    }

    void layoutList()
    {
        const int h = MANTRA_MOBILE ? 56 : 44;
        const int w = viewport.getMaximumVisibleWidth();
        list.setSize (w, jmax (h, rows.size() * h));
        int y = 0;
        for (auto* row : rows) { row->setBounds (0, y, w, h); y += h; }
        if (rows.isEmpty()) emptyHint (true);
        else emptyHint (false);
    }

    void emptyHint (bool show)
    {
        if (show && hint == nullptr) {
            hint = std::make_unique<Label>();
            hint->setText ("No other Mantra SonoBus yet. Start it on another machine, "
                           "or add one below.", dontSendNotification);
            hint->setColour (Label::textColourId, MantraColours::dim);
            hint->setFont (Font (12.0f));
            hint->setJustificationType (Justification::centred);
            list.addAndMakeVisible (*hint);
        }
        if (hint != nullptr) {
            hint->setVisible (show);
            hint->setBounds (list.getLocalBounds());
        }
    }

    void addMachine()
    {
        ed.addManualMachine (addEdit.getText());
        addEdit.clear();
    }

    void setupEdit (TextEditor& t, const String& text)
    {
        t.setText (text, false);
        t.setColour (TextEditor::backgroundColourId, MantraColours::bg);
        t.setColour (TextEditor::textColourId, MantraColours::text);
        t.setColour (TextEditor::outlineColourId, MantraColours::line);
        t.setFont (Font (14.0f));
        t.setIndents (6, 4);
        addAndMakeVisible (t);
    }

    void paintToggle (Button& b, bool on, Colour onColour = MantraColours::on)
    {
        b.setColour (TextButton::buttonColourId, on ? onColour : MantraColours::bg);
        b.setColour (TextButton::textColourOffId, on ? Colours::white : MantraColours::dim);
    }

    void sep (Rectangle<int>& r)
    {
        r.removeFromTop (5);
        separators.add (r.getY());
        r.removeFromTop (6);
    }

    void refreshDevices()
    {
        auto* dm = ed.getAudioDeviceManager ? ed.getAudioDeviceManager() : nullptr;
        if (dm == nullptr) return;

        auto fill = [] (ComboBox& box, const StringArray& names, const String& current) {
            if (box.isPopupActive()) return;
            StringArray old;
            for (int i = 0; i < box.getNumItems(); ++i) old.add (box.getItemText (i));
            if (old != names) {
                box.clear (dontSendNotification);
                box.addItemList (names, 1);
            }
            box.setSelectedItemIndex (names.indexOf (current), dontSendNotification);
        };

        StringArray types;
        for (auto* t : dm->getAvailableDeviceTypes()) types.add (t->getTypeName());
        fill (typeBox, types, dm->getCurrentAudioDeviceType());

        if (auto* type = dm->getCurrentDeviceTypeObject()) {
            auto setup = dm->getAudioDeviceSetup();
            fill (inBox, type->getDeviceNames (true), setup.inputDeviceName);
            fill (outBox, type->getDeviceNames (false), setup.outputDeviceName);
        }
        resized();
    }

    void chooseType()
    {
        if (auto* dm = ed.getAudioDeviceManager ? ed.getAudioDeviceManager() : nullptr) {
            dm->setCurrentAudioDeviceType (typeBox.getText(), true);
            ed.applyAudioSetup();
            refreshDevices();
        }
    }

    void chooseDevice (bool input)
    {
        if (auto* dm = ed.getAudioDeviceManager ? ed.getAudioDeviceManager() : nullptr) {
            auto setup = dm->getAudioDeviceSetup();
            if (input) setup.inputDeviceName = inBox.getText();
            else       setup.outputDeviceName = outBox.getText();
            const auto err = dm->setAudioDeviceSetup (setup, true);
            if (err.isNotEmpty())
                bwLabel.setText (err, dontSendNotification);
            ed.applyAudioSetup();
            ed.getSettings().setValue (input ? "inputDevice" : "outputDevice",
                                       input ? setup.inputDeviceName : setup.outputDeviceName);
        }
    }

    MantraEditor& ed;
    Label title, nameLabel, chanLabel, qualLabel, bwLabel, inLabel, outLabel, machLabel, meetLabel, meetStatus, recInfo;
    TextEditor nameEdit, addEdit, meetEdit;
    OwnedArray<TextButton> chanButtons, qualButtons;
    ComboBox typeBox, inBox, outBox;
    Viewport viewport;
    Component list;
    OwnedArray<Row> rows;
    std::unique_ptr<Label> hint;
    TextButton addButton, recButton, closeButton, quitButton;
    Array<int> separators;
    String lastSig;
    int ticks = 0;

    friend struct Row;
};

//==============================================================================
class MantraPanelWindow : public DocumentWindow
{
public:
    MantraPanelWindow (MantraEditor& e)
        : DocumentWindow ("Mantra SonoBus", MantraColours::panel, DocumentWindow::closeButton), ed (e)
    {
        setUsingNativeTitleBar (true);
        setResizable (true, false);
    }
    void closeButtonPressed() override { ed.closePanel(); }
    MantraEditor& ed;
};

//==============================================================================
MantraEditor::MantraEditor (SonobusAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    PropertiesFile::Options opts;
    opts.applicationName     = "mantra";
    opts.filenameSuffix      = ".settings";
    opts.osxLibrarySubFolder = "Application Support/Mantra SonoBus";
   #if JUCE_LINUX
    opts.folderName          = "~/.config/mantra-sonobus";
   #endif
    settings = std::make_unique<PropertiesFile> (opts);

   #if JUCE_ANDROID
    const String defName = SystemStats::getDeviceDescription().replaceCharacter (' ', '-');
   #else
    const String defName = shortName (SystemStats::getComputerName());
   #endif
    myName       = settings->getValue ("name", defName.isNotEmpty() ? defName : "mantra");
    channels     = settings->getIntValue ("channels", 2);
    quality      = settings->getIntValue ("quality", QualityAuto);
    meetingPoint = settings->getValue ("meetingPoint", kDefaultMeetingPoint);

    bool validChannels = false;
    for (auto c : kChannelChoices) validChannels |= (c == channels);
    if (! validChannels) channels = 2;

    listener = std::make_unique<Listener> (*this);
    proc.addClientListener (listener.get());

    proc.setCurrentUsername (myName);
    proc.setUseSpecificUdpPort (kPeerPort);
    applyEngine();

    setOpaque (true);
   #if MANTRA_MOBILE
    setSize (400, 700);
   #else
    setResizeLimits (200, 24, 10000, 400);
    setSize (900, settings->getIntValue ("stripHeight", 44));
   #endif
    startTimerHz (30);
}

MantraEditor::~MantraEditor()
{
    stopTimer();
    proc.removeClientListener (listener.get());
    panelWindow.reset();
    panel.reset();
    settings->saveIfNeeded();
}

//== the strip ===============================================================
void MantraEditor::paint (Graphics& g)
{
    g.fillAll (MantraColours::bg);

    auto r = getLocalBounds();
   #if MANTRA_MOBILE
    auto strip = r.removeFromBottom (jmin (120, getHeight() / 5));
    if (panel == nullptr || ! panel->isVisible()) {
        g.setColour (MantraColours::dim);
        g.setFont (Font (22.0f, Font::bold));
        g.drawFittedText ("MANTRA SONOBUS", r.reduced (20), Justification::centred, 1);
        g.setFont (Font (14.0f));
        g.drawFittedText ("tap the meters", r.reduced (20).translated (0, 34), Justification::centred, 1);
    }
   #else
    auto strip = r;
   #endif

    auto drawBank = [&g] (Rectangle<int> area, foleys::LevelMeterSource& src, int n, Colour c,
                          const String& caption)
    {
        g.setColour (MantraColours::panel);
        g.fillRect (area);
        auto bars = area.reduced (3, 3);
        const int have = jmin (n, src.getNumChannels());
        const float slot = (float) bars.getWidth() / (float) jmax (1, n);
        const float gap = slot > 6.0f ? 2.0f : 1.0f;

        for (int ch = 0; ch < n; ++ch) {
            const float x = (float) bars.getX() + slot * (float) ch;
            const auto col = Rectangle<float> (x, (float) bars.getY(), slot - gap, (float) bars.getHeight());
            g.setColour (MantraColours::line);
            g.fillRect (col);
            if (ch >= have) continue;
            const float rms  = levelToY (src.getRMSLevel (ch));
            const float peak = levelToY (src.getMaxLevel (ch));
            auto fill = col.withTop (col.getBottom() - col.getHeight() * rms);
            g.setColour (rms > 0.95f ? MantraColours::clip : rms > 0.8f ? MantraColours::warn : c);
            g.fillRect (fill);
            const float py = col.getBottom() - col.getHeight() * peak;
            g.setColour (peak > 0.98f ? MantraColours::clip : c.brighter (0.6f));
            g.fillRect (col.getX(), py, col.getWidth(), 1.5f);
        }

        g.setColour (Colours::white.withAlpha (0.75f));
        g.setFont (Font ((float) jlimit (10, 14, area.getHeight() / 3), Font::bold));
        g.drawText (caption, area.reduced (6, 2), Justification::topLeft, true);
    };

    // names for the captions
    String to, from;
    for (int i = 0; i < proc.getNumberRemotePeers(); ++i) {
        if (proc.getRemotePeerSendActive (i)) to   << (to.isEmpty() ? "" : ", ")   << peerKey (i);
        if (proc.getRemotePeerRecvActive (i)) from << (from.isEmpty() ? "" : ", ") << peerKey (i);
    }

    const int half = strip.getWidth() / 2;
    auto left = strip.removeFromLeft (half).withTrimmedRight (2);
    drawBank (left,  proc.getSendMeterSource(),   channels, MantraColours::send,
              "SEND " + String (channels) + (to.isNotEmpty() ? "  > " + to : String()));
    drawBank (strip, proc.getOutputMeterSource(), channels, MantraColours::hear,
              "HEAR " + String (channels) + (from.isNotEmpty() ? "  < " + from : String()));

    if (isRecording()) {
        g.setColour (MantraColours::clip);
        g.fillEllipse ((float) getWidth() - 14.0f, 4.0f, 9.0f, 9.0f);
    }
}

void MantraEditor::resized()
{
   #if MANTRA_MOBILE
    if (panel != nullptr)
        panel->setBounds (getLocalBounds().withTrimmedBottom (jmin (120, getHeight() / 5)).reduced (6));
   #endif
}

void MantraEditor::mouseDown (const MouseEvent& e)
{
    dragging = false;
    dragStart = e.getScreenPosition();
    if (auto* top = getTopLevelComponent()) dragTopStart = top->getPosition();
}

void MantraEditor::mouseDrag (const MouseEvent& e)
{
   #if ! MANTRA_MOBILE
    // dragging the strip moves its window; a click opens the panel
    if (e.getDistanceFromDragStart() > 6) dragging = true;
    if (dragging)
        if (auto* top = getTopLevelComponent())
            top->setTopLeftPosition (dragTopStart + (e.getScreenPosition() - dragStart));
   #else
    ignoreUnused (e);
   #endif
}

void MantraEditor::mouseUp (const MouseEvent&)
{
    if (! dragging) togglePanel();
    dragging = false;
}

void MantraEditor::togglePanel()
{
    if (panel != nullptr && panel->isVisible()) { closePanel(); return; }

    if (panel == nullptr) panel = std::make_unique<MantraPanel> (*this);

   #if MANTRA_MOBILE
    addAndMakeVisible (*panel);
    resized();
   #else
    auto* display = Desktop::getInstance().getDisplays().getDisplayForPoint (Desktop::getMousePosition());
    auto area = display != nullptr ? display->userArea : Rectangle<int> (0, 0, 1280, 800);
    const int w = 480, h = jmin (720, area.getHeight() - getHeight() - 60);
    if (panelWindow == nullptr) {
        panelWindow = std::make_unique<MantraPanelWindow> (*this);
        panel->setSize (w, h);
        panelWindow->setContentNonOwned (panel.get(), true);   // the window takes the panel's size
    }
    auto strip = getScreenBounds();
    int x = jlimit (area.getX(), area.getRight() - w, Desktop::getMousePosition().x - w / 2);
    int y = strip.getY() > area.getCentreY() ? strip.getY() - panelWindow->getHeight() - 8
                                             : strip.getBottom() + 8;
    panelWindow->setTopLeftPosition (x, jlimit (area.getY(), area.getBottom() - panelWindow->getHeight(), y));
    panelWindow->setAlwaysOnTop (true);
    panelWindow->setVisible (true);
    panelWindow->toFront (true);
   #endif
    repaint();
}

void MantraEditor::closePanel()
{
    if (panel == nullptr) return;
   #if MANTRA_MOBILE
    panel->setVisible (false);
   #else
    if (panelWindow != nullptr) panelWindow->setVisible (false);
   #endif
    settings->saveIfNeeded();
    repaint();
}

void MantraEditor::makeStrip()
{
   #if ! MANTRA_MOBILE
    auto* dw = findParentComponentOfClass<DocumentWindow>();
    if (dw == nullptr) return;
    auto* display = Desktop::getInstance().getDisplays().getPrimaryDisplay();
    if (display == nullptr) return;
    const auto area = display->userArea;
    const int h = settings->getIntValue ("stripHeight", 44);

    dw->setUsingNativeTitleBar (false);
    dw->setTitleBarHeight (0);
    dw->setResizable (false, false);
    setSize (area.getWidth(), h);
    dw->setBounds (area.getX(), area.getBottom() - h, area.getWidth(), h);
    dw->setAlwaysOnTop (true);
   #endif
    stripMade = true;
}

//== engine ==================================================================
void MantraEditor::timerCallback()
{
    repaint();
    if (++tick % 3 == 0) housekeeping();   // 10 times a second
}

void MantraEditor::housekeeping()
{
    if (! stripMade && getAudioDeviceManager != nullptr) {
        makeStrip();
        applyAudioSetup();
    }

    const auto now = Time::getMillisecondCounter();
    const int npeers = proc.getNumberRemotePeers();

    // A peer is tracked by its address (its name arrives a moment later).
    // First sight: SonoBus starts sending to every new peer; Mantra stops that at once.
    // 1.5 s later, when the name is known, the saved SEND / RECEIVE of that machine come back.
    StringArray live;
    for (int i = 0; i < npeers; ++i) {
        String host; int port = 0;
        if (! proc.getRemotePeerAddressInfo (i, host, port)) continue;
        const String id = host + ":" + String (port);
        live.add (id);

        if (! knownPeerKeys.contains (id)) {
            knownPeerKeys.add (id);
            firstSeen[id] = now;
            proc.setRemotePeerSendActive (i, false);
            applyCodecToPeer (i);
        }
        auto& seen = firstSeen[id];
        const bool named = proc.getRemotePeerUserName (i).isNotEmpty();
        if (seen != 0 && now - seen > 1500 && (named || now - seen > 5000)) {
            seen = 0;
            const auto key = peerKey (i);
            if (settings->getBoolValue ("send." + key, false) || settings->getBoolValue ("send." + host, false)) {
                proc.setRemotePeerSendActive (i, true);
                applyCodecToPeer (i);
            }
            if (settings->getBoolValue ("recv." + key, false) || settings->getBoolValue ("recv." + host, false))
                requestReceive (i);
        }
        enforceReceiveLayout (i);
    }
    // forget peers that went away, so they get the same treatment when they return
    for (int k = knownPeerKeys.size(); --k >= 0;)
        if (! live.contains (knownPeerKeys[k])) { firstSeen.erase (knownPeerKeys[k]); knownPeerKeys.remove (k); }

    if (tick % 30 == 0) {           // every 3 s
        ensureMeetingPoint();
        // relink machines that have saved links but are not linked (direct, without a meeting point)
        for (auto& m : machines)
            if (m.peerIndex < 0 && m.online
                && (settings->getBoolValue ("send." + m.name, false) || settings->getBoolValue ("recv." + m.name, false))
                && now - lastDial[m.name] > 20000) {
                lastDial[m.name] = now;
                connectMachine (m);
            }
    }
   #if ! MANTRA_MOBILE
    if (tick % 300 == 3) refreshTailscale();   // every 30 s (and at start)
   #endif
    rebuildMachines();
}

String MantraEditor::peerKey (int index) const
{
    auto name = proc.getRemotePeerUserName (index).trim();
    if (name.isNotEmpty()) return name;
    String host; int port = 0;
    if (proc.getRemotePeerAddressInfo (index, host, port)) return host;
    return {};
}

int MantraEditor::getKbpsPerChannel() const
{
    // the bitrate per channel falls as the channel count rises, so 32 channels stay near 1.5 Mbit/s
    static const int table[3][6] = {
        {  64,  48, 48, 24, 24, 16 },   // LIGHT
        { 128,  96, 96, 64, 48, 48 },   // AUTO
        { 256, 160, 128, 96, 96, 64 },  // HIGH
    };
    if (quality == QualityLossless) return 0;
    int idx = 0;
    for (int i = 0; i < 6; ++i) if (kChannelChoices[i] == channels) idx = i;
    return table[jlimit (0, 2, quality)][idx];
}

String MantraEditor::describeBandwidth() const
{
    const int kbps = getKbpsPerChannel();
    const double total = kbps > 0 ? kbps * channels : 768.0 * channels;   // PCM 16 bit at 48 kHz
    const String tot = total >= 1000.0 ? String (total / 1000.0, 1) + " Mbit/s" : String ((int) total) + " kbit/s";
    if (kbps == 0)
        return "PCM 16 bit x " + String (channels) + " = " + tot + " per link (LAN-class links only)";
    return "Opus " + String (kbps) + " kbps/ch x " + String (channels) + " = " + tot + " per link";
}

int MantraEditor::formatIndexFor (int, int q) const
{
    const int want = getKbpsPerChannel() * 1000;
    int best = proc.getDefaultAudioCodecFormat(), bestDiff = std::numeric_limits<int>::max();
    for (int i = 0; i < proc.getNumberAudioCodecFormats(); ++i) {
        SonobusAudioProcessor::AudioCodecFormatInfo info;
        if (! proc.getAudioCodeFormatInfo (i, info)) continue;
        if (q == QualityLossless) {
            if (info.codec == SonobusAudioProcessor::CodecPCM && info.bitdepth == 2) return i;
            continue;
        }
        if (info.codec != SonobusAudioProcessor::CodecOpus) continue;
        const int diff = std::abs (info.bitrate - want);
        if (diff < bestDiff) { bestDiff = diff; best = i; }
    }
    return best;
}

void MantraEditor::applyCodecToPeer (int index)
{
    proc.setRemotePeerAudioCodecFormat (index, formatIndexFor (channels, quality));
}

void MantraEditor::applyEngine()
{
    // send exactly the input channels, no mono/stereo folding
    if (auto* param = proc.getValueTreeState().getParameter (SonobusAudioProcessor::paramSendChannels))
        param->setValueNotifyingHost (param->convertTo0to1 (0.0f));

    int ins = channels;
    if (auto* dm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr)
        if (auto* dev = dm->getCurrentAudioDevice())
            ins = dev->getActiveInputChannels().countNumberOfSetBits();
    const int n = jlimit (1, channels, ins > 0 ? ins : channels);

    proc.setInputGroupCount (1);
    proc.setInputGroupChannelStartAndCount (0, 0, n);
    proc.setInputGroupName (0, myName);
    proc.setInputMonitor (0, 0.0f);   // never hear your own input through Mantra

    const int fmt = formatIndexFor (channels, quality);
    proc.setDefaultAudioCodecFormat (fmt);
    for (int i = 0; i < proc.getNumberRemotePeers(); ++i) {
        proc.setRemotePeerAudioCodecFormat (i, fmt);
        enforceReceiveLayout (i);
    }
}

void MantraEditor::enforceReceiveLayout (int index)
{
    // whatever arrives lands channel for channel on the outputs;
    // with 2 channels (or a stereo output) a multitrack sender is mixed down to stereo
    const int rc = proc.getRemotePeerRecvChannelCount (index);
    if (rc <= 0) return;
    const int outs = jmax (1, proc.getTotalNumOutputChannels());
    const int dest = (channels <= 2 || outs <= 2) ? jmin (2, outs) : jmin (channels, outs);

    if (proc.getRemotePeerChannelGroupCount (index) != 1)
        proc.setRemotePeerChannelGroupCount (index, 1);

    int start = 0, count = 0;
    if (proc.getRemotePeerChannelGroupStartAndCount (index, 0, start, count) && (start != 0 || count != rc))
        proc.setRemotePeerChannelGroupStartAndCount (index, 0, 0, rc);
    if (proc.getRemotePeerChannelGroupDestStartAndCount (index, 0, start, count) && (start != 0 || count != dest))
        proc.setRemotePeerChannelGroupDestStartAndCount (index, 0, 0, dest);
}

void MantraEditor::applyAudioSetup()
{
    auto* dm = getAudioDeviceManager ? getAudioDeviceManager() : nullptr;
    if (dm == nullptr) return;

    auto setup = dm->getAudioDeviceSetup();
    // first start: the devices saved by Mantra
    if (! stripMade) {
        if (auto in = settings->getValue ("inputDevice"); in.isNotEmpty()) setup.inputDeviceName = in;
        if (auto out = settings->getValue ("outputDevice"); out.isNotEmpty()) setup.outputDeviceName = out;
    }
    int maxIn = channels, maxOut = channels;
    if (auto* dev = dm->getCurrentAudioDevice()) {
        maxIn  = dev->getInputChannelNames().size();
        maxOut = dev->getOutputChannelNames().size();
    }
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    setup.inputChannels.clear();
    setup.outputChannels.clear();
    setup.inputChannels.setRange (0, jmin (channels, jmax (0, maxIn)), true);
    setup.outputChannels.setRange (0, jmin (channels, jmax (1, maxOut)), true);
    dm->setAudioDeviceSetup (setup, true);

    // the device may have changed: ask again how many inputs there really are
    if (auto* dev = dm->getCurrentAudioDevice()) {
        const int realIn  = dev->getInputChannelNames().size();
        const int realOut = dev->getOutputChannelNames().size();
        if (realIn > maxIn || realOut > maxOut) {
            setup = dm->getAudioDeviceSetup();
            setup.inputChannels.clear();
            setup.outputChannels.clear();
            setup.inputChannels.setRange (0, jmin (channels, realIn), true);
            setup.outputChannels.setRange (0, jmin (channels, jmax (1, realOut)), true);
            dm->setAudioDeviceSetup (setup, true);
        }
    }
    applyEngine();
}

void MantraEditor::setChannels (int n)
{
    channels = n;
    settings->setValue ("channels", n);
    applyAudioSetup();
    applyEngine();
}

void MantraEditor::setQuality (int q)
{
    quality = jlimit (0, 3, q);
    settings->setValue ("quality", quality);
    applyEngine();
}

void MantraEditor::setMyName (const String& n)
{
    auto name = n.trim().replaceCharacter (' ', '-');
    if (name.isEmpty() || name == myName) return;
    myName = name;
    settings->setValue ("name", myName);
    // a name change needs a fresh login at the meeting point
    if (proc.isConnectedToServer()) proc.disconnectFromServer();
    proc.setCurrentUsername (myName);
    proc.setInputGroupName (0, myName);
    lastServerAttempt = 0;
}

//== meeting point (SonoBus server on one machine of the tailnet) ===========
bool MantraEditor::isLocalAddress (const String& host) const
{
    if (host == "127.0.0.1" || host == "localhost") return true;
    Array<IPAddress> addrs;
    IPAddress::findAllAddresses (addrs, false);
    for (auto& a : addrs) if (a.toString() == host) return true;
    return shortName (host).equalsIgnoreCase (shortName (SystemStats::getComputerName()));
}

void MantraEditor::setMeetingPoint (const String& mp)
{
    auto v = mp.trim();
    if (v == meetingPoint) return;
    meetingPoint = v;
    settings->setValue ("meetingPoint", meetingPoint);
    if (proc.isConnectedToServer()) proc.disconnectFromServer();
    if (serverHosting) { proc.stopAooServer(); serverHosting = false; }
    meetingStatus = meetingPoint.isEmpty() ? "direct links only" : "connecting";
    lastServerAttempt = 0;
}

void MantraEditor::ensureMeetingPoint()
{
    if (meetingPoint.isEmpty()) { meetingStatus = "direct links only"; return; }

    const auto host = meetingPoint.upToFirstOccurrenceOf (":", false, false);
    const int port = meetingPoint.containsChar (':') ? meetingPoint.fromLastOccurrenceOf (":", false, false).getIntValue()
                                                     : kServerPort;

    // the machine that IS the meeting point runs the server inside the app
    if (! serverHosting && isLocalAddress (host)) {
        proc.startAooServer();
        serverHosting = true;
    }

    if (proc.isConnectedToServer()) {
        if (! joinRequested) {
            proc.setAutoconnectToGroupPeers (true);
            proc.joinServerGroup (kGroup, "", false);
            joinRequested = true;
        }
        if (serverHosting && ! meetingStatus.contains ("here"))
            meetingStatus = meetingStatus + " (hosted here)";
        return;
    }

    const auto now = Time::getMillisecondCounter();
    if (lastServerAttempt != 0 && now - lastServerAttempt < 15000) return;
    lastServerAttempt = now;
    joinRequested = false;
    proc.setCurrentUsername (myName);
    meetingStatus = "calling " + host + ":" + String (port);
    proc.connectToServer (host, port, myName, "");
}

//== machines ================================================================
void MantraEditor::refreshTailscale()
{
    Component::SafePointer<MantraEditor> safe (this);
    Thread::launch ([safe] {
        const char* candidates[] = {
            "/Applications/Tailscale.app/Contents/MacOS/Tailscale",
            "/usr/local/bin/tailscale", "/opt/homebrew/bin/tailscale", "/usr/bin/tailscale",
            "/usr/sbin/tailscale", "tailscale" };
        String out;
        for (auto* exe : candidates) {
            if (String (exe).startsWithChar ('/') && ! File (exe).existsAsFile()) continue;
            ChildProcess cp;
            if (! cp.start (StringArray { exe, "status", "--json" }, ChildProcess::wantStdOut)) continue;
            out = cp.readAllProcessOutput();
            cp.waitForProcessToFinish (3000);
            if (out.trimStart().startsWithChar ('{')) break;
            out.clear();
        }
        if (out.isEmpty()) return;

        StringArray names, hosts;
        String self;
        auto json = JSON::parse (out);
        if (auto* s = json["Self"].getDynamicObject())
            self = shortName (s->getProperty ("DNSName").toString());
        if (auto* peers = json["Peer"].getDynamicObject()) {
            for (auto& p : peers->getProperties()) {
                auto& v = p.value;
                if (! (bool) v["Online"]) continue;
                auto ips = v["TailscaleIPs"];
                if (! ips.isArray() || ips.size() == 0) continue;
                auto name = shortName (v["DNSName"].toString());
                if (name.isEmpty()) name = v["HostName"].toString();
                // phones, Macs and Linux boxes; skip Tailscale's own exit nodes and such
                names.add (name);
                hosts.add (ips[0].toString());
            }
        }
        MessageManager::callAsync ([safe, names, hosts, self] {
            if (auto* ed = safe.getComponent()) {
                ed->tailscaleNames = names;
                ed->tailscaleHosts = hosts;
                // first start without a saved name: be known by the Tailscale name
                if (self.isNotEmpty() && ! ed->settings->containsKey ("name") && ed->myName != self)
                    ed->setMyName (self);
            }
        });
    });
}

void MantraEditor::rebuildMachines()
{
    Array<Machine> list;
    auto findBy = [&list] (const String& name, const String& host) -> Machine* {
        for (auto& m : list)
            if ((name.isNotEmpty() && m.name.equalsIgnoreCase (name))
                || (host.isNotEmpty() && m.host == host))
                return &m;
        return nullptr;
    };

    for (int i = 0; i < proc.getNumberRemotePeers(); ++i) {
        Machine m;
        String host; int port = 0;
        proc.getRemotePeerAddressInfo (i, host, port);
        m.host = host;
        m.name = peerKey (i);
        m.peerIndex = i;
        if (m.name.isNotEmpty()) list.add (m);
    }
    for (int i = 0; i < tailscaleNames.size(); ++i) {
        if (auto* m = findBy (tailscaleNames[i], tailscaleHosts[i])) {
            m->online = true;
            if (m->name == m->host) m->name = tailscaleNames[i];   // a bare IP gets its Tailscale name
            continue;
        }
        Machine m;
        m.name = tailscaleNames[i];
        m.host = tailscaleHosts[i];
        m.online = true;
        list.add (m);
    }
    for (auto& entry : StringArray::fromTokens (settings->getValue ("machines"), ",", "")) {
        auto e = entry.trim();
        if (e.isEmpty()) continue;
        if (auto* m = findBy (e, e)) { m->manual = true; continue; }
        Machine m;
        m.name = e; m.host = e; m.manual = true; m.online = true;   // unknown: try it when asked
        list.add (m);
    }
    // linked first, then online, then the rest, each by name
    std::sort (list.begin(), list.end(), [] (const Machine& a, const Machine& b) {
        const int ra = a.peerIndex >= 0 ? 0 : a.online ? 1 : 2;
        const int rb = b.peerIndex >= 0 ? 0 : b.online ? 1 : 2;
        if (ra != rb) return ra < rb;
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    machines = list;
}

void MantraEditor::connectMachine (const Machine& m)
{
    if (m.host.isEmpty()) return;
    proc.connectRemotePeer (m.host, kPeerPort, "", "", true);
}

void MantraEditor::forgetMachine (const String& name)
{
    for (int i = proc.getNumberRemotePeers(); --i >= 0;)
        if (peerKey (i) == name) proc.removeRemotePeer (i);
    settings->setValue ("send." + name, false);
    settings->setValue ("recv." + name, false);
    auto list = StringArray::fromTokens (settings->getValue ("machines"), ",", "");
    list.removeString (name);
    settings->setValue ("machines", list.joinIntoString (","));
}

void MantraEditor::addManualMachine (const String& hostOrName)
{
    auto e = hostOrName.trim();
    if (e.isEmpty()) return;
    auto list = StringArray::fromTokens (settings->getValue ("machines"), ",", "");
    list.addIfNotAlreadyThere (e);
    settings->setValue ("machines", list.joinIntoString (","));
    Machine m; m.name = e; m.host = e;
    connectMachine (m);
}

bool MantraEditor::getSend (const Machine& m) const
{
    return m.peerIndex >= 0 && m.peerIndex < proc.getNumberRemotePeers() && proc.getRemotePeerSendActive (m.peerIndex);
}

bool MantraEditor::getRecv (const Machine& m) const
{
    return m.peerIndex >= 0 && m.peerIndex < proc.getNumberRemotePeers() && proc.getRemotePeerRecvActive (m.peerIndex);
}

void MantraEditor::setSend (const Machine& m, bool on)
{
    if (m.peerIndex < 0 || m.peerIndex >= proc.getNumberRemotePeers()) return;
    proc.setRemotePeerSendActive (m.peerIndex, on);
    if (on) applyCodecToPeer (m.peerIndex);
    settings->setValue ("send." + m.name, on);
}

void MantraEditor::setRecv (const Machine& m, bool on)
{
    // RECEIVE asks the other machine to send: it works from this side alone
    if (m.peerIndex < 0 || m.peerIndex >= proc.getNumberRemotePeers()) return;
    if (on) requestReceive (m.peerIndex);
    else    proc.setRemotePeerRecvActive (m.peerIndex, false);
    settings->setValue ("recv." + m.name, on);
}

int MantraEditor::findPeer (const String& id) const
{
    for (int i = 0; i < proc.getNumberRemotePeers(); ++i) {
        String host; int port = 0;
        if (proc.getRemotePeerAddressInfo (i, host, port) && host + ":" + String (port) == id) return i;
    }
    return -1;
}

void MantraEditor::requestReceive (int index)
{
    // The other side ignores an invitation while it still lists us as a listener (which it does
    // after Mantra muted the link on connect). So: withdraw, then invite again a moment later.
    String host; int port = 0;
    if (! proc.getRemotePeerAddressInfo (index, host, port)) return;
    const String id = host + ":" + String (port);
    proc.setRemotePeerRecvActive (index, false);
    Component::SafePointer<MantraEditor> safe (this);
    Timer::callAfterDelay (400, [safe, id] {
        if (auto* ed = safe.getComponent()) {
            const int i = ed->findPeer (id);
            if (i >= 0) ed->proc.setRemotePeerRecvActive (i, true);
        }
    });
}

float MantraEditor::getPeerRecvLevel (int index) const
{
    auto* ms = const_cast<SonobusAudioProcessor&> (proc).getRemotePeerRecvMeterSource (index);
    if (ms == nullptr) return 0.0f;
    float level = 0.0f;
    for (int ch = 0; ch < ms->getNumChannels(); ++ch) level = jmax (level, ms->getRMSLevel (ch));
    return level;
}

//== recording ===============================================================
bool MantraEditor::isRecording() const
{
    return const_cast<SonobusAudioProcessor&> (proc).isRecordingToFile();
}

void MantraEditor::toggleRecording()
{
    if (proc.isRecordingToFile()) {
        proc.stopRecordingToFile();
        return;
    }
    File dir (settings->getValue ("recordFolder"));
    if (! dir.isDirectory()) {
        auto procDir = proc.getDefaultRecordingDirectory();
        dir = procDir.isLocalFile() ? procDir.getLocalFile()
                                    : File::getSpecialLocation (File::userMusicDirectory).getChildFile ("Mantra SonoBus");
    }
    dir.createDirectory();
    URL ret;
    // the mix, this machine alone, and every machine as its own (multichannel) file
    const uint32 opts = SonobusAudioProcessor::RecordMix | SonobusAudioProcessor::RecordSelf
                      | SonobusAudioProcessor::RecordIndividualUsers;
    if (proc.startRecordingToFile (URL (dir), "", ret, opts))
        lastRecording = "recording into " + dir.getFullPathName();
    else
        lastRecording = "could not record into " + dir.getFullPathName();
}
