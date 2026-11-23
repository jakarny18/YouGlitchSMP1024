#include "Engine.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace gltch {

static const double TWO_PI = 6.283185307179586476925286766559;

const char* const TRACK_NAMES[6] = {"KICK", "SNARE", "HAT", "CLAP", "BASS", "LEAD"};
const char* const NOTE_NAMES[12] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
const char* const GLITCH_NAMES[16] = {"-", "FRZ 1/2", "DV ECHO", "DIVE", "KILL", "RING", "DIST", "LP", "HP", "DELAY", "PING", "REVERB", "OCT-", "OCT+", "1/2 TPO", "DV RING"};
const char* const GLITCH_SHORT[16] = {"", "F2", "DE", "DV", "KL", "RG", "DS", "LP", "HP", "DL", "PP", "RV", "O-", "O+", "H", "DR"};
const char* const FX_NAMES[FX_COUNT] = {"FRZ 1/2", "DIVE", "DIVE ECHO", "DIVE RING", "KILL", "RING", "DIST", "LP", "HP", "DELAY", "PING-PONG", "REVERB", "OCT -", "OCT +", "1/2 TEMPO"};

const Scale SCALES[6] = {
    {"MINEUR", 7, {0, 2, 3, 5, 7, 8, 10}},
    {"PHRYGIEN", 7, {0, 1, 3, 5, 7, 8, 10}},
    {"CHROMA", 12, {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11}},
    {"WHOLE", 6, {0, 2, 4, 6, 8, 10}},
    {"DIM", 8, {0, 2, 3, 5, 6, 8, 9, 11}},
    {"PENTA", 5, {0, 3, 5, 7, 10}}};

// ---------------------------------------------------------------- sons
namespace {

struct Mul {  // mulberry32, identique au JS
    uint32_t a;
    explicit Mul(uint32_t s) : a(s) {}
    double operator()() {
        a += 0x6D2B79F5u;
        uint32_t t = (a ^ (a >> 15)) * (1u | a);
        t = (t + ((t ^ (t >> 7)) * (61u | t))) ^ t;
        return (double)(t ^ (t >> 14)) / 4294967296.0;
    }
};

const char* const NA[20] = {"Dusty", "Wet", "Raw", "Crisp", "Hollow", "Fat", "Thin", "Wild", "Dark", "Bright",
                            "Toxic", "Glass", "Rusty", "Fuzzy", "Tight", "Loose", "Cold", "Warm", "Sharp", "Dull"};

Snd mk(const char* n, double a, double b, double c, double d, double e, double f, int w) {
    Snd s;
    s.name = n;
    s.p[0] = a; s.p[1] = b; s.p[2] = c; s.p[3] = d; s.p[4] = e; s.p[5] = f;
    s.wave = w;
    return s;
}

std::vector<Snd> build(int tr) {
    std::vector<Snd> L;
    if (tr == 0) {
        L = {mk("808", 160, 40, .45, .3, 2000, .08, 0), mk("DEEP", 110, 35, .7, .1, 2000, .08, 0),
             mk("PUNCH", 220, 50, .25, .5, 2000, .08, 0), mk("TICK", 320, 70, .12, .8, 2000, .08, 0),
             mk("SUB", 80, 30, .9, 0, 2000, .08, 0), mk("SQR", 180, 45, .35, .3, 2000, .08, 2)};
    } else if (tr == 1) {
        L = {mk("TIGHT", 200, 120, 1800, .12, .12, .5, 1), mk("FAT", 170, 100, 1200, .25, .12, .5, 1),
             mk("RIM", 420, 300, 3200, .04, .12, .5, 1), mk("NOISE", 0, 0, 900, .3, .12, .5, 1),
             mk("SNAP", 260, 150, 2500, .08, .12, .5, 1), mk("LOFI", 140, 90, 600, .18, .12, .5, 1)};
    } else if (tr == 2) {
        L = {mk("CLOSED", 8000, 1, .035, 0, 0, 0, 0), mk("TICK", 11000, 1, .015, 0, 0, 0, 0),
             mk("OPEN", 6500, 1, .2, 0, 0, 0, 0), mk("BAND", 9000, 3, .05, 0, 0, 0, 1),
             mk("SHK", 5000, 1.5, .08, 0, 0, 0, 1), mk("DIRT", 4000, .7, .06, 0, 0, 0, 0)};
    } else if (tr == 3) {
        L = {mk("CLAP", 1300, 1.2, .16, 3, 0, 0, 0), mk("SNAP", 2200, 2, .06, 2, 0, 0, 0),
             mk("WIDE", 900, .8, .3, 4, 0, 0, 0), mk("ZAP", 3000, 4, .05, 1, 0, 0, 0),
             mk("LOW", 600, 1, .2, 3, 0, 0, 0), mk("CRACK", 1700, 6, .1, 5, 0, 0, 0)};
    } else if (tr == 4) {
        L = {mk("ACID", 2400, 160, 9, .22, 0, .4, 3), mk("SUB", 400, 100, 1, .3, 0, .4, 0),
             mk("SQR", 1800, 200, 5, .2, 0, .4, 2), mk("REESE", 800, 300, 2, .4, 0, .4, 3),
             mk("PLUCK", 3000, 200, 3, .12, 0, .4, 1), mk("FUZZ", 4000, 300, 12, .25, 0, .4, 2)};
    } else {
        L = {mk("BELL", 3.01, 2.5, .28, 0, 0, 0, 0), mk("GLASS", 5.02, 3.5, .4, 0, 0, 0, 0),
             mk("PLUCK", 1, 1.5, .15, 0, 0, 0, 0), mk("METAL", 1.41, 6, .25, 0, 0, 0, 0),
             mk("SOFT", 2, .8, .35, 0, 0, 0, 0), mk("ACID", .5, 4, .18, 0, 0, 0, 0)};
    }
    static const int BK[6] = {418, 418, 417, 417, 417, 417};
    for (int n = (int)L.size(); n < BK[tr]; ++n) {
        Mul rr((uint32_t)(tr * 7919 + n * 104729 + 5));
        auto r = [&]() { return rr(); };
        auto U = [&](double a, double b) { return a + (b - a) * r(); };
        auto E = [&](double a, double b) { return a * std::pow(b / a, r()); };
        std::string nm = NA[(int)(r() * 20)];
        Snd s;
        if (tr == 0) {
            static const int W[4] = {0, 0, 1, 2};
            double f = E(70, 380);
            double f1 = f * U(.2, .5);
            double dec = E(.1, .95);
            double nv = U(0, .9);
            int w = W[(int)(r() * 4)];
            s.p[0] = f; s.p[1] = f1; s.p[2] = dec; s.p[3] = nv; s.p[4] = 2000; s.p[5] = .08; s.wave = w;
            if (n >= 84) { s.p[4] = E(900, 6000); s.p[5] = E(.03, .16); }
        } else if (tr == 1) {
            double f = (r() < .25) ? 0.0 : E(120, 450);
            double f2 = f * U(.5, .9);
            double nf = E(500, 4500);
            double nd = E(.04, .35);
            s.p[0] = f; s.p[1] = f2; s.p[2] = nf; s.p[3] = nd; s.p[4] = .12; s.p[5] = .5; s.wave = 1;
            if (n >= 84) {
                static const int W[4] = {1, 0, 2, 3};
                s.wave = W[(int)(r() * 4)];
                s.p[4] = E(.05, .22);
                s.p[5] = U(.3, .7);
            }
        } else if (tr == 2) {
            static const int W[3] = {0, 0, 1};
            s.wave = W[(int)(r() * 3)];
            s.p[0] = E(3500, 12000); s.p[1] = U(.7, 4); s.p[2] = E(.012, .25);
        } else if (tr == 3) {
            s.p[0] = E(500, 3500); s.p[1] = U(.8, 6); s.p[2] = E(.04, .35);
            s.p[3] = (double)(int)(1 + r() * 5);
        } else if (tr == 4) {
            static const int W[4] = {3, 2, 1, 0};
            s.wave = W[(int)(r() * 4)];
            s.p[0] = E(800, 4500); s.p[1] = E(100, 400); s.p[2] = U(1, 12); s.p[3] = E(.1, .45);
            s.p[4] = 0; s.p[5] = .4;
            if (n >= 84) { s.p[4] = U(-25, 25); s.p[5] = U(.25, .5); }
        } else {
            static const double RT[8] = {.5, 1, 1.41, 2, 3.01, 3.5, 5.02, 7};
            double ratio;
            if (r() < .5) ratio = RT[(int)(r() * 8)];
            else ratio = U(.5, 7);
            s.p[0] = ratio; s.p[1] = E(.8, 7); s.p[2] = E(.12, .5);
            s.wave = 0;
            if (n >= 84) { static const int W[3] = {0, 0, 1}; s.wave = W[(int)(r() * 3)]; }
        }
        if (n >= 84) {
            Mul r2((uint32_t)(tr * 104729 + n * 7919 + 17));
            nm = nm + " " + NA[(int)(r2() * 20)];
        }
        s.name = nm;
        L.push_back(s);
    }
    return L;
}

inline double blep(double t, double dt) {
    dt = std::fabs(dt);
    if (dt <= 0.0) return 0.0;
    if (t < dt) { t /= dt; return t + t - t * t - 1.0; }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return t * t + t + t + 1.0; }
    return 0.0;
}

inline double oscTick(int wave, double& ph, double inc) {
    double y;
    switch (wave) {
        case 0: y = std::sin(TWO_PI * ph); break;
        case 1: { double t = ph + 0.75; t -= std::floor(t); y = 4.0 * std::fabs(t - 0.5) - 1.0; break; }
        case 2: y = ph < 0.5 ? 1.0 : -1.0; y += blep(ph, inc); y -= blep(std::fmod(ph + 0.5, 1.0), inc); break;
        default: y = 2.0 * ph - 1.0; y -= blep(ph, inc); break;
    }
    ph += inc;
    ph -= std::floor(ph);
    return y;
}

inline int clampI(int v, int a, int b) { return v < a ? a : (v > b ? b : v); }

}  // namespace

const std::vector<Snd>& sounds(int tr) {
    static const std::array<std::vector<Snd>, 6> T = [] {
        std::array<std::vector<Snd>, 6> a;
        for (int i = 0; i < 6; ++i) a[(size_t)i] = build(i);
        return a;
    }();
    return T[(size_t)tr];
}
int soundCount(int tr) { return (int)sounds(tr).size(); }

// ---------------------------------------------------------------- DSP de base
void Biquad::set(int type, double f, double q, double sr) {
    f = std::max(10.0, std::min(f, sr * 0.45));
    q = std::max(q, 0.01);
    double w = TWO_PI * f / sr, c = std::cos(w), s = std::sin(w), al = s / (2.0 * q), a0 = 1.0 + al;
    double nb0, nb1, nb2;
    if (type == LP) { nb0 = (1 - c) * .5; nb1 = 1 - c; nb2 = nb0; }
    else if (type == HP) { nb0 = (1 + c) * .5; nb1 = -(1 + c); nb2 = nb0; }
    else { nb0 = al; nb1 = 0; nb2 = -al; }
    b0 = nb0 / a0; b1 = nb1 / a0; b2 = nb2 / a0;
    a1 = -2.0 * c / a0; a2 = (1.0 - al) / a0;
}

double Voice::tick(double sr, Rng& r) {
    rOut = 0.0;
    if (age >= stopN) { active = false; return 0.0; }
    double y = 0.0, yr = 0.0;
    bool st = false;
    switch (kind) {
        case 0:
            y = oscTick(wave, ph, fcur / sr);
            if (age < fdN) fcur *= fmul;
            break;
        case 1:
            y = bq.process(r() * 2.0 - 1.0);
            break;
        case 2: {
            if ((age & 7) == 0) {
                double t = age >= sweepN ? 1.0 : (double)age / (double)sweepN;
                bq.set(Biquad::LP, c0 * std::pow(c1 / c0, t), std::pow(10.0, qdb / 20.0), sr);
            }
            y = bq.process(oscTick(wave, ph, fcur * detune / sr));
            break;
        }
        case 4: {
            const bool fwd = srate >= 0.0;
            if (sd == nullptr || spos < 0.0 || (fwd ? spos >= sEnd : spos <= sEnd)) { active = false; return 0.0; }
            const int64_t i0 = (int64_t)spos;
            const double fr = spos - (double)i0;
            y = ((double)sd[i0] * (1.0 - fr) + (double)sd[i0 + 1] * fr) * (1.0 / 32768.0);
            if (sdR != nullptr) {
                yr = ((double)sdR[i0] * (1.0 - fr) + (double)sdR[i0 + 1] * fr) * (1.0 / 32768.0);
                st = true;
            }
            spos += srate;
            double env = 1.0;
            if (age < 16) env = (double)age / 16.0;
            const double fo = 64.0 * std::fabs(srate), rem = fwd ? (sEnd - spos) : (spos - sEnd);
            if (rem < fo) env *= std::max(0.0, rem / fo);
            y *= env;
            yr *= env;
            break;
        }
        default: {
            double fm = fcur + mg * std::sin(TWO_PI * ph2);
            ph2 += fcur * ratio / sr;
            ph2 -= std::floor(ph2);
            y = oscTick(wave, ph, fm / sr);
            if (age < sweepN) mg *= mmul;
            break;
        }
    }
    double a = g;
    if (age < decN) g *= gmul;
    ++age;
    double o = y * a;
    rOut = st ? yr * a : o;
    if (releasing) {
        const double k = 1.0 - (double)relAge / (double)relN;
        if (k <= 0.0) { active = false; rOut = 0.0; return 0.0; }
        ++relAge;
        o *= k;
        rOut *= k;
    }
    return o;
}

void Reverb::init(double sr) {
    static const int ct[4] = {1116, 1188, 1277, 1356}, at[2] = {556, 441};
    for (int i = 0; i < 4; ++i) { c[i].b.assign((size_t)std::max(8, (int)(ct[i] * sr / 44100.0)), 0.f); c[i].p = 0; c[i].s = 0.f; }
    for (int i = 0; i < 2; ++i) { a[i].assign((size_t)std::max(8, (int)(at[i] * sr / 44100.0)), 0.f); ap[i] = 0; }
}
float Reverb::process(float in) {
    float x = in * 0.03f, sum = 0.f;
    for (auto& k : c) {
        float o = k.b[(size_t)k.p];
        k.s = o * 0.8f + k.s * 0.2f;
        k.b[(size_t)k.p] = x + k.s * 0.86f;
        if (++k.p >= (int)k.b.size()) k.p = 0;
        sum += o;
    }
    for (int i = 0; i < 2; ++i) {
        float bo = a[i][(size_t)ap[i]];
        float o = -sum + bo;
        a[i][(size_t)ap[i]] = sum + bo * 0.5f;
        if (++ap[i] >= (int)a[i].size()) ap[i] = 0;
        sum = o;
    }
    return sum * 3.0f;
}

// ---------------------------------------------------------------- moteur
Engine::Engine() {
    for (auto& r : P) std::fill(r, r + N, (uint8_t)0);
    for (auto& r : NT) std::fill(r, r + N, (uint8_t)0);
    std::fill(GL, GL + N, (uint8_t)0);
    rng = Rng((uint64_t)std::chrono::steady_clock::now().time_since_epoch().count() * 2654435761ull + 12345);
    for (int i = 0; i < 6; ++i) (void)sounds(i);  // construit les tables hors du thread audio
    sset.reset(new SmpSet[MAX_SAMPLES]);
    prepare(44100.0);
}

void Engine::prepare(double sampleRate) {
    sr = sampleRate;
    int n = (int)(sr * 2.2);
    dl.init(n); pa1.init(n); pa2.init(n); fz.init(n); dlR.init(n); fzR.init(n);
    rev.init(sr); revR.init(sr);
    for (int k = 0; k < 6; ++k) { volS[k] = 1.0; panS[k] = 0.0; }
    for (auto& v : voices) v.active = false;
    npend = 0; now = 0; cgr = 0.0;
    mode = 0; step = 0; lastStep = -1; samplesToNext = 0;
    resetOv();
    ringV = distV = dlV = ppV = rvV = 0; lpV = 20000; hpV = 20;
    hpF = Biquad(); lpF = Biquad(); hpR = Biquad(); lpR = Biquad();
}

void Engine::resetOv() {
    ov = Ov();
    diveOn = false;
    if (rrOn) { roF = 220.0; rrOn = false; }
    fzOn = false;
}

void Engine::enterMode(int m) {
    mode = m;
    if (m == 1) { step = loStep(); samplesToNext = 0; lastStep = -1; }
    else if (m == 2) { lastHostStep = -(1LL << 40); lastStep = -1; }
    else { npend = 0; }
    resetOv();
}

int Engine::note(int r, int i, int base) const {
    const Scale& s = SCALES[clampI(prm.sci, 0, 5)];
    int d = NT[r][i];
    return base + prm.root + s.notes[d % s.n] + 12 * (d / s.n);
}

Voice Engine::mkOsc(int wave, double f0, double f1, double fd, double dec, double vol) const {
    Voice v; v.kind = 0; v.wave = wave; v.fcur = f0;
    v.fdN = (int64_t)(fd * sr);
    v.fmul = (f0 > 0 && f1 > 0 && v.fdN > 0) ? std::pow(f1 / f0, 1.0 / (double)v.fdN) : 1.0;
    v.g = vol; v.decN = std::max<int64_t>(1, (int64_t)(dec * sr));
    v.gmul = std::pow(0.001 / std::max(vol, 1e-6), 1.0 / (double)v.decN);
    v.stopN = (int64_t)((dec + 0.02) * sr);
    return v;
}

Voice Engine::mkNoise(int ftype, double f, double q, double dec, double vol) const {
    Voice v; v.kind = 1;
    v.bq.set(ftype == 0 ? Biquad::HP : Biquad::BP, f, ftype == 0 ? std::pow(10.0, q / 20.0) : q, sr);
    v.g = vol; v.decN = std::max<int64_t>(1, (int64_t)(dec * sr));
    v.gmul = std::pow(0.001 / std::max(vol, 1e-6), 1.0 / (double)v.decN);
    v.stopN = (int64_t)((dec + 0.02) * sr);
    return v;
}

Voice Engine::mkBass(int wave, double cents, double f, double fs, double fe, double qdb, double dur, double vol) const {
    Voice v; v.kind = 2; v.wave = wave; v.fcur = f; v.detune = std::pow(2.0, cents / 1200.0);
    v.c0 = fs; v.c1 = fe; v.qdb = qdb;
    v.sweepN = std::max<int64_t>(1, (int64_t)(dur * sr));
    v.g = vol; v.decN = std::max<int64_t>(1, (int64_t)((dur + 0.02) * sr));
    v.gmul = std::pow(0.001 / std::max(vol, 1e-6), 1.0 / (double)v.decN);
    v.stopN = (int64_t)((dur + 0.05) * sr);
    v.bq.set(Biquad::LP, fs, std::pow(10.0, qdb / 20.0), sr);
    return v;
}

Voice Engine::mkFm(int wave, double f, double ratio, double idx, double d, double vol) const {
    Voice v; v.kind = 3; v.wave = wave; v.fcur = f; v.ratio = ratio;
    v.mg = std::max(f * idx, 1e-3);
    v.sweepN = std::max<int64_t>(1, (int64_t)(d * sr));
    v.mmul = std::pow(1.0 / v.mg, 1.0 / (double)v.sweepN);
    v.g = .22 * vol; v.decN = std::max<int64_t>(1, (int64_t)((d + 0.03) * sr));
    v.gmul = std::pow(0.001 / std::max(v.g, 1e-6), 1.0 / (double)v.decN);
    v.stopN = (int64_t)((d + 0.05) * sr);
    return v;
}

Voice Engine::mkSample(const Sample& s, double pitch, double vol, float gain, float startFrac, bool reverse) const {
    Voice v; v.kind = 4;
    v.sd = s.data->data();
    v.sdR = (s.dataR && s.dataR->size() == s.data->size()) ? s.dataR->data() : nullptr;
    v.slen = (int64_t)s.data->size();
    const double rate = std::max(0.05, std::min(16.0, s.sr / sr * pitch));
    const double startIdx = std::min((double)(v.slen - 2), std::max(0.0, (double)startFrac * (double)(v.slen - 1)));
    if (!reverse) { v.spos = startIdx; v.sEnd = (double)(v.slen - 1); v.srate = rate; }
    else { v.spos = (double)(v.slen - 2); v.sEnd = startIdx; v.srate = -rate; }
    v.g = vol * (double)gain;
    v.stopN = (int64_t)(std::fabs(v.sEnd - v.spos) / rate) + 4;
    return v;
}

void Engine::setSmpSet(int i, float gain, float start, bool rev) {
    if (i < 0 || i >= MAX_SAMPLES) return;
    sset[(size_t)i].gain.store(std::max(0.f, std::min(2.f, gain)));
    sset[(size_t)i].start.store(std::max(0.f, std::min(.95f, start)));
    sset[(size_t)i].rev.store(rev);
}

void Engine::getSmpSet(int i, float& gain, float& start, bool& rev) const {
    gain = 1.f; start = 0.f; rev = false;
    if (i < 0 || i >= MAX_SAMPLES) return;
    gain = sset[(size_t)i].gain.load();
    start = sset[(size_t)i].start.load();
    rev = sset[(size_t)i].rev.load();
}

void Engine::resetSmpSets() {
    for (int i = 0; i < MAX_SAMPLES; ++i) setSmpSet(i, 1.f, 0.f, false);
}

void Engine::setBank(const SampleBank* b) {
    pendingBank.store(b);
    hasPending.store(true);
}

void Engine::push(const Voice& v, int64_t at) {
    if (npend >= NP) return;
    pend[npend].at = at;
    pend[npend].v = v;
    pend[npend].v.trk = curTrk;
    ++npend;
}

void Engine::activate(const Voice& v) {
    if (prm.chokeHat && v.trk == 2)
        for (auto& o : voices)
            if (o.active && o.trk == 2 && !o.releasing) {
                o.releasing = true;
                o.relN = std::max<int64_t>(1, (int64_t)(.004 * sr));
                o.relAge = 0;
            }
    int free = -1, oldest = 0;
    int64_t best = -1;
    for (int i = 0; i < NV; ++i) {
        if (!voices[i].active) { free = i; break; }
        if (voices[i].age > best) { best = voices[i].age; oldest = i; }
    }
    int k = free >= 0 ? free : oldest;
    voices[k] = v;
    voices[k].active = true;
}

void Engine::trigTrack(int r, int st, double vol, int64_t at, double pm, int midiNote) {
    curTrk = r;
    if (prm.smpOn[r] && bank != nullptr && !bank->s.empty()) {
        const int n = (int)bank->s.size();
        const int si = ((prm.smpIdx[r] % n) + n) % n;
        const Sample& s = bank->s[(size_t)si];
        if (s.data && s.data->size() > 2) {
            double pitch = pm;
            if (r >= 4) {
                if (midiNote >= 0) pitch *= std::pow(2.0, (midiNote - 60) / 12.0);
                else pitch *= std::pow(2.0, (note(r, st, r == 4 ? 48 : 60) - 60) / 12.0);
            }
            float g, sf; bool rv;
            getSmpSet(si, g, sf, rv);
            push(mkSample(s, pitch, .8 * vol, g, sf, rv), at);
            return;
        }
    }
    const Snd& k = sounds(r)[(size_t)SV[r]];
    switch (r) {
        case 0:
            push(mkOsc(k.wave, k.p[0] * pm, k.p[1] * pm, k.p[5], k.p[2], vol), at);
            push(mkNoise(0, k.p[4], 1.0, .012, k.p[3] * vol), at);
            break;
        case 1:
            if (k.p[0] > 0) push(mkOsc(k.wave, k.p[0], k.p[1], .05, k.p[4], k.p[5] * vol), at);
            push(mkNoise(0, k.p[2], .7, k.p[3], .7 * vol), at);
            break;
        case 2:
            push(mkNoise(k.wave, k.p[0], k.p[1], k.p[2], .4 * vol), at);
            break;
        case 3: {
            int cnt = (int)k.p[3];
            for (int n = 0; n < cnt; ++n) push(mkNoise(1, k.p[0], k.p[1], .02, .8 * vol), at + (int64_t)(n * .011 * sr));
            push(mkNoise(1, k.p[0], k.p[1], k.p[2], .8 * vol), at + (int64_t)(cnt * .011 * sr));
            break;
        }
        case 4: {
            double f = 440.0 * std::pow(2.0, ((midiNote >= 0 ? midiNote : note(4, st, 36)) - 69) / 12.0) * pm;
            push(mkBass(k.wave, k.p[4], f, k.p[0], k.p[1], k.p[2], k.p[3], k.p[5] * vol), at);
            break;
        }
        default: {
            double f = 440.0 * std::pow(2.0, ((midiNote >= 0 ? midiNote : note(5, st, 60)) - 69) / 12.0) * pm;
            push(mkFm(k.wave, f, k.p[0], k.p[1], k.p[2], vol), at);
            break;
        }
    }
}

void Engine::dive(double sdS, double amt) {
    diveAmt = diveOn ? std::max(diveAmt, amt) : amt;
    diveOn = true; diveAge = 0; diveLen = std::max(1.0, .95 * sdS);
}

void Engine::rep(double sdS, int div, double amt) {
    fzAmt = fzOn ? std::max(fzAmt, amt) : amt;
    fzL = clampI((int)(sdS / div), 1, fz.size() - 1);
    fzEnd = (int)sdS;
    fzAge = 0;
    fzOn = true;
}

static inline float lpHz(double a) { return (float)(20000.0 * std::pow(500.0 / 20000.0, a)); }
static inline float hpHz(double a) { return (float)(20.0 * std::pow(1800.0 / 20.0, a)); }

void Engine::glitch(int ty, double sdS) {
    static const double RO[5] = {55, 110, 330, 880, 1500};
    static const int FXOF[16] = {-1, FX_F2, FX_DE, FX_DV, FX_KL, FX_RG, FX_DS, FX_LP, FX_HP, FX_DL, FX_PP, FX_RV, FX_PD, FX_PU, FX_HF, FX_DR};
    if (ty < 1 || ty > 15 || A(FXOF[ty]) <= 0.0) return;  // intensité 0 : l'effet ne fait rien
    auto mx = [](float& dst, double v) { dst = std::max(dst, (float)v); };
    switch (ty) {
        case 1: rep(sdS, 2, A(FX_F2)); break;
        case 2: dive(sdS, A(FX_DE)); mx(ov.dl, .9 * A(FX_DE)); mx(ov.pp, .9 * A(FX_DE)); break;
        case 3: dive(sdS, A(FX_DV)); break;
        case 4: mx(ov.kill, A(FX_KL)); break;
        case 5: roF = RO[rng.below(5)]; rrOn = false; mx(ov.ring, A(FX_RG)); break;
        case 6: mx(ov.dist, A(FX_DS)); break;
        case 7: ov.lp = lpHz(A(FX_LP)); break;
        case 8: ov.hp = hpHz(A(FX_HP)); break;
        case 9: mx(ov.dl, .9 * A(FX_DL)); break;
        case 10: mx(ov.pp, .9 * A(FX_PP)); break;
        case 11: mx(ov.rv, .8 * A(FX_RV)); break;
        case 15: dive(sdS, A(FX_DR)); rrOn = true; mx(ov.ring, A(FX_DR)); break;
        default: break;
    }
}

double Engine::doStep(int st) {
    if (st == loStep() && lastStep == hiStep() && prm.autoRnd) { rndG(rng, prm.dens); rnd2(rng); }
    lastStep = st;
    curStep.store(st);
    const int g0 = GL[st];
    const double tm = (prm.fx >> FX_HF & 1) ? 1.0 - .5 * A(FX_HF) : 1.0;
    const double sd = 15.0 / ebpm / (tm * (g0 == 14 ? 1.0 - .5 * A(FX_HF) : 1.0));
    const double sdS = sd * sr;
    const double j = ((st & 1) ? prm.sw * sd * .4 : 0.0) + (rng() - .5) * prm.chaos * .012;
    const double pmBase = (prm.fx >> FX_PD & 1) ? std::pow(2.0, -A(FX_PD)) : ((prm.fx >> FX_PU & 1) ? std::pow(2.0, A(FX_PU)) : 1.0);
    const double pm = pmBase * (g0 == 12 ? std::pow(2.0, -A(FX_PD)) : g0 == 13 ? std::pow(2.0, A(FX_PU)) : 1.0);
    for (int r = 0; r < 6; ++r) {
        int x = P[r][st];
        if (!x || !audible(r)) continue;
        int n = x == 1 ? 1 : x == 2 ? 2 : 4;
        for (int k = 0; k < n; ++k) {
            double off = std::max(0.0, j + k * sd / n);
            trigTrack(r, st, 1.0 - k * .08, now + (int64_t)(off * sr), (r == 0 || r >= 4) ? pm : 1.0);
        }
    }
    resetOv();
    if (g0) glitch(g0, sdS);
    else if (rng() < prm.chaos * .3) glitch(1 + rng.below(11), sdS);
    auto held = [&](int fx) { return (prm.fx >> fx & 1) && A(fx) > 0.0; };
    auto mx = [](float& dst, double v) { dst = std::max(dst, (float)v); };
    if (held(FX_F2)) rep(sdS, 2, A(FX_F2));
    if (held(FX_DV)) dive(sdS, A(FX_DV));
    if (held(FX_DE)) { dive(sdS, A(FX_DE)); mx(ov.dl, .9 * A(FX_DE)); mx(ov.pp, .9 * A(FX_DE)); }
    if (held(FX_DR)) { dive(sdS, A(FX_DR)); rrOn = true; mx(ov.ring, A(FX_DR)); }
    if (held(FX_KL)) mx(ov.kill, A(FX_KL));
    return sdS;
}

void Engine::process(float* L, float* R, int n, const Params& p, const Transport& t,
                     const MidiEv* ev, int nev, const Aux* aux) {
    prm = p;
    ebpm = (t.mode == 2 && t.bpm > 20.0) ? t.bpm : std::max(20.0, (double)p.bpm);
    for (int r = 0; r < 6; ++r) SV[r] = clampI(p.snd[r], 0, soundCount(r) - 1);
    if (hasPending.exchange(false)) {
        for (auto& v : voices) if (v.active && v.kind == 4) v.active = false;
        for (int q = 0; q < npend;) {
            if (pend[q].v.kind == 4) pend[q] = pend[--npend];
            else ++q;
        }
        bank = pendingBank.load();
        activeId.store(bank ? bank->id : -1);
    }
    if (t.mode != mode) enterMode(t.mode);
    running.store(mode != 0);

    const int dlS = clampI((int)(45.0 / ebpm * sr), 1, dl.size() - 1);
    const int ppS = clampI((int)(30.0 / ebpm * sr), 1, pa1.size() - 1);
    const float aMix = 1.f - (float)std::exp(-1.0 / (.003 * sr));
    const float aFrq = 1.f - (float)std::exp(-1.0 / (.01 * sr));
    const double cA = std::exp(-1.0 / (.003 * sr)), cR = std::exp(-1.0 / (.12 * sr));
    const double aVol = 1.0 - std::exp(-1.0 / (.005 * sr));
    int mi = 0;

    int pv = previewMask.exchange(0);
    for (int r = 0; r < 6; ++r)
        if (pv >> r & 1) trigTrack(r, 0, 1.0, now, 1.0);

    for (int i = 0; i < n; ++i) {
        if (mode == 1) {
            if (samplesToNext <= 0) {
                const int lo = loStep(), hi = hiStep();
                if (step < lo || step > hi) step = lo;
                samplesToNext += doStep(step);
                step = (step >= hi) ? lo : step + 1;
            }
            samplesToNext -= 1.0;
        } else if (mode == 2) {
            double pq = t.ppq + i * ebpm / (60.0 * sr);
            long long si = (long long)std::floor(pq * 4.0 + 1e-6);
            if (si != lastHostStep) {
                lastHostStep = si;
                const int lo = loStep(), len = hiStep() - lo + 1;
                doStep(lo + (int)(((si % len) + len) % len));
            }
        }

        while (mi < nev && ev[mi].offset <= i) {
            const MidiEv& m = ev[mi++];
            if (m.track >= 0 && m.track < 6 && audible(m.track))
                trigTrack(m.track, 0, std::max(0.f, std::min(1.f, m.vel)), now, 1.0, m.note);
        }

        for (int q = 0; q < npend;) {
            if (pend[q].at <= now) { activate(pend[q].v); pend[q] = pend[--npend]; }
            else ++q;
        }

        double tl[6] = {0, 0, 0, 0, 0, 0}, tr[6] = {0, 0, 0, 0, 0, 0};
        for (auto& v : voices)
            if (v.active) {
                const double l = v.tick(sr, rng);
                tl[v.trk] += l;
                tr[v.trk] += v.rOut;
            }
        double inL = 0.0, inR = 0.0;
        for (int k = 0; k < 6; ++k) {
            volS[k] += ((double)prm.vol[k] - volS[k]) * aVol;
            panS[k] += ((double)prm.pan[k] - panS[k]) * aVol;
            const double pn = panS[k];
            const double gl = pn > 0.0 ? 1.0 - pn : 1.0, gr = pn < 0.0 ? 1.0 + pn : 1.0;
            const double vl = tl[k] * volS[k] * gl, vr = tr[k] * volS[k] * gr;
            if (aux != nullptr && aux->l[k] != nullptr && aux->r[k] != nullptr) {
                aux->l[k][i] = (float)vl;
                aux->r[k][i] = (float)vr;
            } else {
                inL += vl;
                inR += vr;
            }
        }

        // cibles
        const uint32_t fx = prm.fx;
        float tRing = ov.ring >= 0 ? ov.ring : ((fx >> FX_RG & 1) ? (float)A(FX_RG) : 0.f);
        float tDist = ov.dist >= 0 ? ov.dist : ((fx >> FX_DS & 1) ? (float)A(FX_DS) : 0.f);
        float tLp = ov.lp >= 0 ? ov.lp : ((fx >> FX_LP & 1) ? lpHz(A(FX_LP)) : 20000.f);
        float tHp = ov.hp >= 0 ? ov.hp : ((fx >> FX_HP & 1) ? hpHz(A(FX_HP)) : 20.f);
        float tDl = ov.dl >= 0 ? ov.dl : ((fx >> FX_DL & 1) ? (float)(.8 * A(FX_DL)) : 0.f);
        float tPp = ov.pp >= 0 ? ov.pp : ((fx >> FX_PP & 1) ? (float)(.9 * A(FX_PP)) : 0.f);
        float tRv = ov.rv >= 0 ? ov.rv : ((fx >> FX_RV & 1) ? (float)(.7 * A(FX_RV)) : 0.f);
        ringV += (tRing - ringV) * aMix; distV += (tDist - distV) * aMix;
        dlV += (tDl - dlV) * aMix; ppV += (tPp - ppV) * aMix; rvV += (tRv - rvV) * aMix;
        lpV += (tLp - lpV) * aFrq; hpV += (tHp - hpV) * aFrq;

        double gt = 1.0 - (double)ov.kill, lpEff = lpV, roEff = roF;
        if (diveOn) {
            double a = std::min(1.0, diveAge / diveLen);
            gt *= (1.0 - a * diveAmt);
            const double cut = 8000.0 * std::pow(60.0 / 8000.0, a);
            lpEff = lpV * std::pow(cut / lpV, diveAmt);
            if (rrOn) roEff = 1500.0 * std::pow(40.0 / 1500.0, a);
            diveAge += 1.0;
        }
        if ((now & 15) == 0) {
            hpF.set(Biquad::HP, hpV, 1.122, sr);
            lpF.set(Biquad::LP, lpEff, 1.122, sr);
            hpR.set(Biquad::HP, hpV, 1.122, sr);
            lpR.set(Biquad::LP, lpEff, 1.122, sr);
        }

        double xl = inL * gt, xr = inR * gt;
        roPh += roEff / sr; roPh -= std::floor(roPh);
        const double rsn = std::sin(TWO_PI * roPh);
        xl = xl * (1.0 - ringV * .85) + xl * rsn * ringV;
        xr = xr * (1.0 - ringV * .85) + xr * rsn * ringV;
        const double cxl = std::max(-1.0, std::min(1.0, xl)), cxr = std::max(-1.0, std::min(1.0, xr));
        xl = xl * (1.0 - distV * .85) + std::tanh(14.0 * cxl) * distV;
        xr = xr * (1.0 - distV * .85) + std::tanh(14.0 * cxr) * distV;
        xl = lpF.process(hpF.process(xl));
        xr = lpR.process(hpR.process(xr));
        const float outL = (float)xl, outR = (float)xr;

        float fzoL = 0.f, fzoR = 0.f;
        if (fzOn) {
            const float dL = fz.read(fzL), dR = fzR.read(fzL);
            const bool loop = fzAge >= fzL;
            fz.write((loop ? 0.f : outL) + dL * (loop ? .97f : 0.f));
            fzR.write((loop ? 0.f : outR) + dR * (loop ? .97f : 0.f));
            if (loop) { fzoL = dL * (float)fzAmt; fzoR = dR * (float)fzAmt; }
            if (++fzAge >= fzEnd) fzOn = false;
        } else { fz.write(0.f); fzR.write(0.f); }

        const float dyL = dl.read(dlS), dyR = dlR.read(dlS);
        dl.write(outL * (float)dlV + dyL * .5f);
        dlR.write(outR * (float)dlV + dyR * .5f);
        const float mid = .5f * (outL + outR);
        const float y1 = pa1.read(ppS), y2 = pa2.read(ppS);
        pa1.write(mid * (float)ppV + .6f * y2);
        pa2.write(.6f * y1);
        const float rvL = rev.process(outL * (float)rvV), rvR = revR.process(outR * (float)rvV);

        double bl = outL + fzoL + dyL + y1 + rvL, br = outR + fzoR + dyR + y2 + rvR;

        // compresseur (approximation du DynamicsCompressor Web Audio)
        double lvl = std::max(std::fabs(bl), std::fabs(br));
        double db = 20.0 * std::log10(std::max(lvl, 1e-6)), over = db + 14.0, odb;
        const double K = 30.0, Rr = 6.0;
        if (2.0 * over < -K) odb = db;
        else if (2.0 * std::fabs(over) <= K) odb = db + (1.0 / Rr - 1.0) * (over + K / 2) * (over + K / 2) / (2.0 * K);
        else odb = -14.0 + over / Rr;
        double gr = odb - db, c = gr < cgr ? cA : cR;
        cgr = c * cgr + (1.0 - c) * gr;
        double gain = std::pow(10.0, cgr / 20.0) * 1.5 * .8;
        auto sc = [](double v) {
            double a = std::fabs(v);
            if (a < .8) return v;
            double o = .8 + .2 * std::tanh((a - .8) / .2);
            return v < 0 ? -o : o;
        };
        L[i] = (float)sc(bl * gain);
        R[i] = (float)sc(br * gain);
        ++now;
    }
}

// ---------------------------------------------------------------- aléa
static const double KP[16] = {.9, .05, .1, .2, .1, .05, .3, .1, .05, .2, .5, .1, .05, .1, .3, .15};
static const double SP[16] = {0, 0, .08, 0, .9, 0, .1, .3, 0, .12, .1, 0, .9, .1, .2, .5};

void Engine::rndN(Rng& r) {
    for (int i = 0; i < N; ++i) {
        bool c = i >= 128 && r() < .8;
        NT[4][i] = c ? NT[4][i - 128] : (uint8_t)(r() < .4 ? 0 : r.below(7));
        NT[5][i] = c ? NT[5][i - 128] : (uint8_t)r.below(12);
    }
}

void Engine::rnd2(Rng& r) {
    static const int RTv[3] = {2, 2, 3};
    auto rt = [&]() { return r() < .18 ? RTv[r.below(3)] : 1; };
    auto gen = [&](int tr, int k, double f) -> int {
        auto y = [&](double p) { return r() < p * f; };
        switch (tr) {
            case 0: return y(KP[k]) ? 1 : 0;
            case 1: return y(SP[k]) ? (r() < .2 ? rt() : 1) : 0;
            case 2: return y(k % 2 ? .45 : .8) ? rt() : 0;
            case 3: return y(k == 8 ? .2 : .12) ? 1 : 0;
            case 4: return y(k % 4 == 0 ? .55 : .25) ? 1 : 0;
            default: return y(.22) ? 1 : 0;
        }
    };
    for (int b = 0; b < 64; ++b)
        for (int k = 0; k < 16; ++k) {
            int i = b * 16 + k;
            for (int t = 0; t < 6; ++t)
                P[t][i] = (b >= 8 && r() < .85) ? P[t][i - 128] : (uint8_t)gen(t, k, b % 4 == 3 ? 1.6 : 1.0);
        }
}

void Engine::rndG(Rng& r, double dens) {
    std::fill(GL, GL + N, (uint8_t)0);
    static const int NL[5] = {1, 1, 2, 3, 4};
    for (int i = 0; i < N;) {
        int k = i & 15;
        double p = dens * (k > 11 ? 1.8 : (k % 2 ? 1.0 : .6));
        if (r() < p * .5) {
            int ty = 1 + r.below(15), n = NL[r.below(5)];
            for (int j = 0; j < n && i + j < N; ++j) GL[i + j] = (uint8_t)ty;
            i += n;
        } else ++i;
    }
}

void Engine::mutate(Rng& r) {
    static const int PV[4] = {1, 1, 2, 3};
    for (int n = 0; n < 80; ++n) {
        int t = r.below(6), i = r.below(N);
        P[t][i] = (uint8_t)(r() < .45 ? 0 : PV[r.below(4)]);
        if (t > 3) NT[t][i] = (uint8_t)r.below(10);
    }
    for (int n = 0; n < 40; ++n) GL[r.below(N)] = (uint8_t)(r() < .4 ? 0 : 1 + r.below(15));
}

void Engine::clearAll() {
    for (auto& r : P) std::fill(r, r + N, (uint8_t)0);
    std::fill(GL, GL + N, (uint8_t)0);
}

}  // namespace gltch
