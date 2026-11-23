#pragma once
#include <JuceHeader.h>
#include <array>
#include "Engine.h"

class GltchProcessor : public juce::AudioProcessor
{
public:
    GltchProcessor();
    ~GltchProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "YouGlitch SMP 1024"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- accès pour l'éditeur
    void setParamValue (const juce::String& id, float realValue);
    float getParamValue (const juce::String& id) const;

    // presets (50 emplacements, persistants dans %APPDATA%/GLTCH-1024/presets.json)
    static constexpr int numPresets = 50;
    bool hasPreset (int i) const { return presets[(size_t) i].isObject(); }
    juce::String getPresetName (int i) const;
    void storePreset (int i, const juce::String& name = {});
    void restoreFactoryPresets();  // remet les presets d'usine dans les emplacements 1 a 8
    static constexpr int numFactory = 8;
    void clearPreset (int i);
    bool applyPreset (int i);
    juce::String presetsAsJson() const;
    int importPresetsJson (const juce::String& text); // -1 si invalide, sinon nombre de presets
    void savePresetFile();

    // ---- banque de samples (jusqu'a 500, WAV / MP3 / AIFF / FLAC / OGG), chargee en tache de fond
    void loadSampleFiles (const juce::Array<juce::File>& files, bool replace, bool keepOrder = false);
    void clearSamples();
    int getNumSamples() const { return current ? (int) current->s.size() : 0; }
    juce::String getSampleName (int i) const;
    bool isLoading() const { return loading.load(); }
    int getLoadDone() const { return loadDone.load(); }
    int getLoadTotal() const { return loadTotal.load(); }
    juce::String getLoadMessage() const { return loadMsg; }

    // ---- copier / coller / effacer (0 = piste x page, 1 = page entiere (6 pistes + glitch), 2 = piste entiere)
    void clipCopy (int scope, int track, int page);
    bool clipPaste (int scope, int track, int page);
    void clipClear (int scope, int track, int page);
    bool clipHas (int scope) const { return clip.valid && clip.scope == scope; }

    // ---- reglages persistants de l'interface
    std::atomic<int> themeSeed { 0 };   // 0 = theme d'origine
    int editorW = 0, editorH = 0;       // taille de fenetre memorisee (0 = defaut)

    juce::AudioProcessorValueTreeState apvts;
    gltch::Engine engine;
    gltch::Rng uiRng;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    juce::var encodePreset() const;
    bool applyPresetVar (const juce::var& v);
    static bool presetOk (const juce::var& o);
    juce::File presetFile() const;
    void loadPresetFile();
    static std::string encP (const gltch::Engine& e);
    static std::string encN (const gltch::Engine& e);
    static std::string encG (const gltch::Engine& e);
    juce::var makeFactory (int k) const;
    struct Clip { bool valid = false; int scope = 0; std::vector<uint8_t> p, n, g; } clip;
    std::atomic<float>* pVol[6] {};
    std::atomic<float>* pPan[6] {};
    std::atomic<float>* pLoopA = nullptr;
    std::atomic<float>* pLoopB = nullptr;
    std::atomic<float>* pChoke = nullptr;
    std::atomic<float>* pMidiMode = nullptr;
    void decodePatterns (const juce::String& p, const juce::String& n, const juce::String& g);

    std::atomic<float>* pBpm = nullptr;
    std::atomic<float>* pDens = nullptr;
    std::atomic<float>* pChaos = nullptr;
    std::atomic<float>* pSw = nullptr;
    std::atomic<float>* pRoot = nullptr;
    std::atomic<float>* pSci = nullptr;
    std::atomic<float>* pAuto = nullptr;
    std::atomic<float>* pPlay = nullptr;
    std::atomic<float>* pFx[gltch::FX_COUNT] {};
    std::atomic<float>* pSnd[6] {};

    std::array<juce::var, numPresets> presets;

    struct Loader;
    std::unique_ptr<Loader> loader;
    juce::AudioFormatManager formatManager;
    std::vector<std::shared_ptr<gltch::SampleBank>> banks;
    std::shared_ptr<gltch::SampleBank> current;
    std::atomic<int> nextBankId { 0 }, loadGen { 0 }, loadDone { 0 }, loadTotal { 0 };
    std::atomic<bool> loading { false };
    juce::String loadMsg;
    void installBank (std::shared_ptr<gltch::SampleBank> nb, int gen, const juce::String& msg);
    void cleanBanks();
    std::atomic<float>* pSmpOn[6] {};
    std::atomic<float>* pSmpI[6] {};
    std::atomic<float>* pMute[6] {};
    std::atomic<float>* pSolo[6] {};
    std::atomic<float>* pFxA[gltch::FX_COUNT] {};

    JUCE_DECLARE_WEAK_REFERENCEABLE (GltchProcessor)
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GltchProcessor)
};
