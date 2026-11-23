#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <set>

#define U8(x) juce::String::fromUTF8 (x)

using namespace gltch;

static juce::StringArray noteNames()
{
    juce::StringArray a;
    for (auto* n : NOTE_NAMES) a.add (n);
    return a;
}

static juce::StringArray scaleNames()
{
    juce::StringArray a;
    for (auto& s : SCALES) a.add (s.name);
    return a;
}

juce::AudioProcessorValueTreeState::ParameterLayout GltchProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    using PID = juce::ParameterID;

    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "bpm", 1 }, "BPM (interne)",
                                                              juce::NormalisableRange<float> (80.f, 220.f, 1.f), 150.f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "dens", 1 }, "Densite",
                                                              juce::NormalisableRange<float> (0.f, 1.f, 0.01f), .35f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "chaos", 1 }, "Chaos",
                                                              juce::NormalisableRange<float> (0.f, 1.f, 0.01f), .15f));
    p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "sw", 1 }, "Swing",
                                                              juce::NormalisableRange<float> (0.f, 1.f, 0.01f), 0.f));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (PID { "root", 1 }, "Ton", noteNames(), 0));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (PID { "sci", 1 }, "Gamme", scaleNames(), 0));
    p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "auto", 1 }, "Auto alea / 1024", false));
    p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "play", 1 }, "Lecture interne", false));

    for (int i = 0; i < 6; ++i)
        p.push_back (std::make_unique<juce::AudioParameterInt> (PID { "snd" + juce::String (i), 1 },
                                                                juce::String (TRACK_NAMES[i]) + " son",
                                                                0, soundCount (i) - 1, 0));
    for (int i = 0; i < 6; ++i)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "smpOn" + juce::String (i), 1 },
                                                                 juce::String (TRACK_NAMES[i]) + " mode sample", false));
        p.push_back (std::make_unique<juce::AudioParameterInt> (PID { "smpI" + juce::String (i), 1 },
                                                                juce::String (TRACK_NAMES[i]) + " sample", 0, MAX_SAMPLES - 1, 0));
    }
    for (int i = 0; i < 6; ++i)
    {
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "mute" + juce::String (i), 1 },
                                                                 juce::String (TRACK_NAMES[i]) + " mute", false));
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "solo" + juce::String (i), 1 },
                                                                 juce::String (TRACK_NAMES[i]) + " solo", false));
    }
    for (int i = 0; i < 6; ++i)
    {
        p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "vol" + juce::String (i), 1 },
                                                                  juce::String (TRACK_NAMES[i]) + " volume",
                                                                  juce::NormalisableRange<float> (0.f, 200.f, 1.f), 100.f));
        p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "pan" + juce::String (i), 1 },
                                                                  juce::String (TRACK_NAMES[i]) + " pan",
                                                                  juce::NormalisableRange<float> (-100.f, 100.f, 1.f), 0.f));
    }
    p.push_back (std::make_unique<juce::AudioParameterInt> (PID { "loopA", 1 }, "Boucle page debut", 1, 16, 1));
    p.push_back (std::make_unique<juce::AudioParameterInt> (PID { "loopB", 1 }, "Boucle page fin", 1, 16, 16));
    p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "choke", 1 }, "Choke charleston", false));
    p.push_back (std::make_unique<juce::AudioParameterChoice> (PID { "midiMode", 1 }, "Mode MIDI",
                    juce::StringArray { "Auto (canaux)", "Pads 36-41", "Clavier split C3", "Clavier > BASS", "Clavier > LEAD" }, 0));
    for (int i = 0; i < FX_COUNT; ++i)
        p.push_back (std::make_unique<juce::AudioParameterFloat> (PID { "fxA" + juce::String (i), 1 },
                                                                  juce::String ("FX ") + FX_NAMES[i] + " intensite",
                                                                  juce::NormalisableRange<float> (0.f, 100.f, 1.f), 100.f));
    for (int i = 0; i < FX_COUNT; ++i)
        p.push_back (std::make_unique<juce::AudioParameterBool> (PID { "fx" + juce::String (i), 1 },
                                                                 juce::String ("FX ") + FX_NAMES[i], false));
    return { p.begin(), p.end() };
}


// ============================================================ chargeur de samples (thread de fond)
struct GltchProcessor::Loader : public juce::Thread
{
    Loader (GltchProcessor& o, std::vector<juce::File> f, std::vector<Sample> k, int g, bool ordered)
        : juce::Thread ("GLTCH samples"), owner (o), files (std::move (f)), keep (std::move (k)), gen (g), keepOrder (ordered) {}

    void run() override
    {
        std::vector<juce::File> list;
        for (auto& f : files)
        {
            if (threadShouldExit()) return;
            if (f.isDirectory())
            {
                juce::Array<juce::File> found;
                f.findChildFiles (found, juce::File::findFiles, true);
                for (auto& x : found) list.push_back (x);
            }
            else list.push_back (f);
        }

        if (! keepOrder)
        {
            list.erase (std::remove_if (list.begin(), list.end(), [] (const juce::File& f)
                        {
                            static const char* exts[] = { ".wav", ".mp3", ".aif", ".aiff", ".aifc", ".flac", ".ogg" };
                            const auto e = f.getFileExtension().toLowerCase();
                            for (auto* x : exts) if (e == x) return false;
                            return true;
                        }), list.end());
            std::sort (list.begin(), list.end(), [] (const juce::File& a, const juce::File& b)
                       { return a.getFullPathName().compareNatural (b.getFullPathName()) < 0; });
        }

        std::set<std::string> known;
        size_t bytes = 0;
        for (auto& s : keep)
        {
            known.insert (s.path);
            if (s.data) bytes += s.data->size() * sizeof (int16_t);
            if (s.dataR) bytes += s.dataR->size() * sizeof (int16_t);
        }
        const size_t budget = (size_t) 1536 * 1024 * 1024;
        const int before = (int) keep.size();
        std::vector<Sample> out = std::move (keep);

        owner.loadDone = 0;
        owner.loadTotal = (int) list.size();
        int skipped = 0;
        bool full = false, mem = false;

        for (auto& f : list)
        {
            if (threadShouldExit()) return;
            const std::string path = f.getFullPathName().toStdString();
            if (! keepOrder && known.count (path) > 0) { ++owner.loadDone; continue; }
            if ((int) out.size() >= MAX_SAMPLES) { full = true; break; }
            if (bytes >= budget) { mem = true; break; }

            std::unique_ptr<juce::AudioFormatReader> rd (owner.formatManager.createReaderFor (f));
            std::shared_ptr<std::vector<float>> chL, chR;
            double sr = 44100.0;

            if (rd != nullptr && rd->lengthInSamples >= 4 && rd->sampleRate >= 1000.0 && rd->numChannels >= 1)
            {
                const int n = (int) juce::jmin<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 10.0));
                juce::AudioBuffer<float> buf (2, n);
                buf.clear();
                if (rd->read (&buf, 0, n, 0, true, true))
                {
                    const bool stereo = rd->numChannels > 1;
                    const float* l = buf.getReadPointer (0);
                    chL = std::make_shared<std::vector<float>> (l, l + n);
                    float pk = 0.f;
                    for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (l[i]));
                    if (stereo)
                    {
                        const float* r = buf.getReadPointer (1);
                        chR = std::make_shared<std::vector<float>> (r, r + n);
                        for (int i = 0; i < n; ++i) pk = juce::jmax (pk, std::abs (r[i]));
                    }
                    if (pk > 1e-5f)
                    {
                        const float g = .9f / pk;
                        for (auto& v : *chL) v *= g;
                        if (chR != nullptr) for (auto& v : *chR) v *= g;
                    }
                    sr = rd->sampleRate;
                }
            }

            Sample s;
            s.path = path;
            if (chL != nullptr)
            {
                s.name = f.getFileNameWithoutExtension().toStdString();
                s.sr = sr;
                auto conv = [] (const std::vector<float>& src)
                {
                    auto q = std::make_shared<std::vector<int16_t>> (src.size());
                    for (size_t i = 0; i < src.size(); ++i)
                        (*q)[i] = (int16_t) juce::roundToInt (juce::jlimit (-1.f, 1.f, src[i]) * 32767.f);
                    return q;
                };
                s.data = conv (*chL);
                bytes += s.data->size() * sizeof (int16_t);
                if (chR != nullptr)
                {
                    s.dataR = conv (*chR);
                    bytes += s.dataR->size() * sizeof (int16_t);
                }
            }
            else
            {
                ++skipped;
                if (! keepOrder) { ++owner.loadDone; continue; }  // illisible : ignore
                // restauration d'un projet : garde la place (silence) pour que les index restent valides
                s.name = "(introuvable) " + f.getFileNameWithoutExtension().toStdString();
                s.data = std::make_shared<std::vector<int16_t>> (64, (int16_t) 0);
            }
            out.push_back (std::move (s));
            ++owner.loadDone;
        }

        if (threadShouldExit()) return;

        juce::String msg;
        const int added = (int) out.size() - before;
        if (added <= 0 && ! keepOrder)
            msg = skipped > 0 ? juce::String (skipped) + U8 (" fichier(s) illisible(s)") : U8 ("Aucun nouveau sample");
        else
            msg = juce::String ((int) out.size()) + U8 (" samples") + (skipped > 0 ? U8 (" · ") + juce::String (skipped) + (keepOrder ? U8 (" introuvable(s)") : U8 (" illisible(s)")) : juce::String());
        if (full) msg += U8 (" · limite de ") + juce::String (MAX_SAMPLES) + U8 (" atteinte");
        if (mem) msg += U8 (" · mémoire max atteinte");

        std::shared_ptr<SampleBank> nb;
        if (added > 0 || keepOrder)
        {
            nb = std::make_shared<SampleBank>();
            nb->id = ++owner.nextBankId;
            nb->s = std::move (out);
        }
        juce::WeakReference<GltchProcessor> weak (&owner);
        const int g = gen;
        juce::MessageManager::callAsync ([weak, nb, g, msg]
                                         { if (auto* p = weak.get()) p->installBank (nb, g, msg); });
    }

    GltchProcessor& owner;
    std::vector<juce::File> files;
    std::vector<Sample> keep;
    int gen;
    bool keepOrder;
};

GltchProcessor::GltchProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("KICK", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("SNARE", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("HAT", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("CLAP", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("BASS", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("LEAD", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "GLTCH", createLayout()),
      uiRng ((uint64_t) juce::Time::currentTimeMillis() * 2654435761ull + 99)
{
    pBpm = apvts.getRawParameterValue ("bpm");
    pDens = apvts.getRawParameterValue ("dens");
    pChaos = apvts.getRawParameterValue ("chaos");
    pSw = apvts.getRawParameterValue ("sw");
    pRoot = apvts.getRawParameterValue ("root");
    pSci = apvts.getRawParameterValue ("sci");
    pAuto = apvts.getRawParameterValue ("auto");
    pPlay = apvts.getRawParameterValue ("play");
    for (int i = 0; i < FX_COUNT; ++i) pFx[i] = apvts.getRawParameterValue ("fx" + juce::String (i));
    for (int i = 0; i < 6; ++i) pSnd[i] = apvts.getRawParameterValue ("snd" + juce::String (i));
    for (int i = 0; i < 6; ++i)
    {
        pSmpOn[i] = apvts.getRawParameterValue ("smpOn" + juce::String (i));
        pSmpI[i] = apvts.getRawParameterValue ("smpI" + juce::String (i));
    }
    for (int i = 0; i < 6; ++i)
    {
        pMute[i] = apvts.getRawParameterValue ("mute" + juce::String (i));
        pSolo[i] = apvts.getRawParameterValue ("solo" + juce::String (i));
    }
    for (int i = 0; i < FX_COUNT; ++i) pFxA[i] = apvts.getRawParameterValue ("fxA" + juce::String (i));
    for (int i = 0; i < 6; ++i)
    {
        pVol[i] = apvts.getRawParameterValue ("vol" + juce::String (i));
        pPan[i] = apvts.getRawParameterValue ("pan" + juce::String (i));
    }
    pLoopA = apvts.getRawParameterValue ("loopA");
    pLoopB = apvts.getRawParameterValue ("loopB");
    pChoke = apvts.getRawParameterValue ("choke");
    pMidiMode = apvts.getRawParameterValue ("midiMode");
    formatManager.registerBasicFormats();

    engine.rnd2 (uiRng);
    engine.rndN (uiRng);
    engine.rndG (uiRng, .35);
    loadPresetFile();
}

GltchProcessor::~GltchProcessor()
{
    if (loader != nullptr) loader->stopThread (5000);
}

void GltchProcessor::loadSampleFiles (const juce::Array<juce::File>& files, bool replace, bool keepOrder)
{
    if (loader != nullptr) loader->stopThread (5000);
    std::vector<juce::File> f;
    for (auto& x : files) f.push_back (x);
    std::vector<Sample> keep;
    if (! replace && current != nullptr) keep = current->s;
    const int gen = ++loadGen;
    loadDone = 0;
    loadTotal = 0;
    loading = true;
    loadMsg = U8 ("Chargement…");
    loader = std::make_unique<Loader> (*this, std::move (f), std::move (keep), gen, keepOrder);
    loader->startThread();
}

void GltchProcessor::clearSamples()
{
    if (loader != nullptr) loader->stopThread (5000);
    const int gen = ++loadGen;
    auto nb = std::make_shared<SampleBank>();
    nb->id = ++nextBankId;
    installBank (nb, gen, U8 ("Banque vidée"));
}

void GltchProcessor::installBank (std::shared_ptr<SampleBank> nb, int gen, const juce::String& msg)
{
    if (gen != loadGen.load()) return;
    loading = false;
    loadMsg = msg;
    if (nb == nullptr) return;
    current = nb;
    banks.push_back (nb);
    engine.setBank (nb.get());
    juce::WeakReference<GltchProcessor> weak (this);
    juce::Timer::callAfterDelay (3000, [weak] { if (auto* p = weak.get()) p->cleanBanks(); });
}

void GltchProcessor::cleanBanks()
{
    const int active = engine.activeBankId();
    const int latest = current != nullptr ? current->id : -1;
    banks.erase (std::remove_if (banks.begin(), banks.end(),
                                 [&] (const std::shared_ptr<SampleBank>& b) { return b->id != latest && b->id != active; }),
                 banks.end());
}

juce::String GltchProcessor::getSampleName (int i) const
{
    if (current == nullptr || i < 0 || i >= (int) current->s.size()) return {};
    return juce::String::fromUTF8 (current->s[(size_t) i].name.c_str());
}

void GltchProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate);
}

bool GltchProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (int i = 1; i < layouts.outputBuses.size(); ++i)
    {
        const auto s = layouts.getChannelSet (false, i);
        if (! s.isDisabled() && s != juce::AudioChannelSet::stereo()) return false;
    }
    return layouts.inputBuses.isEmpty() || layouts.getMainInputChannelSet().isDisabled();
}

// correspondance note MIDI -> piste ; renvoie -1 si ignoree. note = -1 : garde la note du motif.
static int mapMidi (int mode, int ch, int note, int& outNote)
{
    outNote = note;
    switch (mode)
    {
        case 1:
            outNote = -1;
            return (note >= 36 && note <= 41) ? note - 36 : -1;
        case 2: return note < 60 ? 4 : 5;
        case 3: return 4;
        case 4: return 5;
        default:
            if (ch == 2) return 4;
            if (ch == 3) return 5;
            if (ch == 1 || ch == 10)
            {
                outNote = -1;
                if (note == 35 || note == 36) return 0;
                if (note == 37 || note == 38 || note == 40) return 1;
                if (note == 42 || note == 44 || note == 46) return 2;
                if (note == 39) return 3;
            }
            return -1;
    }
}

void GltchProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (buffer.getNumChannels() < 2) return;

    Params p;
    p.bpm = pBpm->load();
    p.sw = pSw->load();
    p.chaos = pChaos->load();
    p.dens = pDens->load();
    p.root = (int) pRoot->load();
    p.sci = (int) pSci->load();
    p.autoRnd = pAuto->load() > .5f;
    uint32_t m = 0;
    for (int i = 0; i < FX_COUNT; ++i)
        if (pFx[i]->load() > .5f) m |= (1u << i);
    p.fx = m;
    for (int i = 0; i < 6; ++i) p.snd[i] = (int) pSnd[i]->load();
    for (int i = 0; i < 6; ++i)
    {
        p.smpOn[i] = pSmpOn[i]->load() > .5f;
        p.smpIdx[i] = (int) pSmpI[i]->load();
        p.mute[i] = pMute[i]->load() > .5f;
        p.solo[i] = pSolo[i]->load() > .5f;
    }
    for (int i = 0; i < FX_COUNT; ++i) p.fxAmt[i] = pFxA[i]->load() * .01f;
    for (int i = 0; i < 6; ++i)
    {
        p.vol[i] = pVol[i]->load() * .01f;
        p.pan[i] = pPan[i]->load() * .01f;
    }
    p.loopA = (int) pLoopA->load() - 1;
    p.loopB = (int) pLoopB->load() - 1;
    p.chokeHat = pChoke->load() > .5f;

    MidiEv evs[256];
    int nev = 0;
    const int nSamples = buffer.getNumSamples(), mmode = (int) pMidiMode->load();
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (! msg.isNoteOn() || nev >= 256) continue;
        int nn = 0;
        const int tr = mapMidi (mmode, msg.getChannel(), msg.getNoteNumber(), nn);
        if (tr < 0) continue;
        evs[nev].offset = juce::jlimit (0, juce::jmax (0, nSamples - 1), meta.samplePosition);
        evs[nev].track = tr;
        evs[nev].note = nn;
        evs[nev].vel = msg.getFloatVelocity();
        ++nev;
    }
    midi.clear();

    Aux ax;
    bool useAux = false;
    for (int k = 0; k < 6; ++k)
        if (auto* bus = getBus (false, k + 1))
            if (bus->isEnabled())
            {
                auto bb = getBusBuffer (buffer, false, k + 1);
                if (bb.getNumChannels() >= 2)
                {
                    ax.l[k] = bb.getWritePointer (0);
                    ax.r[k] = bb.getWritePointer (1);
                    useAux = true;
                }
            }

    Transport t;
    t.bpm = p.bpm;
    bool hostPlaying = false, havePpq = false;
    if (auto* ph = getPlayHead())
    {
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) { t.ppq = *q; havePpq = true; }
            if (auto b = pos->getBpm()) t.bpm = *b;
        }
    }
    t.mode = (hostPlaying && havePpq) ? 2 : (pPlay->load() > .5f ? 1 : 0);

    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), nSamples, p, t, evs, nev, useAux ? &ax : nullptr);
}

juce::AudioProcessorEditor* GltchProcessor::createEditor() { return new GltchEditor (*this); }

void GltchProcessor::setParamValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

float GltchProcessor::getParamValue (const juce::String& id) const
{
    if (auto* a = const_cast<juce::AudioProcessorValueTreeState&> (apvts).getRawParameterValue (id))
        return a->load();
    return 0.f;
}

// ------------------------------------------------------------ encodage des motifs
std::string GltchProcessor::encP (const Engine& engine)
{
    std::string s;
    s.reserve (6144);
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < Engine::N; ++i) s.push_back ((char) ('0' + (engine.P[r][i] & 3)));
    return s;
}

std::string GltchProcessor::encN (const Engine& engine)
{
    std::string s;
    s.reserve (6144);
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < Engine::N; ++i)
        {
            int v = std::min<int> (engine.NT[r][i], 35);
            s.push_back (v < 10 ? (char) ('0' + v) : (char) ('a' + v - 10));
        }
    return s;
}

std::string GltchProcessor::encG (const Engine& engine)
{
    static const char* hex = "0123456789abcdef";
    std::string s;
    s.reserve (1024);
    for (int i = 0; i < Engine::N; ++i) s.push_back (hex[engine.GL[i] & 15]);
    return s;
}

static int b36 (juce::juce_wchar c)
{
    if (c >= '0' && c <= '9') return (int) (c - '0');
    if (c >= 'a' && c <= 'z') return (int) (c - 'a') + 10;
    if (c >= 'A' && c <= 'Z') return (int) (c - 'A') + 10;
    return 0;
}

void GltchProcessor::decodePatterns (const juce::String& p, const juce::String& n, const juce::String& g)
{
    if (p.length() != 6144 || n.length() != 6144 || g.length() != 1024) return;
    for (int r = 0; r < 6; ++r)
        for (int i = 0; i < Engine::N; ++i)
        {
            engine.P[r][i] = (uint8_t) juce::jlimit (0, 3, b36 (p[r * 1024 + i]));
            engine.NT[r][i] = (uint8_t) juce::jlimit (0, 35, b36 (n[r * 1024 + i]));
        }
    for (int i = 0; i < Engine::N; ++i) engine.GL[i] = (uint8_t) juce::jlimit (0, 15, b36 (g[i]));
}

void GltchProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    st.setProperty ("pat_p", juce::String (encP (engine)), nullptr);
    st.setProperty ("pat_n", juce::String (encN (engine)), nullptr);
    st.setProperty ("pat_g", juce::String (encG (engine)), nullptr);
    st.setProperty ("theme", themeSeed.load(), nullptr);
    st.setProperty ("edW", editorW, nullptr);
    st.setProperty ("edH", editorH, nullptr);
    {
        juce::StringArray ss;
        for (int i = 0; i < MAX_SAMPLES; ++i)
        {
            float g, s0; bool rv;
            engine.getSmpSet (i, g, s0, rv);
            if (g != 1.f || s0 != 0.f || rv)
                ss.add (juce::String (i) + ":" + juce::String (g, 3) + ":" + juce::String (s0, 3) + ":" + (rv ? "1" : "0"));
        }
        st.setProperty ("smpset", ss.joinIntoString (";"), nullptr);
    }
    if (current != nullptr && ! current->s.empty())
    {
        juce::StringArray paths;
        for (auto& s : current->s) paths.add (juce::String::fromUTF8 (s.path.c_str()));
        st.setProperty ("samples", paths.joinIntoString ("\n"), nullptr);
    }
    if (auto xml = st.createXml()) copyXmlToBinary (*xml, dest);
}

void GltchProcessor::setStateInformation (const void* data, int size)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, size));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType())) return;
    auto st = juce::ValueTree::fromXml (*xml);
    decodePatterns (st.getProperty ("pat_p").toString(), st.getProperty ("pat_n").toString(), st.getProperty ("pat_g").toString());
    st.removeProperty ("pat_p", nullptr);
    st.removeProperty ("pat_n", nullptr);
    st.removeProperty ("pat_g", nullptr);
    const juce::String sp = st.getProperty ("samples").toString();
    st.removeProperty ("samples", nullptr);
    themeSeed = (int) st.getProperty ("theme", 0);
    editorW = (int) st.getProperty ("edW", 0);
    editorH = (int) st.getProperty ("edH", 0);
    engine.resetSmpSets();
    for (auto& e : juce::StringArray::fromTokens (st.getProperty ("smpset").toString(), ";", ""))
    {
        auto f = juce::StringArray::fromTokens (e, ":", "");
        if (f.size() == 4) engine.setSmpSet (f[0].getIntValue(), f[1].getFloatValue(), f[2].getFloatValue(), f[3] == "1");
    }
    for (auto* k : { "theme", "edW", "edH", "smpset" }) st.removeProperty (k, nullptr);
    apvts.replaceState (st);
    if (sp.isNotEmpty())
    {
        juce::Array<juce::File> fs;
        for (auto& line : juce::StringArray::fromLines (sp))
            if (line.isNotEmpty()) fs.add (juce::File (line));
        loadSampleFiles (fs, true, true);
    }
    else if (getNumSamples() > 0)
        clearSamples();
}

// ------------------------------------------------------------ presets (format compatible avec la page HTML)
juce::var GltchProcessor::encodePreset() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("v", 1);
    o->setProperty ("bpm", (double) pBpm->load());
    o->setProperty ("sw", (double) pSw->load());
    o->setProperty ("chaos", (double) pChaos->load());
    o->setProperty ("dens", (double) pDens->load());
    o->setProperty ("root", (int) pRoot->load());
    o->setProperty ("sci", (int) pSci->load());
    juce::Array<juce::var> sv;
    for (int i = 0; i < 6; ++i) sv.add ((int) pSnd[i]->load());
    o->setProperty ("sv", sv);
    juce::Array<juce::var> so, si;
    for (int i = 0; i < 6; ++i) { so.add (pSmpOn[i]->load() > .5f ? 1 : 0); si.add ((int) pSmpI[i]->load()); }
    o->setProperty ("smpOn", so);
    o->setProperty ("smpI", si);
    juce::Array<juce::var> fa;
    for (int i = 0; i < FX_COUNT; ++i) fa.add ((int) pFxA[i]->load());
    o->setProperty ("fxA", fa);
    juce::Array<juce::var> vl, pn;
    for (int i = 0; i < 6; ++i) { vl.add ((int) pVol[i]->load()); pn.add ((int) pPan[i]->load()); }
    o->setProperty ("vol", vl);
    o->setProperty ("pan", pn);
    o->setProperty ("loopA", (int) pLoopA->load());
    o->setProperty ("loopB", (int) pLoopB->load());
    o->setProperty ("choke", pChoke->load() > .5f ? 1 : 0);
    o->setProperty ("p", juce::String (encP (engine)));
    o->setProperty ("n", juce::String (encN (engine)));
    o->setProperty ("g", juce::String (encG (engine)));
    return juce::var (o);
}

juce::String GltchProcessor::getPresetName (int i) const
{
    const auto& v = presets[(size_t) i];
    if (v.isObject() && v.hasProperty ("name") && v["name"].toString().isNotEmpty()) return v["name"].toString();
    return "Preset " + juce::String (i + 1);
}

// presets d'usine : motifs generes de facon deterministe
juce::var GltchProcessor::makeFactory (int k) const
{
    struct F { const char* name; double bpm, sw, chaos, dens; int root, sci; uint64_t seed; };
    static const F fs[numFactory] = {
        { "Four Floor Glitch", 128, 0.f,  .15, .30, 0, 0, 101 },
        { "Breakbeat Chaos",   160, .20,  .35, .45, 2, 1, 202 },
        { "Techno 140",        140, 0.f,  .10, .35, 5, 2, 303 },
        { "Dub Stutter",        96, .30,  .25, .25, 7, 3, 404 },
        { "Acid Bass",         132, .10,  .20, .40, 9, 1, 505 },
        { "Minimal Click",     124, .05,  .05, .15, 3, 4, 606 },
        { "Trap Hats",         150, .15,  .30, .50, 10, 0, 707 },
        { "Stutter Storm",     180, .25,  .70, .60, 1, 5, 808 },
    };
    const F& f = fs[juce::jlimit (0, numFactory - 1, k)];
    auto tmp = std::make_unique<Engine>();
    Rng r (f.seed);
    tmp->rnd2 (r);
    tmp->rndN (r);
    tmp->rndG (r, f.dens);
    auto* o = new juce::DynamicObject();
    o->setProperty ("v", 1);
    o->setProperty ("name", juce::String (f.name));
    o->setProperty ("bpm", f.bpm);
    o->setProperty ("sw", f.sw);
    o->setProperty ("chaos", f.chaos);
    o->setProperty ("dens", f.dens);
    o->setProperty ("root", f.root);
    o->setProperty ("sci", f.sci);
    juce::Array<juce::var> sv;
    for (int i = 0; i < 6; ++i) sv.add (r.below (soundCount (i)));
    o->setProperty ("sv", sv);
    o->setProperty ("p", juce::String (encP (*tmp)));
    o->setProperty ("n", juce::String (encN (*tmp)));
    o->setProperty ("g", juce::String (encG (*tmp)));
    return juce::var (o);
}

void GltchProcessor::restoreFactoryPresets()
{
    for (int k = 0; k < numFactory; ++k) presets[(size_t) k] = makeFactory (k);
    savePresetFile();
}

bool GltchProcessor::presetOk (const juce::var& o)
{
    if (! o.isObject()) return false;
    return (int) o.getProperty ("v", 0) == 1
        && o.getProperty ("p", juce::var()).toString().length() == 6144
        && o.getProperty ("n", juce::var()).toString().length() == 6144
        && o.getProperty ("g", juce::var()).toString().length() == 1024
        && o.getProperty ("sv", juce::var()).isArray();
}

bool GltchProcessor::applyPresetVar (const juce::var& o)
{
    if (! presetOk (o)) return false;
    decodePatterns (o["p"].toString(), o["n"].toString(), o["g"].toString());
    if (auto* a = o["sv"].getArray())
        for (int r = 0; r < 6 && r < a->size(); ++r)
            setParamValue ("snd" + juce::String (r), (float) juce::jlimit (0, soundCount (r) - 1, (int) (*a)[r]));
    if (auto* a = o["smpOn"].getArray())
        for (int r = 0; r < 6 && r < a->size(); ++r)
            setParamValue ("smpOn" + juce::String (r), (int) (*a)[r] ? 1.f : 0.f);
    if (auto* a = o["smpI"].getArray())
        for (int r = 0; r < 6 && r < a->size(); ++r)
            setParamValue ("smpI" + juce::String (r), (float) juce::jlimit (0, MAX_SAMPLES - 1, (int) (*a)[r]));
    if (auto* a = o["fxA"].getArray())
        for (int r = 0; r < FX_COUNT && r < a->size(); ++r)
            setParamValue ("fxA" + juce::String (r), (float) juce::jlimit (0, 100, (int) (*a)[r]));
    if (auto* a = o["vol"].getArray())
        for (int r = 0; r < 6 && r < a->size(); ++r) setParamValue ("vol" + juce::String (r), (float) juce::jlimit (0, 200, (int) (*a)[r]));
    if (auto* a = o["pan"].getArray())
        for (int r = 0; r < 6 && r < a->size(); ++r) setParamValue ("pan" + juce::String (r), (float) juce::jlimit (-100, 100, (int) (*a)[r]));
    if (o.hasProperty ("loopA")) setParamValue ("loopA", (float) juce::jlimit (1, 16, (int) o["loopA"]));
    if (o.hasProperty ("loopB")) setParamValue ("loopB", (float) juce::jlimit (1, 16, (int) o["loopB"]));
    if (o.hasProperty ("choke")) setParamValue ("choke", (int) o["choke"] ? 1.f : 0.f);
    setParamValue ("root", (float) juce::jlimit (0, 11, (int) o.getProperty ("root", 0)));
    setParamValue ("sci", (float) juce::jlimit (0, 5, (int) o.getProperty ("sci", 0)));
    if (o.hasProperty ("bpm")) setParamValue ("bpm", (float) (double) o["bpm"]);
    if (o.hasProperty ("sw")) setParamValue ("sw", (float) (double) o["sw"]);
    if (o.hasProperty ("chaos")) setParamValue ("chaos", (float) (double) o["chaos"]);
    if (o.hasProperty ("dens")) setParamValue ("dens", (float) (double) o["dens"]);
    return true;
}

void GltchProcessor::storePreset (int i, const juce::String& name)
{
    auto v = encodePreset();
    const auto nm = name.trim().isNotEmpty() ? name.trim().substring (0, 24) : getPresetName (i);
    if (auto* o = v.getDynamicObject()) o->setProperty ("name", nm);
    presets[(size_t) i] = v;
    savePresetFile();
}

// ------------------------------------------------------------ copier / coller / effacer
void GltchProcessor::clipCopy (int scope, int track, int page)
{
    clip = Clip();
    clip.scope = scope;
    track = juce::jlimit (0, 5, track);
    page = juce::jlimit (0, 15, page);
    auto add = [&] (int r, int a, int b)
    {
        for (int i = a; i < b; ++i) { clip.p.push_back (engine.P[r][i]); clip.n.push_back (engine.NT[r][i]); }
    };
    if (scope == 0) add (track, page * 64, page * 64 + 64);
    else if (scope == 2) add (track, 0, Engine::N);
    else
    {
        for (int r = 0; r < 6; ++r) add (r, page * 64, page * 64 + 64);
        for (int i = page * 64; i < page * 64 + 64; ++i) clip.g.push_back (engine.GL[i]);
    }
    clip.valid = true;
}

bool GltchProcessor::clipPaste (int scope, int track, int page)
{
    if (! clipHas (scope)) return false;
    track = juce::jlimit (0, 5, track);
    page = juce::jlimit (0, 15, page);
    size_t k = 0;
    auto put = [&] (int r, int a, int b)
    {
        for (int i = a; i < b; ++i, ++k) { engine.P[r][i] = clip.p[k]; engine.NT[r][i] = clip.n[k]; }
    };
    if (scope == 0) put (track, page * 64, page * 64 + 64);
    else if (scope == 2) put (track, 0, Engine::N);
    else
    {
        for (int r = 0; r < 6; ++r) put (r, page * 64, page * 64 + 64);
        for (int i = 0; i < 64; ++i) engine.GL[page * 64 + i] = clip.g[(size_t) i];
    }
    return true;
}

void GltchProcessor::clipClear (int scope, int track, int page)
{
    track = juce::jlimit (0, 5, track);
    page = juce::jlimit (0, 15, page);
    auto z = [&] (int r, int a, int b) { for (int i = a; i < b; ++i) { engine.P[r][i] = 0; engine.NT[r][i] = 0; } };
    if (scope == 0) z (track, page * 64, page * 64 + 64);
    else if (scope == 2) z (track, 0, Engine::N);
    else
    {
        for (int r = 0; r < 6; ++r) z (r, page * 64, page * 64 + 64);
        for (int i = page * 64; i < page * 64 + 64; ++i) engine.GL[i] = 0;
    }
}

void GltchProcessor::clearPreset (int i)
{
    presets[(size_t) i] = juce::var();
    savePresetFile();
}

bool GltchProcessor::applyPreset (int i) { return applyPresetVar (presets[(size_t) i]); }

juce::String GltchProcessor::presetsAsJson() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("gltch1024", 1);
    juce::Array<juce::var> arr;
    for (auto& v : presets) arr.add (v.isObject() ? v : juce::var());
    root->setProperty ("presets", arr);
    return juce::JSON::toString (juce::var (root), true);
}

int GltchProcessor::importPresetsJson (const juce::String& text)
{
    auto j = juce::JSON::parse (text);
    auto* arr = j["presets"].getArray();
    if (arr == nullptr) return -1;
    int n = 0;
    for (int i = 0; i < numPresets; ++i)
    {
        if (i < arr->size() && presetOk ((*arr)[i])) { presets[(size_t) i] = (*arr)[i]; ++n; }
        else presets[(size_t) i] = juce::var();
    }
    savePresetFile();
    return n;
}

juce::File GltchProcessor::presetFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("GLTCH-1024").getChildFile ("presets.json");
}

void GltchProcessor::savePresetFile()
{
    auto f = presetFile();
    f.getParentDirectory().createDirectory();
    f.replaceWithText (presetsAsJson());
}

void GltchProcessor::loadPresetFile()
{
    auto f = presetFile();
    if (f.existsAsFile()) importPresetsJson (f.loadFileAsString());
    else restoreFactoryPresets();
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new GltchProcessor(); }
