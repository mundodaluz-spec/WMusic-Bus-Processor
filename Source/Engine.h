// WMusic Bus Processor - motor de audio (C++17 puro, sem dependencia de JUCE).
// Ordem do sinal: [saturador PRE] -> compressor -> [saturador POS] -> espacial -> saida.
// Nada aloca memoria no processamento. Latencia: zero.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

namespace wm
{
constexpr double kPi = 3.14159265358979323846;
constexpr float kRangeFree = 23.95f;   // RANGE no maximo = sem limite de reducao
constexpr float kFilterOff = 20.5f;    // filtros em 20 Hz = desligados

inline float dbToGain (float db) noexcept   { return std::exp (db * 0.11512925464970229f); }
inline float gainToDb (float g) noexcept    { return 20.0f * std::log10 (std::max (g, 1.0e-9f)); }

//==============================================================================
struct Params
{
    // Compressor
    bool  compOn    = true;
    float threshDb  = 0.0f;     // -50..0 dBFS
    float makeupDb  = 0.0f;     // 0..20 dB
    float attackMs  = 10.0f;    // 0.1..30 ms
    float releaseMs = 300.0f;   // 100..1200 ms (ou o valor vindo do tempo da DAW)
    bool  autoRel   = false;
    float ratio     = 4.0f;     // 1.3..10
    float knee      = 0.3f;     // 0 (duro) .. 1 (suave, 12 dB)
    float rangeDb   = 24.0f;    // 0..24 dB; >= kRangeFree = livre
    float compMix   = 100.0f;   // %

    // Sidechain
    float link      = 100.0f;   // 0 (dual mono) .. 100 (casado)
    float scHpfHz   = 20.0f;    // 20 = desligado .. 500 Hz
    float scTilt    = 0.0f;     // -1 (ouve mais grave) .. +1 (ouve mais agudo)

    // Saturador
    bool  satOn     = true;
    float satAmount = 0.0f;     // 0..10
    float satMix    = 100.0f;   // %
    float satTone   = 0.0f;     // -1 (satura mais o grave) .. +1 (satura mais o agudo)
    bool  satTom    = true;     // harmonicos pares (calor)
    bool  satHarm   = false;    // curva dura: limpa embaixo, corta seco no topo
    bool  satPre    = false;    // antes do compressor

    // Espacial
    bool  spatOn    = true;
    float air       = 0.0f;     // 0..100 %
    float width     = 100.0f;   // 0 (mono) .. 100 (original) .. 200 (so lados)
    float monoBassHz = 20.0f;   // 20 = desligado .. 500 Hz

    float outDb     = 0.0f;     // -60 (mudo) .. +12 dB
};

//==============================================================================
// Suavizador exponencial que chega exatamente no alvo.
struct Smooth
{
    float z = 0.0f, a = 0.0f;
    void setup (double fs, double ms) noexcept   { a = (float) std::exp (-1.0 / (0.001 * ms * fs)); }
    void snap (float v) noexcept                 { z = v; }
    float next (float target) noexcept
    {
        z = target + a * (z - target);
        if (std::abs (z - target) < 1.0e-6f) z = target;
        return z;
    }
};

// Rampa linear 0..1 para ligar/desligar secoes sem clique.
struct Fade
{
    float g = 0.0f, step = 0.001f;
    void setup (double fs, double ms) noexcept   { step = (float) (1.0 / (0.001 * ms * fs)); }
    void snap (bool on) noexcept                 { g = on ? 1.0f : 0.0f; }
    float next (bool on) noexcept
    {
        g = on ? std::min (1.0f, g + step) : std::max (0.0f, g - step);
        return g;
    }
};

//==============================================================================
// Filtro de 1a ordem generico.
struct FirstOrder
{
    float b0 = 1.0f, b1 = 0.0f, a1 = 0.0f, x1 = 0.0f, y1 = 0.0f;

    void reset() noexcept { x1 = y1 = 0.0f; }

    float tick (float x) noexcept
    {
        const float y = b0 * x + b1 * x1 - a1 * y1;
        x1 = x; y1 = y;
        return y;
    }

    void setHighPass (double fs, double f) noexcept
    {
        const double k = std::tan (kPi * std::min (f, fs * 0.45) / fs);
        b0 = (float) (1.0 / (1.0 + k)); b1 = -b0; a1 = (float) ((k - 1.0) / (k + 1.0));
    }

    // Tilt: graves x 1/G, agudos x G, girando em f0.
    void setTilt (double fs, double f0, double G) noexcept
    {
        const double k = std::tan (kPi * std::min (f0, fs * 0.45) / fs);
        b0 = (float) ((G + k / G) / (1.0 + k));
        b1 = (float) ((k / G - G) / (1.0 + k));
        a1 = (float) ((k - 1.0) / (1.0 + k));
    }

    // Inverso exato do tilt acima (a soma dos dois e plana).
    void setTiltInverse (double fs, double f0, double G) noexcept
    {
        const double k = std::tan (kPi * std::min (f0, fs * 0.45) / fs);
        const double B0 = (G + k / G) / (1.0 + k), B1 = (k / G - G) / (1.0 + k), A1 = (k - 1.0) / (1.0 + k);
        b0 = (float) (1.0 / B0); b1 = (float) (A1 / B0); a1 = (float) (B1 / B0);
    }
};

// Biquad (forma transposta II).
struct Biquad
{
    float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f, a1 = 0.0f, a2 = 0.0f, s1 = 0.0f, s2 = 0.0f;

    void reset() noexcept { s1 = s2 = 0.0f; }

    float tick (float x) noexcept
    {
        const float y = b0 * x + s1;
        s1 = b1 * x - a1 * y + s2;
        s2 = b2 * x - a2 * y;
        return y;
    }

    void setHighPass (double fs, double f, double q = 0.70710678) noexcept
    {
        const double w = 2.0 * kPi * std::min (f, fs * 0.45) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
        const double a0 = 1.0 + al;
        b0 = (float) ((1.0 + c) * 0.5 / a0); b1 = (float) (-(1.0 + c) / a0); b2 = b0;
        a1 = (float) (-2.0 * c / a0); a2 = (float) ((1.0 - al) / a0);
    }

    void setHighShelf (double fs, double f, double gainDb) noexcept
    {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * kPi * std::min (f, fs * 0.45) / fs, c = std::cos (w), s = std::sin (w);
        const double al = s * 0.5 * std::sqrt (2.0), sq = 2.0 * std::sqrt (A) * al;
        const double a0 = (A + 1.0) - (A - 1.0) * c + sq;
        b0 = (float) (A * ((A + 1.0) + (A - 1.0) * c + sq) / a0);
        b1 = (float) (-2.0 * A * ((A - 1.0) + (A + 1.0) * c) / a0);
        b2 = (float) (A * ((A + 1.0) + (A - 1.0) * c - sq) / a0);
        a1 = (float) (2.0 * ((A - 1.0) - (A + 1.0) * c) / a0);
        a2 = (float) (((A + 1.0) - (A - 1.0) * c - sq) / a0);
    }
};

//==============================================================================
// Oversampling 2x com meia-banda IIR polifasico (passa-tudo), sem latencia informada.
struct Halfband2x
{
    struct AP
    {
        float a = 0.0f, x1 = 0.0f, y1 = 0.0f;
        float tick (float x) noexcept { const float y = a * (x - y1) + x1; x1 = x; y1 = y; return y; }
    };

    AP upA[4], upB[4], dnA[4], dnB[4];

    Halfband2x() noexcept
    {
        static constexpr float ca[4] = { 0.07711507983241622f, 0.4820706250610472f, 0.7968204713315797f, 0.9412514277740471f };
        static constexpr float cb[4] = { 0.2659685265210946f, 0.6651041532634957f, 0.8841015085506159f, 0.9820054141886075f };
        for (int i = 0; i < 4; ++i) { upA[i].a = dnA[i].a = ca[i]; upB[i].a = dnB[i].a = cb[i]; }
    }

    void reset() noexcept
    {
        for (int i = 0; i < 4; ++i)
            upA[i].x1 = upA[i].y1 = upB[i].x1 = upB[i].y1 = dnA[i].x1 = dnA[i].y1 = dnB[i].x1 = dnB[i].y1 = 0.0f;
    }

    void up (float x, float& o0, float& o1) noexcept
    {
        float a = x, b = x;
        for (int i = 0; i < 4; ++i) { a = upA[i].tick (a); b = upB[i].tick (b); }
        o0 = a; o1 = b;
    }

    float down (float s0, float s1) noexcept
    {
        float a = s1, b = s0;
        for (int i = 0; i < 4; ++i) { a = dnA[i].tick (a); b = dnB[i].tick (b); }
        return 0.5f * (a + b);
    }
};

//==============================================================================
class Engine
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        for (auto* s : { &sThresh, &sMakeup, &sCompMix, &sSatMix, &sDrive, &sWidth, &sOut })
            s->setup (fs, 20.0);
        for (auto* f : { &fComp, &fSat, &fSpat })
            f->setup (fs, 20.0);

        holdSamples = (int) (0.008 * fs);
        holdDecay   = (float) std::exp (-1.0 / (0.002 * fs));
        autoFastRel = (float) std::exp (-1.0 / (0.080 * fs));
        autoSlowAtk = (float) std::exp (-1.0 / (0.400 * fs));
        autoSlowRel = (float) std::exp (-1.0 / (2.000 * fs));
        reset();
        first = true;
    }

    void reset() noexcept
    {
        for (int c = 0; c < 2; ++c)
        {
            scHp[c].reset(); scTilt[c].reset(); satPreF[c].reset(); satPostF[c].reset(); satDc[c].reset();
            os[c].reset(); air[c].reset();
            gr[c] = grFast[c] = grSlow[c] = 0.0f; holdVal[c] = 0.0f; holdCnt[c] = 0;
        }
        sideHp.reset();
        satIdle = spatIdle = true;
    }

    void setParams (const Params& np) noexcept
    {
        p = np;
        p.ratio = std::max (1.0f, p.ratio);

        atkCoef = (float) std::exp (-1.0 / (0.001 * std::max (0.02f, p.attackMs) * fs));
        relCoef = (float) std::exp (-1.0 / (0.001 * std::max (5.0f, p.releaseMs) * fs));
        slope   = 1.0f - 1.0f / p.ratio;
        kneeW   = std::clamp (p.knee, 0.0f, 1.0f) * 12.0f;
        rangeLim = p.rangeDb >= kRangeFree ? 1000.0f : std::max (0.0f, p.rangeDb);

        scHpOn = p.scHpfHz > kFilterOff;
        const double scG = std::pow (10.0, std::clamp (p.scTilt, -1.0f, 1.0f) * 6.0 / 20.0);
        for (int c = 0; c < 2; ++c)
        {
            scHp[c].setHighPass (fs, scHpOn ? p.scHpfHz : 20.0);
            scTilt[c].setTilt (fs, 1000.0, scG);
        }
        scTiltOn = std::abs (p.scTilt) > 0.001f;

        // Saturador (roda em 2x)
        const double fs2 = fs * 2.0;
        const double satG = std::pow (10.0, std::clamp (p.satTone, -1.0f, 1.0f) * 9.0 / 20.0);
        for (int c = 0; c < 2; ++c)
        {
            satPreF[c].setTilt (fs2, 800.0, satG);
            satPostF[c].setTiltInverse (fs2, 800.0, satG);
            satDc[c].setHighPass (fs2, 5.0);
        }
        satToneOn = std::abs (p.satTone) > 0.001f;
        const float amt = std::clamp (p.satAmount, 0.0f, 10.0f);
        driveTarget = dbToGain (amt * 1.8f);                       // ate +18 dB de drive
        satBlend    = std::min (1.0f, amt / 0.5f) * std::clamp (p.satMix, 0.0f, 100.0f) * 0.01f;
        bias        = p.satTom ? 0.30f : 0.0f;
        biasOut     = core (bias, p.satHarm);
        biasNorm    = 1.0f / coreSlope (bias, p.satHarm);

        // Espacial
        airOn = p.air > 0.05f;
        for (int c = 0; c < 2; ++c)
            air[c].setHighShelf (fs, 10000.0, airOn ? p.air * 0.08 : 0.0);   // ate +8 dB
        monoBassOn = p.monoBassHz > kFilterOff;
        sideHp.setHighPass (fs, monoBassOn ? p.monoBassHz : 20.0);

        outTarget = p.outDb <= -59.9f ? 0.0f : dbToGain (p.outDb);

        if (first)
        {
            first = false;
            sThresh.snap (p.threshDb); sMakeup.snap (p.makeupDb); sCompMix.snap (p.compMix * 0.01f);
            sSatMix.snap (satBlend); sDrive.snap (driveTarget); sWidth.snap (p.width * 0.01f); sOut.snap (outTarget);
            fComp.snap (p.compOn); fSat.snap (p.satOn && satBlend > 0.0f); fSpat.snap (p.spatOn);
        }
    }

    // io: canais de audio (1 ou 2). sc: sidechain externo com o mesmo numero de canais, ou nullptr.
    void process (float* const* io, int numCh, const float* const* sc, int numSamples) noexcept
    {
        numCh = std::clamp (numCh, 1, 2);
        float grMax = 0.0f, drvMax = 0.0f;

        for (int i = 0; i < numSamples; ++i)
        {
            float x[2] = { io[0][i], numCh > 1 ? io[1][i] : 0.0f };

            // O saturador entra e sai por crossfade (o caminho em 2x tem fase propria).
            const float gSat = fSat.next (p.satOn && satBlend > 0.0f);
            const float satMixNow = sSatMix.next (satBlend);
            const float drive = sDrive.next (driveTarget);
            const bool  satRun = gSat > 0.0f;
            if (! satRun) satIdle = true;
            else if (satIdle) { satIdle = false; for (int c = 0; c < 2; ++c) { os[c].reset(); satPreF[c].reset(); satPostF[c].reset(); satDc[c].reset(); } }

            if (satRun && p.satPre)
                for (int c = 0; c < numCh; ++c) x[c] += gSat * (saturate (c, x[c], drive, satMixNow, drvMax) - x[c]);

            //------------------------------------------------------------ compressor
            const float gComp = fComp.next (p.compOn);
            const float thr = sThresh.next (p.threshDb), mk = sMakeup.next (p.makeupDb), cmix = sCompMix.next (p.compMix * 0.01f);
            if (gComp > 0.0f)
            {
                float lv[2] = { 0.0f, 0.0f };
                for (int c = 0; c < numCh; ++c)
                {
                    float d = sc != nullptr ? sc[c][i] : x[c];
                    const float h = scHp[c].tick (d);    if (scHpOn) d = h;
                    const float t = scTilt[c].tick (d);  if (scTiltOn) d = t;
                    lv[c] = std::abs (d);
                }
                const float lmax = numCh > 1 ? std::max (lv[0], lv[1]) : lv[0];
                const float lk = std::clamp (p.link, 0.0f, 100.0f) * 0.01f;

                for (int c = 0; c < numCh; ++c)
                {
                    const float l = lv[c] + lk * (lmax - lv[c]);

                    // retencao de pico (evita ondulacao em graves)
                    if (l >= holdVal[c]) { holdVal[c] = l; holdCnt[c] = holdSamples; }
                    else if (holdCnt[c] > 0) --holdCnt[c];
                    else holdVal[c] = l + holdDecay * (holdVal[c] - l);

                    const float over = gainToDb (holdVal[c]) - thr;
                    float o;
                    if (kneeW > 0.0f && std::abs (over) < kneeW * 0.5f) { const float k = over + kneeW * 0.5f; o = k * k / (2.0f * kneeW); }
                    else o = std::max (0.0f, over);
                    const float target = std::min (o * slope, rangeLim);

                    if (p.autoRel)
                    {
                        grFast[c] = target + (target > grFast[c] ? atkCoef : autoFastRel) * (grFast[c] - target);
                        grSlow[c] = target + (target > grSlow[c] ? autoSlowAtk : autoSlowRel) * (grSlow[c] - target);
                        gr[c] = std::max (grFast[c], grSlow[c]);
                    }
                    else
                    {
                        gr[c] = target + (target > gr[c] ? atkCoef : relCoef) * (gr[c] - target);
                        grFast[c] = grSlow[c] = gr[c];
                    }
                    if (gr[c] < 1.0e-5f) gr[c] = 0.0f;
                    grMax = std::max (grMax, gr[c]);

                    const float wet = x[c] * dbToGain (mk - gr[c]);
                    const float y = x[c] + cmix * (wet - x[c]);
                    x[c] += gComp * (y - x[c]);
                }
            }
            else
            {
                for (int c = 0; c < 2; ++c) { gr[c] = grFast[c] = grSlow[c] = 0.0f; holdVal[c] = 0.0f; }
            }

            if (satRun && ! p.satPre)
                for (int c = 0; c < numCh; ++c) x[c] += gSat * (saturate (c, x[c], drive, satMixNow, drvMax) - x[c]);

            //------------------------------------------------------------ espacial
            const float gSp = fSpat.next (p.spatOn);
            const float w = sWidth.next (p.width * 0.01f);
            if (gSp <= 0.0f) spatIdle = true;
            else
            {
                if (spatIdle) { spatIdle = false; air[0].reset(); air[1].reset(); sideHp.reset(); }
                float y[2] = { x[0], x[1] };
                if (numCh > 1)
                {
                    float m = 0.5f * (y[0] + y[1]), s = 0.5f * (y[0] - y[1]);
                    const float sh = sideHp.tick (s);
                    if (monoBassOn) s = sh;
                    if (w <= 1.0f) s *= w; else m *= (2.0f - w);
                    y[0] = m + s; y[1] = m - s;
                }
                for (int c = 0; c < numCh; ++c)
                {
                    const float a = air[c].tick (y[c]);
                    if (airOn) y[c] = a;
                    x[c] += gSp * (y[c] - x[c]);
                }
            }

            const float og = sOut.next (outTarget);
            io[0][i] = x[0] * og;
            if (numCh > 1) io[1][i] = x[1] * og;
        }

        grMeter.store (grMax, std::memory_order_relaxed);
        driveMeter.store (gainToDb (drvMax), std::memory_order_relaxed);
    }

    std::atomic<float> grMeter { 0.0f };       // dB de reducao (positivo)
    std::atomic<float> driveMeter { -120.0f }; // dB na entrada da curva de saturacao

    // Curva estatica do saturador (usada tambem nos testes).
    static float core (float u, bool hard) noexcept
    {
        if (! hard) return std::tanh (u);
        const float c = std::clamp (u, -1.5f, 1.5f);
        return c - (4.0f / 27.0f) * c * c * c;
    }

private:
    static float coreSlope (float u, bool hard) noexcept
    {
        if (! hard) { const float t = std::tanh (u); return 1.0f - t * t; }
        return 1.0f - (4.0f / 9.0f) * u * u;
    }

    float shape (float v, float drive) const noexcept
    {
        // ganho pequeno-sinal = raiz do drive: sobe o corpo sem estourar o teto
        return (core (v * drive + bias, p.satHarm) - biasOut) * biasNorm / std::sqrt (drive);
    }

    float saturate (int c, float x, float drive, float mix, float& drvMax) noexcept
    {
        float s[2];
        os[c].up (x, s[0], s[1]);
        for (int k = 0; k < 2; ++k)
        {
            const float dry = s[k];
            float v = dry;
            const float pe = satPreF[c].tick (v);
            if (satToneOn) v = pe;
            drvMax = std::max (drvMax, std::abs (v) * drive);
            v = satDc[c].tick (shape (v, drive));
            const float de = satPostF[c].tick (v);
            if (satToneOn) v = de;
            s[k] = dry + mix * (v - dry);
        }
        return os[c].down (s[0], s[1]);
    }

    double fs = 48000.0;
    Params p;
    bool first = true;

    Smooth sThresh, sMakeup, sCompMix, sSatMix, sDrive, sWidth, sOut;
    Fade fComp, fSat, fSpat;

    // compressor
    FirstOrder scHp[2], scTilt[2];
    bool scHpOn = false, scTiltOn = false;
    float atkCoef = 0.0f, relCoef = 0.0f, slope = 0.75f, kneeW = 0.0f, rangeLim = 1000.0f;
    float autoFastRel = 0.0f, autoSlowAtk = 0.0f, autoSlowRel = 0.0f, holdDecay = 0.0f;
    int holdSamples = 0, holdCnt[2] = { 0, 0 };
    float holdVal[2] = { 0.0f, 0.0f }, gr[2] = { 0.0f, 0.0f }, grFast[2] = { 0.0f, 0.0f }, grSlow[2] = { 0.0f, 0.0f };

    // saturador
    Halfband2x os[2];
    FirstOrder satPreF[2], satPostF[2], satDc[2];
    bool satToneOn = false, satIdle = true;
    float driveTarget = 1.0f, satBlend = 0.0f, bias = 0.0f, biasOut = 0.0f, biasNorm = 1.0f;

    // espacial
    Biquad air[2], sideHp;
    bool airOn = false, monoBassOn = false, spatIdle = true;
    float outTarget = 1.0f;
};
} // namespace wm
