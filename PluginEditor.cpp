#include "PluginEditor.h"

using namespace gltch;

namespace Col
{
    static juce::Colour bg (0xff0d0e11), pn (0xff181a1f), btn (0xff23262d), ink (0xffece8dc),
        dim (0xff8d8a7e), c0 (0xff2b2d34), o1 (0xffff5a2b), o2 (0xffffb21f), o3 (0xffe6c800), ac (0xffe8431f);
    static float hueShift = 0.f;
    static int curSeed = -1;

    // applique le thème de la graine (0 = thème d'origine) ; sans effet si déjà appliqué
    static void sync (int seed)
    {
        if (seed == curSeed) return;
        curSeed = seed;
        if (seed == 0)
        {
            bg = juce::Colour (0xff0d0e11); pn = juce::Colour (0xff181a1f); btn = juce::Colour (0xff23262d);
            ink = juce::Colour (0xffece8dc); dim = juce::Colour (0xff8d8a7e); c0 = juce::Colour (0xff2b2d34);
            o1 = juce::Colour (0xffff5a2b); o2 = juce::Colour (0xffffb21f); o3 = juce::Colour (0xffe6c800);
            ac = juce::Colour (0xffe8431f);
            hueShift = 0.f;
            return;
        }
        juce::Random r (seed);
        const float h0 = r.nextFloat(), h1 = std::fmod (h0 + .25f + r.nextFloat() * .5f, 1.f);
        const bool light = r.nextInt (5) == 0;
        const float sat = .12f + r.nextFloat() * .25f;
        if (light)
        {
            bg = juce::Colour::fromHSL (h0, sat, .88f, 1.f);  pn = juce::Colour::fromHSL (h0, sat, .82f, 1.f);
            btn = juce::Colour::fromHSL (h0, sat, .74f, 1.f); c0 = juce::Colour::fromHSL (h0, sat, .68f, 1.f);
            ink = juce::Colour::fromHSL (h0, .3f, .10f, 1.f); dim = juce::Colour::fromHSL (h0, .15f, .35f, 1.f);
        }
        else
        {
            bg = juce::Colour::fromHSL (h0, sat, .06f, 1.f);  pn = juce::Colour::fromHSL (h0, sat, .10f, 1.f);
            btn = juce::Colour::fromHSL (h0, sat, .15f, 1.f); c0 = juce::Colour::fromHSL (h0, sat, .19f, 1.f);
            ink = juce::Colour::fromHSL (h0, .25f, .90f, 1.f); dim = juce::Colour::fromHSL (h0, .12f, .58f, 1.f);
        }
        o1 = juce::Colour::fromHSL (h1, .85f, .55f, 1.f);
        o2 = juce::Colour::fromHSL (std::fmod (h1 + .08f, 1.f), .9f, .58f, 1.f);
        o3 = juce::Colour::fromHSL (std::fmod (h1 + .16f, 1.f), .9f, .56f, 1.f);
        ac = juce::Colour::fromHSL (h1, .8f, .50f, 1.f);
        hueShift = r.nextFloat();
    }
}

#define U8(x) juce::String::fromUTF8 (x)

static juce::Colour glitchColour (int v)
{
    if (v <= 0) return Col::c0;
    return juce::Colour::fromHSL (std::fmod ((float) ((v - 1) * 24) / 360.f + Col::hueShift, 1.f), .85f, .55f, 1.f);
}

// ============================================================ StepGrid
juce::Rectangle<int> StepGrid::cellRect (int i) const
{
    const int col = i % 16, row = i / 16;
    const float cw = ((float) getWidth() - 39.f) / 16.f;
    const float rh = ((float) getHeight() - 6.f) / 4.f;
    return { (int) ((float) col * (cw + 2.f) + (float) (col / 4) * 3.f), (int) ((float) row * (rh + 2.f)), (int) cw, (int) rh };
}

int StepGrid::cellAt (juce::Point<int> p) const
{
    for (int i = 0; i < 64; ++i)
        if (cellRect (i).expanded (1).contains (p)) return i;
    return -1;
}

void StepGrid::paint (juce::Graphics& g)
{
    Col::sync (ed.proc.themeSeed.load());
    auto& e = ed.proc.engine;
    const int o = ed.page * 64, cs = e.curStep.load();
    const bool run = e.running.load();
    for (int i = 0; i < 64; ++i)
    {
        auto r = cellRect (i).toFloat();
        juce::Colour c = Col::c0;
        juce::String txt;
        if (isGlitch)
        {
            const int v = e.GL[o + i];
            if (v) { c = glitchColour (v); txt = GLITCH_SHORT[v]; }
        }
        else
        {
            const int v = e.P[ed.trk][o + i];
            if (v == 1) c = Col::o1;
            else if (v == 2) { c = Col::o2; txt = "2"; }
            else if (v == 3) { c = Col::o3; txt = "4"; }
        }
        g.setColour (c);
        g.fillRoundedRectangle (r, 2.f);
        if (txt.isNotEmpty())
        {
            g.setColour (juce::Colours::black.withAlpha (.8f));
            g.setFont (11.f);
            g.drawText (txt, r.toNearestInt(), juce::Justification::centred);
        }
        if (run && (cs >> 6) == ed.page && (cs & 63) == i)
        {
            g.setColour (Col::ink);
            g.drawRoundedRectangle (r.reduced (1.f), 2.f, 2.f);
        }
    }
}

void StepGrid::applyCell (int c)
{
    auto& e = ed.proc.engine;
    const int idx = ed.page * 64 + c;
    if (isGlitch) e.GL[idx] = (uint8_t) paintVal;
    else e.P[ed.trk][idx] = (uint8_t) paintVal;
    lastCell = c;
    repaint();
}

void StepGrid::mouseDown (const juce::MouseEvent& m)
{
    const int c = cellAt (m.getPosition());
    if (c < 0) return;
    auto& e = ed.proc.engine;
    const int idx = ed.page * 64 + c;
    if (isGlitch) paintVal = (e.GL[idx] == ed.brush) ? 0 : ed.brush;
    else paintVal = (e.P[ed.trk][idx] + 1) % 4;
    applyCell (c);
}

void StepGrid::mouseDrag (const juce::MouseEvent& m)
{
    const int c = cellAt (m.getPosition());
    if (c >= 0 && c != lastCell) applyCell (c);
}

// ============================================================ FxPad
FxPad::FxPad (GltchEditor& e, int index)
    : ed (e), idx (index), par (e.proc.apvts.getParameter ("fx" + juce::String (index)))
{
}

bool FxPad::isOn() const { return par != nullptr && par->getValue() > .5f; }

void FxPad::setOn (bool on)
{
    if (par == nullptr) return;
    par->beginChangeGesture();
    par->setValueNotifyingHost (on ? 1.f : 0.f);
    par->endChangeGesture();
    repaint();
}

void FxPad::paint (juce::Graphics& g)
{
    Col::sync (ed.proc.themeSeed.load());
    auto r = getLocalBounds().toFloat();
    const bool on = isOn();
    g.setColour (on ? Col::o1 : Col::btn);
    g.fillRoundedRectangle (r, 5.f);
    g.setColour (Col::o1);
    g.fillRoundedRectangle (r.withHeight (4.f), 2.f);
    g.setColour (on ? juce::Colours::black : Col::ink);
    g.setFont (12.f);
    g.drawText (FX_NAMES[idx], getLocalBounds(), juce::Justification::centred);
    g.setFont (9.f);
    g.setColour (on ? juce::Colours::black : Col::dim);
    g.drawText (juce::String ((int) ed.proc.getParamValue ("fxA" + juce::String (idx))) + "%",
                getLocalBounds().reduced (4, 2), juce::Justification::topRight);
}

void FxPad::mouseDown (const juce::MouseEvent&)
{
    if (ed.latch) setOn (! isOn());
    else setOn (true);
}

void FxPad::mouseUp (const juce::MouseEvent&)
{
    if (! ed.latch) setOn (false);
}

// ============================================================ PresetGrid
juce::Rectangle<int> PresetGrid::cellRect (int i) const
{
    const int cw = getWidth() / 10, ch = getHeight() / 5;
    return juce::Rectangle<int> ((i % 10) * cw, (i / 10) * ch, cw, ch).reduced (1);
}

void PresetGrid::paint (juce::Graphics& g)
{
    Col::sync (ed.proc.themeSeed.load());
    for (int i = 0; i < GltchProcessor::numPresets; ++i)
    {
        auto r = cellRect (i);
        g.setColour (ed.proc.hasPreset (i) ? Col::o2 : Col::c0);
        g.fillRoundedRectangle (r.toFloat(), 3.f);
        g.setColour (ed.proc.hasPreset (i) ? juce::Colours::black : Col::dim);
        g.setFont (11.f);
        {
            const auto nm = ed.proc.hasPreset (i) ? ed.proc.getPresetName (i) : juce::String();
            g.setFont (nm.isNotEmpty() && ! nm.startsWith ("Preset ") ? 9.f : 11.f);
            g.drawText (nm.isNotEmpty() && ! nm.startsWith ("Preset ") ? juce::String (i + 1) + " " + nm : juce::String (i + 1),
                        r.reduced (2, 0), juce::Justification::centred, true);
        }
        if (i == ed.pcur)
        {
            g.setColour (Col::ink);
            g.drawRoundedRectangle (r.toFloat().reduced (1.f), 3.f, 2.f);
        }
    }
}

void PresetGrid::mouseDown (const juce::MouseEvent& m)
{
    for (int i = 0; i < GltchProcessor::numPresets; ++i)
        if (cellRect (i).contains (m.getPosition())) { ed.presetClick (i); return; }
}

// ============================================================ LoopBar
int LoopBar::cellAt (juce::Point<int> p) const
{
    const float cw = (float) getWidth() / 16.f;
    return juce::jlimit (0, 15, (int) ((float) p.x / cw));
}

void LoopBar::setRange (int a, int b)
{
    ed.proc.setParamValue ("loopA", (float) (juce::jmin (a, b) + 1));
    ed.proc.setParamValue ("loopB", (float) (juce::jmax (a, b) + 1));
    repaint();
}

void LoopBar::mouseDown (const juce::MouseEvent& m) { anchor = cellAt (m.getPosition()); setRange (anchor, anchor); }
void LoopBar::mouseDrag (const juce::MouseEvent& m) { setRange (anchor, cellAt (m.getPosition())); }
void LoopBar::mouseDoubleClick (const juce::MouseEvent&) { setRange (0, 15); }

void LoopBar::paint (juce::Graphics& g)
{
    Col::sync (ed.proc.themeSeed.load());
    const int a = (int) ed.proc.getParamValue ("loopA") - 1, b = (int) ed.proc.getParamValue ("loopB") - 1;
    const int lo = juce::jmin (a, b), hi = juce::jmax (a, b);
    const int cur = ed.proc.engine.running.load() ? (ed.proc.engine.curStep.load() >> 6) : -1;
    const float cw = (float) getWidth() / 16.f;
    for (int k = 0; k < 16; ++k)
    {
        auto r = juce::Rectangle<float> ((float) k * cw + 1.f, 0.f, cw - 2.f, (float) getHeight());
        const bool in = k >= lo && k <= hi;
        g.setColour (in ? Col::o2 : Col::c0);
        g.fillRoundedRectangle (r, 3.f);
        g.setColour (in ? juce::Colours::black : Col::dim);
        g.setFont (11.f);
        g.drawText (juce::String (k + 1), r.toNearestInt(), juce::Justification::centred);
        if (k == cur)
        {
            g.setColour (Col::ink);
            g.drawRoundedRectangle (r.reduced (1.f), 3.f, 2.f);
        }
    }
}

void EditorContent::paint (juce::Graphics& g) { ed.paintContent (g); }
void EditorContent::resized() { ed.layoutContent(); }

// ============================================================ Editor
static void setupSlider (juce::Slider& s, juce::Label& l, const juce::String& text, juce::Component& parent)
{
    s.setSliderStyle (juce::Slider::LinearHorizontal);
    s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 20);
    parent.addAndMakeVisible (s);
    l.setText (text, juce::dontSendNotification);
    l.setColour (juce::Label::textColourId, Col::dim);
    parent.addAndMakeVisible (l);
}

GltchEditor::GltchEditor (GltchProcessor& p) : juce::AudioProcessorEditor (&p), proc (p)
{
    laf.setColour (juce::TextButton::buttonColourId, Col::btn);
    laf.setColour (juce::TextButton::buttonOnColourId, Col::ac);
    laf.setColour (juce::TextButton::textColourOffId, Col::ink);
    laf.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    laf.setColour (juce::Slider::thumbColourId, Col::ac);
    laf.setColour (juce::Slider::trackColourId, Col::ac);
    laf.setColour (juce::Slider::backgroundColourId, Col::c0);
    laf.setColour (juce::Slider::textBoxTextColourId, Col::ink);
    laf.setColour (juce::Slider::textBoxOutlineColourId, Col::c0);
    setLookAndFeel (&laf);

    setupSlider (bpmS, bpmL, "BPM", content);
    setupSlider (densS, densL, U8 ("DENSITÉ"), content);
    setupSlider (chaosS, chaosL, "CHAOS", content);
    setupSlider (swS, swL, "SWING", content);
    bpmA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "bpm", bpmS);
    densA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "dens", densS);
    chaosA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "chaos", chaosS);
    swA = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "sw", swS);

    auto mk = [this] (juce::TextButton& b, const juce::String& t)
    {
        b.setButtonText (t);
        content.addAndMakeVisible (b);
    };

    mk (playB, "PLAY");
    playB.setClickingTogglesState (true);
    playA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "play", playB);

    mk (rgB, U8 ("ALÉA GLITCH"));
    mk (rpB, U8 ("ALÉA PATTERN"));
    mk (muB, "MUTATE");
    mk (rnB, U8 ("ALÉA NOTES"));
    mk (raB, U8 ("TOUT ALÉA"));
    mk (cxB, "EFFACER");
    mk (auB, U8 ("AUTO ALÉA / 1024"));
    auB.setClickingTogglesState (true);
    autoA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "auto", auB);
    mk (laB, "LATCH FX");
    laB.setClickingTogglesState (true);
    mk (scB, ""); mk (rtB, ""); mk (foB, "SUIVRE");

    auto& e = proc.engine;
    auto dens = [this] { return (double) proc.getParamValue ("dens"); };

    rgB.onClick = [this, &e, dens] { e.rndG (proc.uiRng, dens()); };
    rpB.onClick = [this, &e] { e.rnd2 (proc.uiRng); };
    muB.onClick = [this, &e] { e.mutate (proc.uiRng); };
    rnB.onClick = [this, &e] { e.rndN (proc.uiRng); };
    raB.onClick = [this, &e, dens]
    {
        e.rnd2 (proc.uiRng); e.rndN (proc.uiRng); e.rndG (proc.uiRng, dens());
        for (int r = 0; r < 6; ++r) setSound (r, proc.uiRng.below (soundTotal (r)));
        preview();
    };
    cxB.onClick = [&e] { e.clearAll(); };
    laB.onClick = [this] { latch = laB.getToggleState(); };
    scB.onClick = [this] { cycleParam ("sci", 6); };
    rtB.onClick = [this] { cycleParam ("root", 12); };
    foB.onClick = [this] { follow = ! follow; };

    for (int k = 0; k < 16; ++k)
    {
        mk (pageB[k], juce::String (k + 1));
        pageB[k].onClick = [this, k] { page = k; follow = false; };
        mk (palB[k], k == 0 ? juce::String ("-") : juce::String (GLITCH_NAMES[k]));
        palB[k].setColour (juce::TextButton::buttonColourId, Col::btn.interpolatedWith (glitchColour (k), k ? .3f : 0.f));
        palB[k].setColour (juce::TextButton::buttonOnColourId, glitchColour (k));
        palB[k].setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        palB[k].onClick = [this, k] { brush = k; };
    }
    for (int k = 0; k < 6; ++k)
    {
        mk (tabB[k], TRACK_NAMES[k]);
        tabB[k].setColour (juce::TextButton::buttonOnColourId, Col::o1);
        tabB[k].setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        tabB[k].onClick = [this, k] { trk = k; };
    }

    mk (sbb, "<<"); mk (sp, "<"); mk (sn, ""); mk (sx, ">"); mk (sff, ">>");
    mk (sr, U8 ("ALÉA PISTE")); mk (sa, U8 ("ALÉA SONS"));
    mk (smpModeB, ""); mk (smpLoadB, "CHARGER SAMPLES"); mk (smpDirB, "DOSSIER"); mk (smpClrB, "VIDER");
    smpModeB.setColour (juce::TextButton::buttonOnColourId, Col::o2);
    smpModeB.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    smpModeB.onClick = [this]
    {
        const auto id = "smpOn" + juce::String (trk);
        proc.setParamValue (id, proc.getParamValue (id) > .5f ? 0.f : 1.f);
    };
    smpLoadB.onClick = [this] { pickSamples (false); };
    smpDirB.onClick = [this] { pickSamples (true); };
    smpClrB.onClick = [this] { proc.clearSamples(); };
    sbb.onClick = [this] { stepSound (smpMode (trk) ? -100 : -25); };
    sp.onClick = [this] { stepSound (-1); };
    sx.onClick = [this] { stepSound (1); };
    sff.onClick = [this] { stepSound (smpMode (trk) ? 100 : 25); };
    sn.onClick = [this] { preview(); };
    sr.onClick = [this] { setSound (trk, proc.uiRng.below (soundTotal (trk))); preview(); };
    sa.onClick = [this]
    {
        for (int r = 0; r < 6; ++r) setSound (r, proc.uiRng.below (soundTotal (r)));
        preview();
    };

    content.addAndMakeVisible (glGrid);
    content.addAndMakeVisible (trGrid);
    for (int i = 0; i < FX_COUNT; ++i)
    {
        fxPad[i] = std::make_unique<FxPad> (*this, i);
        content.addAndMakeVisible (*fxPad[i]);
    }
    for (int i = 0; i < FX_COUNT; ++i)
    {
        auto& s = fxSlider[i];
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        s.setDoubleClickReturnValue (true, 100.0);
        s.setTextValueSuffix ("%");
        s.setPopupDisplayEnabled (true, true, &content);
        content.addAndMakeVisible (s);
        fxAtt[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "fxA" + juce::String (i), s);
    }
    for (int k = 0; k < 6; ++k)
    {
        muteB[k].setButtonText ("M");
        soloB[k].setButtonText ("S");
        for (auto* b : { &muteB[k], &soloB[k] }) { b->setClickingTogglesState (true); content.addAndMakeVisible (*b); }
        muteB[k].setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xffd03a3a));
        muteB[k].setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        soloB[k].setColour (juce::TextButton::buttonOnColourId, Col::o3);
        soloB[k].setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        muteA[k] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "mute" + juce::String (k), muteB[k]);
        soloA[k] = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "solo" + juce::String (k), soloB[k]);
    }

    mk (pmsB, U8 ("SAUVER (écraser)")); mk (pmdB, "SUPPR"); mk (pexB, "EXPORT .JSON"); mk (pimB, "IMPORT");
    pmsB.onClick = [this]
    {
        pmode = pmode == 1 ? 0 : 1;
        presetMsg = pmode ? U8 ("Touche une case pour sauver") : juce::String();
        pmsB.setToggleState (pmode == 1, juce::dontSendNotification);
        pmdB.setToggleState (false, juce::dontSendNotification);
    };
    pmdB.onClick = [this]
    {
        pmode = pmode == 2 ? 0 : 2;
        presetMsg = pmode ? U8 ("Touche une case pour supprimer") : juce::String();
        pmdB.setToggleState (pmode == 2, juce::dontSendNotification);
        pmsB.setToggleState (false, juce::dontSendNotification);
    };
    pexB.onClick = [this] { doExport(); };
    pimB.onClick = [this] { doImport(); };
    content.addAndMakeVisible (pgGrid);

    // ---- couleurs, choke, MIDI
    mk (themeB, U8 ("COULEURS ALÉA")); mk (themeResetB, U8 ("COULEURS D'ORIGINE")); mk (chokeB, "CHOKE HAT");
    chokeB.setClickingTogglesState (true);
    chokeA = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "choke", chokeB);
    themeB.onClick = [this]
    {
        proc.themeSeed = 1 + (int) (juce::Random::getSystemRandom().nextInt (2000000000));
        applyTheme();
    };
    themeResetB.onClick = [this] { proc.themeSeed = 0; applyTheme(); };
    midiBox.addItemList (juce::StringArray { "MIDI : Auto (canaux)", "MIDI : Pads 36-41", "MIDI : Clavier split C3",
                                             "MIDI : Clavier > BASS", "MIDI : Clavier > LEAD" }, 1);
    content.addAndMakeVisible (midiBox);
    midiA = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "midiMode", midiBox);

    // ---- boucle
    content.addAndMakeVisible (loopBar);

    // ---- volume / pan par piste
    for (int k = 0; k < 6; ++k)
    {
        for (auto* sl : { &volS[k], &panS[k] })
        {
            sl->setSliderStyle (juce::Slider::LinearHorizontal);
            sl->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            sl->setPopupDisplayEnabled (true, true, &content);
            content.addAndMakeVisible (*sl);
        }
        volS[k].setDoubleClickReturnValue (true, 100.0);
        volS[k].setTextValueSuffix ("% vol");
        panS[k].setDoubleClickReturnValue (true, 0.0);
        panS[k].setTextValueSuffix (" pan");
        volA[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "vol" + juce::String (k), volS[k]);
        panA[k] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "pan" + juce::String (k), panS[k]);
    }

    // ---- réglages du sample courant
    setupSlider (gainS, gainL, "GAIN", content);
    setupSlider (startS, startL, U8 ("DÉBUT"), content);
    gainS.setRange (0.0, 200.0, 1.0); gainS.setValue (100.0, juce::dontSendNotification);
    gainS.setDoubleClickReturnValue (true, 100.0); gainS.setTextValueSuffix ("%");
    startS.setRange (0.0, 95.0, 1.0); startS.setValue (0.0, juce::dontSendNotification);
    startS.setDoubleClickReturnValue (true, 0.0); startS.setTextValueSuffix ("%");
    gainS.onValueChange = [this] { pushSmp(); };
    startS.onValueChange = [this] { pushSmp(); };
    mk (revB, "REVERSE");
    revB.setClickingTogglesState (true);
    revB.setColour (juce::TextButton::buttonOnColourId, Col::o2);
    revB.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    revB.onClick = [this] { pushSmp(); preview(); };

    // ---- copier / coller / effacer
    scopeBox.addItemList (juce::StringArray { U8 ("Piste × page"), U8 ("Page entière (6 pistes + glitch)"), U8 ("Piste entière (16 pages)") }, 1);
    scopeBox.setSelectedId (1, juce::dontSendNotification);
    content.addAndMakeVisible (scopeBox);
    mk (copyB, "COPIER"); mk (pasteB, "COLLER"); mk (clearB, U8 ("EFFACER ZONE"));
    copyB.onClick = [this]
    {
        proc.clipCopy (scopeBox.getSelectedId() - 1, trk, page);
        presetMsg = U8 ("Copié");
        content.repaint (lblPreset);
    };
    pasteB.onClick = [this]
    {
        presetMsg = proc.clipPaste (scopeBox.getSelectedId() - 1, trk, page) ? U8 ("Collé") : U8 ("Rien à coller (copie d'abord, même zone)");
        content.repaint (lblPreset);
    };
    clearB.onClick = [this]
    {
        proc.clipClear (scopeBox.getSelectedId() - 1, trk, page);
        presetMsg = U8 ("Zone effacée");
        content.repaint (lblPreset);
    };

    // ---- presets nommés
    pnameE.setTextToShowWhenEmpty ("nom du preset", Col::dim);
    pnameE.setInputRestrictions (24);
    content.addAndMakeVisible (pnameE);
    mk (pfacB, "USINE");
    pfacB.onClick = [this]
    {
        proc.restoreFactoryPresets();
        pcur = -1;
        presetMsg = U8 ("Presets d'usine remis (cases 1-8)");
        content.repaint();
    };

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, baseW * 2, baseH * 2);
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) baseW / (double) baseH);
    int w = proc.editorW;
    if (w <= 0)
    {
        double sc = 1.0;
        if (auto* d = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
            sc = juce::jlimit (.55, 1.0, ((double) d->userArea.getHeight() - 90.0) / (double) baseH);
        w = (int) (baseW * sc);
    }
    w = juce::jlimit (baseW / 2, baseW * 2, w);
    setSize (w, juce::roundToInt ((double) w * baseH / baseW));
    applyTheme();
    startTimerHz (30);
}

void GltchEditor::applyTheme()
{
    Col::sync (proc.themeSeed.load());
    laf.setColour (juce::TextButton::buttonColourId, Col::btn);
    laf.setColour (juce::TextButton::buttonOnColourId, Col::ac);
    laf.setColour (juce::TextButton::textColourOffId, Col::ink);
    laf.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    laf.setColour (juce::Slider::thumbColourId, Col::ac);
    laf.setColour (juce::Slider::trackColourId, Col::ac);
    laf.setColour (juce::Slider::backgroundColourId, Col::c0);
    laf.setColour (juce::Slider::textBoxTextColourId, Col::ink);
    laf.setColour (juce::Slider::textBoxOutlineColourId, Col::c0);
    laf.setColour (juce::ComboBox::backgroundColourId, Col::btn);
    laf.setColour (juce::ComboBox::textColourId, Col::ink);
    laf.setColour (juce::ComboBox::outlineColourId, Col::c0);
    laf.setColour (juce::ComboBox::arrowColourId, Col::ink);
    laf.setColour (juce::PopupMenu::backgroundColourId, Col::pn);
    laf.setColour (juce::PopupMenu::textColourId, Col::ink);
    laf.setColour (juce::PopupMenu::highlightedBackgroundColourId, Col::ac);
    laf.setColour (juce::TextEditor::backgroundColourId, Col::c0);
    laf.setColour (juce::TextEditor::textColourId, Col::ink);
    laf.setColour (juce::TextEditor::outlineColourId, Col::btn);
    laf.setColour (juce::TextEditor::focusedOutlineColourId, Col::ac);
    laf.setColour (juce::Label::textColourId, Col::dim);
    for (auto* l : { &bpmL, &densL, &chaosL, &swL, &gainL, &startL }) l->setColour (juce::Label::textColourId, Col::dim);
    pnameE.applyColourToAllText (Col::ink);
    for (int k = 0; k < 16; ++k)
    {
        palB[k].setColour (juce::TextButton::buttonColourId, Col::btn.interpolatedWith (glitchColour (k), k ? .3f : 0.f));
        palB[k].setColour (juce::TextButton::buttonOnColourId, glitchColour (k));
    }
    for (int k = 0; k < 6; ++k) tabB[k].setColour (juce::TextButton::buttonOnColourId, Col::o1);
    smpModeB.setColour (juce::TextButton::buttonOnColourId, Col::o2);
    revB.setColour (juce::TextButton::buttonOnColourId, Col::o2);
    for (int k = 0; k < 6; ++k) soloB[k].setColour (juce::TextButton::buttonOnColourId, Col::o3);
    sendLookAndFeelChange();
    content.repaint();
    repaint();
}

int GltchEditor::smpIndex() const
{
    const int n = proc.getNumSamples();
    if (n <= 0) return -1;
    return (int) proc.getParamValue ("smpI" + juce::String (trk)) % n;
}

void GltchEditor::pushSmp()
{
    if (refreshing) return;
    const int i = smpIndex();
    if (i >= 0) proc.engine.setSmpSet (i, (float) gainS.getValue() * .01f, (float) startS.getValue() * .01f, revB.getToggleState());
}

void GltchEditor::refreshSmp()
{
    const int i = smpIndex();
    const int key = i < 0 ? -1 : trk * 10000 + i;
    const bool en = i >= 0;
    if (gainS.isEnabled() != en) { gainS.setEnabled (en); startS.setEnabled (en); revB.setEnabled (en); }
    if (key == lastSmpKey) return;
    lastSmpKey = key;
    float g = 1.f, st = 0.f; bool rv = false;
    if (i >= 0) proc.engine.getSmpSet (i, g, st, rv);
    refreshing = true;
    gainS.setValue (g * 100.0, juce::dontSendNotification);
    startS.setValue (st * 100.0, juce::dontSendNotification);
    revB.setToggleState (rv, juce::dontSendNotification);
    refreshing = false;
}

GltchEditor::~GltchEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

bool GltchEditor::smpMode (int r) const
{
    return proc.getParamValue ("smpOn" + juce::String (r)) > .5f && proc.getNumSamples() > 0;
}

int GltchEditor::soundTotal (int r) const { return smpMode (r) ? proc.getNumSamples() : soundCount (r); }

int GltchEditor::soundIdx (int r) const
{
    if (smpMode (r)) return (int) proc.getParamValue ("smpI" + juce::String (r)) % juce::jmax (1, proc.getNumSamples());
    return (int) proc.getParamValue ("snd" + juce::String (r));
}

void GltchEditor::setSound (int r, int idx)
{
    proc.setParamValue ((smpMode (r) ? "smpI" : "snd") + juce::String (r), (float) idx);
}

void GltchEditor::stepSound (int d)
{
    const int n = soundTotal (trk);
    setSound (trk, ((soundIdx (trk) + d) % n + n) % n);
    preview();
}

void GltchEditor::pickSamples (bool folder)
{
    chooser = std::make_unique<juce::FileChooser> (folder ? U8 ("Choisir un dossier de samples") : U8 ("Choisir des samples (WAV, MP3...)"),
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg");
    const int flags = folder ? (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories)
                             : (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                | juce::FileBrowserComponent::canSelectMultipleItems);
    juce::Component::SafePointer<GltchEditor> safe (this);
    chooser->launchAsync (flags, [safe] (const juce::FileChooser& fc)
                          {
                              if (safe == nullptr) return;
                              auto res = fc.getResults();
                              if (res.isEmpty()) return;
                              safe->proc.loadSampleFiles (res, false);
                          });
}

bool GltchEditor::isInterestedInFileDrag (const juce::StringArray& files) { return files.size() > 0; }

void GltchEditor::filesDropped (const juce::StringArray& files, int, int)
{
    juce::Array<juce::File> fs;
    for (auto& f : files) fs.add (juce::File (f));
    proc.loadSampleFiles (fs, false);
}

void GltchEditor::preview() { proc.engine.previewMask.fetch_or (1 << trk); }

void GltchEditor::cycleParam (const juce::String& id, int count)
{
    proc.setParamValue (id, (float) (((int) proc.getParamValue (id) + 1) % count));
}

void GltchEditor::presetClick (int i)
{
    const int m = pmode;
    pmode = 0;
    pmsB.setToggleState (false, juce::dontSendNotification);
    pmdB.setToggleState (false, juce::dontSendNotification);
    const auto n = juce::String (i + 1);
    if (m == 2)
    {
        if (proc.hasPreset (i)) { proc.clearPreset (i); if (pcur == i) pcur = -1; presetMsg = "Preset " + n + U8 (" supprimé"); }
        else presetMsg = "Slot " + n + U8 (" déjà vide");
    }
    else if (m == 1 || ! proc.hasPreset (i))
    {
        proc.storePreset (i, pnameE.getText());
        pcur = i;
        pnameE.setText (proc.getPresetName (i), false);
        presetMsg = "Preset " + n + U8 (" sauvé");
    }
    else if (proc.applyPreset (i))
    {
        pcur = i;
        pnameE.setText (proc.getPresetName (i), false);
        presetMsg = "Preset " + n + " " + proc.getPresetName (i) + U8 (" chargé");
    }
    else presetMsg = "Preset illisible";
    content.repaint();
}

void GltchEditor::doExport()
{
    chooser = std::make_unique<juce::FileChooser> ("Exporter les presets",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("gltch1024-presets.json"), "*.json");
    juce::Component::SafePointer<GltchEditor> sp2 (this);
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [sp2] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (sp2 == nullptr || f == juce::File()) return;
                              sp2->presetMsg = f.replaceWithText (sp2->proc.presetsAsJson()) ? U8 ("Export terminé") : U8 ("Export refusé");
                              sp2->repaint();
                          });
}

void GltchEditor::doImport()
{
    chooser = std::make_unique<juce::FileChooser> ("Importer des presets",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.json");
    juce::Component::SafePointer<GltchEditor> sp2 (this);
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [sp2] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (sp2 == nullptr || f == juce::File()) return;
                              const int n = sp2->proc.importPresetsJson (f.loadFileAsString());
                              sp2->pcur = -1;
                              sp2->presetMsg = n < 0 ? U8 ("Fichier invalide") : juce::String (n) + U8 (" preset(s) importé(s)");
                              sp2->repaint();
                          });
}

void GltchEditor::timerCallback()
{
    auto& e = proc.engine;
    const bool run = e.running.load();
    const int cs = e.curStep.load();
    if (follow && run && (cs >> 6) != page) page = cs >> 6;

    for (int k = 0; k < 16; ++k)
    {
        pageB[k].setToggleState (k == page, juce::dontSendNotification);
        palB[k].setToggleState (k == brush, juce::dontSendNotification);
    }
    for (int k = 0; k < 6; ++k) tabB[k].setToggleState (k == trk, juce::dontSendNotification);
    foB.setToggleState (follow, juce::dontSendNotification);

    {
        const bool sm = smpMode (trk);
        const int si = soundIdx (trk), tot = soundTotal (trk);
        juce::String nm = sm ? proc.getSampleName (si)
                             : juce::String (sounds (trk)[(size_t) juce::jlimit (0, soundCount (trk) - 1, si)].name);
        if (nm.length() > 26) nm = nm.substring (0, 25) + "~";
        sn.setButtonText (juce::String (TRACK_NAMES[trk]) + (sm ? " [SMP] " : " - ") + nm
                          + " " + juce::String (si + 1) + "/" + juce::String (tot));

        const bool on = proc.getParamValue ("smpOn" + juce::String (trk)) > .5f;
        smpModeB.setButtonText (on ? (proc.getNumSamples() > 0 ? "SAMPLE : ON" : "SAMPLE : ON (vide)") : "SAMPLE : OFF");
        smpModeB.setToggleState (on, juce::dontSendNotification);

        juce::String m = U8 ("BANQUE : ") + juce::String (proc.getNumSamples()) + "/" + juce::String (gltch::MAX_SAMPLES);
        if (proc.isLoading())
            m += U8 (" · chargement ") + juce::String (proc.getLoadDone()) + "/" + juce::String (proc.getLoadTotal()) + U8 ("…");
        else if (proc.getLoadMessage().isNotEmpty())
            m += U8 (" · ") + proc.getLoadMessage();
        if (m != smpMsg) { smpMsg = m; content.repaint (lblSmp); }
    }
    refreshSmp();
    loopBar.repaint();
    scB.setButtonText ("GAMME " + juce::String (SCALES[juce::jlimit (0, 5, (int) proc.getParamValue ("sci"))].name));
    rtB.setButtonText ("TON " + juce::String (NOTE_NAMES[juce::jlimit (0, 11, (int) proc.getParamValue ("root"))]));
    playB.setButtonText (playB.getToggleState() ? "STOP" : "PLAY");

    glGrid.repaint();
    trGrid.repaint();
    pgGrid.repaint();
    for (auto& f : fxPad) if (f) f->repaint();
    content.repaint (lblPreset);
}

void GltchEditor::paint (juce::Graphics& g)
{
    Col::sync (proc.themeSeed.load());
    g.fillAll (Col::bg);
}

void GltchEditor::paintContent (juce::Graphics& g)
{
    Col::sync (proc.themeSeed.load());
    g.fillAll (Col::bg);
    g.setColour (Col::ink);
    g.setFont (juce::Font (juce::FontOptions (26.f, juce::Font::bold | juce::Font::italic)));
    const juce::Font tf (juce::FontOptions (26.f, juce::Font::bold | juce::Font::italic));
    g.setFont (tf);
    const int tw = juce::GlyphArrangement::getStringWidthInt (tf, "YouGlitch ");
    g.drawText ("YouGlitch", 10, 8, tw + 4, 34, juce::Justification::centredLeft);
    g.setColour (Col::ac);
    g.drawText ("SMP 1024", 10 + tw, 8, 160, 34, juce::Justification::centredLeft);

    g.setColour (Col::dim);
    g.setFont (11.f);
    g.drawText (U8 ("GLITCH LANE · choisis un effet, puis tape les pas"), lblGlitch, juce::Justification::centredLeft);
    g.drawText (U8 ("PISTE · pas : 1 coup · 2 roulade ×2 · 3 roulade ×4"), lblHint, juce::Justification::centredLeft);
    g.drawText (U8 ("FX PERF · maintiens (ou LATCH) · barre = intensité 0-100 % (double-clic = 100 %)"), lblFx, juce::Justification::centredLeft);
    g.drawText (U8 ("PRESETS · 50 emplacements · case vide = sauve, case pleine = charge"), lblPreset.withTrimmedRight (200), juce::Justification::centredLeft);
    g.setColour (Col::o2);
    g.drawText (presetMsg, lblPreset.withTrimmedLeft (juce::jmax (0, lblPreset.getWidth() - 200)), juce::Justification::centredRight);
    g.drawText (smpMsg, lblSmp, juce::Justification::centredLeft);
    g.setColour (Col::dim);
    g.setFont (11.f);
    g.drawText (U8 ("BOUCLE · pages jouées (clic = 1 page, glisser = plage, double-clic = 1-16)"), lblLoop, juce::Justification::centredLeft);
    g.drawText (U8 ("VOLUME (haut) · PAN (bas) par piste · double-clic = remise à zéro"), lblVolPan, juce::Justification::centredLeft);
}

void GltchEditor::resized()
{
    const float sc = (float) getWidth() / (float) baseW;
    content.setTransform (juce::AffineTransform::scale (sc));
    content.setBounds (0, 0, baseW, baseH);
    proc.editorW = getWidth();
    proc.editorH = getHeight();
}

void GltchEditor::layoutContent()
{
    auto b = content.getLocalBounds().reduced (10, 8);
    const int W = b.getWidth(), x0 = b.getX();
    int y = b.getY();
    auto row = [&] (int h) { juce::Rectangle<int> r (x0, y, W, h); y += h + 4; return r; };
    auto distribute = [] (juce::Rectangle<int> r, std::initializer_list<juce::Component*> cs, int gap)
    {
        const int n = (int) cs.size();
        const int w = (r.getWidth() - gap * (n - 1)) / n;
        int i = 0;
        for (auto* c : cs) { c->setBounds (r.getX() + i * (w + gap), r.getY(), w, r.getHeight()); ++i; }
    };

    {   // en-tête
        auto r = row (34);
        r.removeFromLeft (270);
        playB.setBounds (r.removeFromRight (90));
        r.removeFromRight (8);
        bpmL.setBounds (r.removeFromLeft (36));
        bpmS.setBounds (r);
    }
    distribute (row (30), { &rgB, &rpB, &muB, &rnB, &raB, &cxB }, 5);
    {
        auto r = row (26);
        const int w = (r.getWidth() - 20) / 3;
        auto a = r.removeFromLeft (w); r.removeFromLeft (10);
        auto c = r.removeFromLeft (w); r.removeFromLeft (10);
        densL.setBounds (a.removeFromLeft (62)); densS.setBounds (a);
        chaosL.setBounds (c.removeFromLeft (50)); chaosS.setBounds (c);
        swL.setBounds (r.removeFromLeft (50)); swS.setBounds (r);
    }
    distribute (row (30), { &auB, &laB, &scB, &rtB }, 5);
    {
        auto r = row (28);
        midiBox.setBounds (r.removeFromRight (230));
        r.removeFromRight (5);
        distribute (r, { &themeB, &themeResetB, &chokeB }, 5);
    }
    {
        auto r = row (28);
        foB.setBounds (r.removeFromRight (74));
        r.removeFromRight (4);
        const int w = (r.getWidth() - 2 * 15) / 16;
        for (int k = 0; k < 16; ++k) pageB[k].setBounds (r.getX() + k * (w + 2), r.getY(), w, r.getHeight());
    }

    lblLoop = row (14);
    loopBar.setBounds (row (22));
    lblGlitch = row (16);
    distribute (row (24), { &palB[0], &palB[1], &palB[2], &palB[3], &palB[4], &palB[5], &palB[6], &palB[7] }, 3);
    distribute (row (24), { &palB[8], &palB[9], &palB[10], &palB[11], &palB[12], &palB[13], &palB[14], &palB[15] }, 3);
    glGrid.setBounds (row (80));

    lblTrack = row (0);
    y += 2;
    {
        auto r = row (30);
        const int gap = 6, m = 24;
        const int gw = (r.getWidth() - 5 * gap) / 6;
        for (int k = 0; k < 6; ++k)
        {
            const int x = r.getX() + k * (gw + gap);
            tabB[k].setBounds (x, r.getY(), gw - 2 * m - 4, r.getHeight());
            muteB[k].setBounds (x + gw - 2 * m - 2, r.getY(), m, r.getHeight());
            soloB[k].setBounds (x + gw - m, r.getY(), m, r.getHeight());
        }
    }
    lblVolPan = row (14);
    for (int pass = 0; pass < 2; ++pass)
    {
        auto r = row (16);
        const int gap = 6, gw = (r.getWidth() - 5 * gap) / 6;
        for (int k = 0; k < 6; ++k)
            (pass == 0 ? volS[k] : panS[k]).setBounds (r.getX() + k * (gw + gap), r.getY(), gw, r.getHeight());
    }
    {
        auto r = row (28);
        sbb.setBounds (r.removeFromLeft (50)); sp.setBounds (r.removeFromLeft (50));
        sff.setBounds (r.removeFromRight (50)); sx.setBounds (r.removeFromRight (50));
        sn.setBounds (r.reduced (3, 0));
    }
    distribute (row (28), { &sr, &sa }, 5);
    distribute (row (28), { &smpModeB, &smpLoadB, &smpDirB, &smpClrB }, 5);
    {
        auto r = row (28);
        revB.setBounds (r.removeFromRight (90));
        r.removeFromRight (6);
        const int w = (r.getWidth() - 10) / 2;
        auto a = r.removeFromLeft (w); r.removeFromLeft (10);
        gainL.setBounds (a.removeFromLeft (44)); gainS.setBounds (a);
        startL.setBounds (r.removeFromLeft (52)); startS.setBounds (r);
    }
    {
        auto r = row (28);
        scopeBox.setBounds (r.removeFromLeft (260));
        r.removeFromLeft (5);
        distribute (r, { &copyB, &pasteB, &clearB }, 5);
    }
    lblSmp = row (16);
    lblHint = row (14);
    trGrid.setBounds (row (80));

    lblFx = row (16);
    for (int k = 0; k < 3; ++k)
    {
        auto r = row (30);
        auto r2 = row (14);
        const int w = (r.getWidth() - 4 * 4) / 5;
        for (int c = 0; c < 5; ++c)
        {
            fxPad[k * 5 + c]->setBounds (r.getX() + c * (w + 4), r.getY(), w, r.getHeight());
            fxSlider[k * 5 + c].setBounds (r2.getX() + c * (w + 4), r2.getY(), w, r2.getHeight());
        }
    }

    lblPreset = row (16);
    {
        auto r = row (28);
        pnameE.setBounds (r.removeFromLeft (190));
        r.removeFromLeft (5);
        distribute (r, { &pmsB, &pmdB, &pfacB, &pexB, &pimB }, 5);
    }
    pgGrid.setBounds (row (5 * 22));
}
