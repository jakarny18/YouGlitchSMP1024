#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GltchEditor;

// Conteneur de taille fixe (base 820 x 1155) mis à l'échelle par l'éditeur redimensionnable
class EditorContent : public juce::Component
{
public:
    explicit EditorContent (GltchEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GltchEditor& ed;
};

// Sélecteur de plage de pages de la boucle : clic = une page, glisser = plage, double-clic = 1-16
class LoopBar : public juce::Component
{
public:
    explicit LoopBar (GltchEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    int cellAt (juce::Point<int> p) const;
    void setRange (int a, int b);
    GltchEditor& ed;
    int anchor = 0;
};

// Grille de 64 pas (16 colonnes x 4 lignes) : voie glitch ou piste
class StepGrid : public juce::Component
{
public:
    StepGrid (GltchEditor& e, bool glitchLane) : ed (e), isGlitch (glitchLane) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> cellRect (int i) const;
    int cellAt (juce::Point<int> p) const;
    void applyCell (int c);

    GltchEditor& ed;
    bool isGlitch;
    int paintVal = 0, lastCell = -1;
};

// Pad FX : maintenu (ou LATCH) -> paramètre automatisable fxN
class FxPad : public juce::Component
{
public:
    FxPad (GltchEditor& e, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    bool isOn() const;
    void setOn (bool on);
    GltchEditor& ed;
    int idx;
    juce::RangedAudioParameter* par;
};

class PresetGrid : public juce::Component
{
public:
    explicit PresetGrid (GltchEditor& e) : ed (e) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<int> cellRect (int i) const;
    GltchEditor& ed;
};

class GltchEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit GltchEditor (GltchProcessor& p);
    ~GltchEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void paintContent (juce::Graphics&);
    void layoutContent();

    static constexpr int baseW = 820, baseH = 1155;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    GltchProcessor& proc;
    int page = 0, brush = 1, trk = 0, pcur = -1;
    bool follow = true, latch = false;

    void presetClick (int i);

private:
    void timerCallback() override;
    int soundIdx (int r) const;
    bool smpMode (int r) const;
    int soundTotal (int r) const;
    void pickSamples (bool folder);
    void setSound (int r, int idx);
    void stepSound (int d);
    void preview();
    void cycleParam (const juce::String& id, int count);
    void doExport();
    void doImport();
    void applyTheme();
    void pushSmp();
    void refreshSmp();
    int smpIndex() const;

    juce::LookAndFeel_V4 laf;
    EditorContent content { *this };
    LoopBar loopBar { *this };
    juce::TextButton themeB, themeResetB, chokeB, copyB, pasteB, clearB, revB, pfacB;
    juce::ComboBox midiBox, scopeBox;
    juce::Slider volS[6], panS[6], gainS, startS;
    juce::Label gainL, startL;
    juce::TextEditor pnameE;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> volA[6], panA[6];
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> chokeA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> midiA;
    int lastSmpKey = -1;
    bool refreshing = false;
    juce::Rectangle<int> lblLoop, lblVolPan;

    juce::Slider bpmS, densS, chaosS, swS;
    juce::Label bpmL, densL, chaosL, swL;
    juce::TextButton playB, rgB, rpB, muB, rnB, raB, cxB, auB, laB, scB, rtB, foB;
    juce::TextButton sbb, sp, sn, sx, sff, sr, sa, pmsB, pmdB, pexB, pimB;
    juce::TextButton smpModeB, smpLoadB, smpDirB, smpClrB;
    juce::TextButton muteB[6], soloB[6];
    juce::Slider fxSlider[gltch::FX_COUNT];
    juce::TextButton pageB[16], palB[16], tabB[6];

    StepGrid glGrid { *this, true };
    StepGrid trGrid { *this, false };
    PresetGrid pgGrid { *this };
    std::unique_ptr<FxPad> fxPad[gltch::FX_COUNT];

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> bpmA, densA, chaosA, swA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playA, autoA;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteA[6], soloA[6];
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> fxAtt[gltch::FX_COUNT];

    juce::Rectangle<int> lblGlitch, lblTrack, lblHint, lblFx, lblPreset, lblSmp;
    juce::String presetMsg, smpMsg;
    int pmode = 0;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GltchEditor)
};
