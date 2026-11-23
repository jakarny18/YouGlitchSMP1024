// GLTCH-1024 : moteur audio / séquenceur (C++ standard uniquement, sans JUCE)
#pragma once
#include <array>
#include <algorithm>
#include <atomic>
#include <memory>
#include <cstdint>
#include <string>
#include <vector>

namespace gltch {

struct Rng {
    uint64_t s;
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 1) {}
    double operator()() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return (double)(s >> 11) * (1.0 / 9007199254740992.0);
    }
    int below(int n) { return (int)((*this)() * n); }
};

// Ordre des FX perf (bits du masque)
enum Fx { FX_F2, FX_DV, FX_DE, FX_DR, FX_KL, FX_RG, FX_DS, FX_LP, FX_HP, FX_DL, FX_PP, FX_RV, FX_PD, FX_PU, FX_HF, FX_COUNT };

struct Snd {
    std::string name;
    double p[6] = {0, 0, 0, 0, 0, 0};
    int wave = 0;  // 0 sine 1 triangle 2 square 3 saw ; piste HAT : 0 highpass 1 bandpass
};
const std::vector<Snd>& sounds(int track);
int soundCount(int track);

struct Scale { const char* name; int n; int notes[12]; };
extern const Scale SCALES[6];
extern const char* const NOTE_NAMES[12];
extern const char* const TRACK_NAMES[6];
extern const char* const GLITCH_NAMES[16];
extern const char* const GLITCH_SHORT[16];
extern const char* const FX_NAMES[FX_COUNT];

// ---- banque de samples (mono 16 bits, chargée hors du thread audio)
constexpr int MAX_SAMPLES = 5000;
struct Sample {
    std::string name;
    std::string path;  // chemin du fichier (UTF-8), pour la sauvegarde du projet
    double sr = 44100.0;
    std::shared_ptr<const std::vector<int16_t>> data;   // canal gauche (ou mono), 16 bits
    std::shared_ptr<const std::vector<int16_t>> dataR;  // canal droit si le fichier est stéréo, sinon nul
};
struct SampleBank {
    std::vector<Sample> s;
    int id = 0;
};

struct Params {
    float bpm = 150.f, sw = 0.f, chaos = .15f, dens = .35f;
    int root = 0, sci = 0;
    uint32_t fx = 0;
    bool autoRnd = false;
    int snd[6] = {0, 0, 0, 0, 0, 0};
    bool smpOn[6] = {false, false, false, false, false, false};
    int smpIdx[6] = {0, 0, 0, 0, 0, 0};
    bool mute[6] = {false, false, false, false, false, false};
    bool solo[6] = {false, false, false, false, false, false};
    float fxAmt[FX_COUNT];  // intensité de chaque effet, 0..1 (1 = comportement d'origine)
    float vol[6] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f};   // volume par piste (1 = 100 %)
    float pan[6] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f};   // balance par piste, -1 (gauche) .. 1 (droite)
    int loopA = 0, loopB = 15;                       // plage de pages jouée en boucle (0..15, bornes incluses)
    bool chokeHat = false;                           // un nouveau coup de charleston coupe le précédent
    Params() { for (auto& a : fxAmt) a = 1.f; }
};

// sorties séparées : pointeurs non nuls = piste envoyée sur sa propre sortie (sec, avec volume/pan), hors du mix
struct Aux {
    float* l[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
    float* r[6] = {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr};
};

// note MIDI à déclencher dans le bloc (offset en échantillons depuis le début du bloc)
struct MidiEv { int offset = 0; int track = 0; int note = 60; float vel = 1.f; };

// réglages par sample (lus par le thread audio, écrits par l'interface)
struct SmpSet {
    std::atomic<float> gain{1.f};   // 0..2
    std::atomic<float> start{0.f};  // 0..0.95 : fraction de la durée ignorée au début
    std::atomic<bool> rev{false};   // lecture à l'envers
};

struct Transport {
    int mode = 0;      // 0 arrêté, 1 horloge interne, 2 synchronisé à l'hôte
    double ppq = 0.0;  // position hôte (noires) au début du bloc
    double bpm = 150.0;
};

struct Biquad {
    enum { LP, HP, BP };
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void set(int type, double f, double q, double sr);
    inline double process(double x) {
        double y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }
};

struct Voice {
    bool active = false;
    int kind = 0;  // 0 osc, 1 bruit, 2 basse, 3 FM, 4 sample
    int wave = 0;
    double ph = 0, ph2 = 0, fcur = 0, fmul = 1, g = 0, gmul = 1;
    int64_t fdN = 0, decN = 0, stopN = 0, age = 0, sweepN = 0;
    double c0 = 0, c1 = 0, qdb = 0, ratio = 0, mg = 0, mmul = 1, detune = 1;
    Biquad bq;
    const int16_t* sd = nullptr;
    const int16_t* sdR = nullptr;  // canal droit (nul = mono)
    int64_t slen = 0;
    double spos = 0, srate = 1, sEnd = 0;
    int trk = 0;                   // piste d'origine
    bool releasing = false;        // coupure rapide (choke)
    int64_t relN = 0, relAge = 0;
    double rOut = 0;               // canal droit du dernier échantillon produit par tick()
    double tick(double sr, Rng& rng);
};

struct Delay {
    std::vector<float> b;
    int w = 0;
    void init(int n) { b.assign((size_t)n, 0.f); w = 0; }
    int size() const { return (int)b.size(); }
    float read(int d) const { int r = w - d; if (r < 0) r += (int)b.size(); return b[(size_t)r]; }
    void write(float x) { b[(size_t)w] = x; if (++w >= (int)b.size()) w = 0; }
};

struct Reverb {
    struct Comb { std::vector<float> b; int p = 0; float s = 0.f; };
    Comb c[4];
    std::vector<float> a[2];
    int ap[2] = {0, 0};
    void init(double sr);
    float process(float in);
};

class Engine {
public:
    static constexpr int N = 1024;
    uint8_t P[6][N];
    uint8_t NT[6][N];
    uint8_t GL[N];

    std::atomic<int> curStep{0};
    std::atomic<int> previewMask{0};
    std::atomic<bool> running{false};

    Engine();
    void prepare(double sampleRate);
    // aux : sorties séparées (peut être nul) ; ev/nev : notes MIDI triées par offset
    void process(float* L, float* R, int n, const Params& p, const Transport& t,
                 const MidiEv* ev = nullptr, int nev = 0, const Aux* aux = nullptr);

    // génération (thread UI ou audio selon l'appelant, avec son propre Rng)
    void rndG(Rng& r, double dens);
    void rnd2(Rng& r);
    void rndN(Rng& r);
    void mutate(Rng& r);
    void clearAll();
    void seedRng(uint64_t s) { rng = Rng(s); }

    // Banque de samples : appelable depuis le thread UI. La banque doit rester valide
    // tant que activeBankId() lui correspond (et quelques secondes après un changement).
    void setBank(const SampleBank* b);
    int activeBankId() const { return activeId.load(); }

    // réglages par sample (index dans la banque)
    void setSmpSet(int i, float gain, float start, bool rev);
    void getSmpSet(int i, float& gain, float& start, bool& rev) const;
    void resetSmpSets();

private:
    struct Pend { int64_t at; Voice v; };
    struct Ov { float ring = -1, dist = -1, lp = -1, hp = -1, dl = -1, pp = -1, rv = -1, kill = 0; };

    double doStep(int st);
    void enterMode(int m);
    void resetOv();
    void glitch(int ty, double sdS);
    void dive(double sdS, double amt);
    void rep(double sdS, int div, double amt);
    void trigTrack(int r, int st, double vol, int64_t at, double pm, int midiNote = -1);
    int loStep() const { int a = std::min(prm.loopA, prm.loopB); return (a < 0 ? 0 : (a > 15 ? 15 : a)) * 64; }
    int hiStep() const { int b = std::max(prm.loopA, prm.loopB); return (b < 0 ? 0 : (b > 15 ? 15 : b)) * 64 + 63; }
    void push(const Voice& v, int64_t at);
    void activate(const Voice& v);
    int note(int r, int i, int base) const;
    double A(int fx) const { return prm.fxAmt[fx] < 0.f ? 0.0 : (prm.fxAmt[fx] > 1.f ? 1.0 : (double)prm.fxAmt[fx]); }
    bool audible(int r) const {
        if (prm.mute[r]) return false;
        for (int k = 0; k < 6; ++k) if (prm.solo[k]) return prm.solo[r];
        return true;
    }

    Voice mkOsc(int wave, double f0, double f1, double fd, double dec, double vol) const;
    Voice mkNoise(int ftype, double f, double q, double dec, double vol) const;
    Voice mkBass(int wave, double cents, double f, double fs, double fe, double qdb, double dur, double vol) const;
    Voice mkSample(const Sample& s, double pitch, double vol, float gain, float startFrac, bool reverse) const;
    Voice mkFm(int wave, double f, double ratio, double idx, double d, double vol) const;

    double sr = 44100.0, ebpm = 150.0;
    Params prm;
    int mode = 0, SV[6] = {0, 0, 0, 0, 0, 0};
    Rng rng;
    int64_t now = 0;

    // banque de samples
    std::atomic<const SampleBank*> pendingBank{nullptr};
    std::atomic<bool> hasPending{false};
    std::atomic<int> activeId{-1};
    const SampleBank* bank = nullptr;
    std::unique_ptr<SmpSet[]> sset;
    int curTrk = 0;

    // séquenceur
    int step = 0, lastStep = -1;
    double samplesToNext = 0;
    long long lastHostStep = 0;
    Ov ov;
    bool diveOn = false, rrOn = false, fzOn = false;
    double diveAmt = 1.0, fzAmt = 1.0;
    double diveAge = 0, diveLen = 1, roF = 220.0, roPh = 0.0;
    int fzAge = 0, fzL = 1, fzEnd = 0;

    static constexpr int NV = 96, NP = 512;
    Voice voices[NV];
    Pend pend[NP];
    int npend = 0;

    // chaîne
    Biquad hpF, lpF, hpR, lpR;
    double volS[6] = {1, 1, 1, 1, 1, 1}, panS[6] = {0, 0, 0, 0, 0, 0};
    double ringV = 0, distV = 0, lpV = 20000, hpV = 20, dlV = 0, ppV = 0, rvV = 0;
    Delay dl, pa1, pa2, fz, dlR, fzR;
    Reverb rev, revR;
    double cgr = 0.0;
};

}  // namespace gltch
