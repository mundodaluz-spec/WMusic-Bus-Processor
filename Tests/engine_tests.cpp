// Testes do motor do WMusic Bus Processor (sem JUCE).
#include "../Source/Engine.h"

#include <chrono>
#include <cstdio>
#include <random>
#include <vector>

using namespace wm;

static int passed = 0, failed = 0;
static void check (bool ok, const char* name, double got = 0.0, const char* unit = "")
{
    std::printf ("%s  %-62s %10.4f %s\n", ok ? "ok  " : "FALHOU", name, got, unit);
    ok ? ++passed : ++failed;
}

using Buf = std::vector<float>;

static Buf sine (double fs, double f, double ampDb, int n, double phase = 0.0)
{
    Buf b ((size_t) n);
    const double a = std::pow (10.0, ampDb / 20.0);
    for (int i = 0; i < n; ++i) b[(size_t) i] = (float) (a * std::sin (2.0 * kPi * f * i / fs + phase));
    return b;
}

// amplitude (dB) de um componente, medida no trecho [from, to)
static double toneDb (const Buf& b, double fs, double f, int from, int to)
{
    double re = 0.0, im = 0.0;
    for (int i = from; i < to; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * kPi * (i - from) / (to - from));
        re += w * b[(size_t) i] * std::cos (2.0 * kPi * f * i / fs);
        im += w * b[(size_t) i] * std::sin (2.0 * kPi * f * i / fs);
    }
    return 20.0 * std::log10 (std::max (1e-12, 4.0 * std::sqrt (re * re + im * im) / (to - from)));
}

static double peakDb (const Buf& b, int from, int to)
{
    float p = 0.0f;
    for (int i = from; i < to; ++i) p = std::max (p, std::abs (b[(size_t) i]));
    return 20.0 * std::log10 (std::max (1e-12f, p));
}

static void run (Engine& e, Buf& l, Buf& r, const Buf* scL = nullptr, const Buf* scR = nullptr, int block = 256)
{
    const int n = (int) l.size();
    for (int pos = 0; pos < n; pos += block)
    {
        const int m = std::min (block, n - pos);
        float* io[2] = { l.data() + pos, r.data() + pos };
        const float* sc[2] = { scL ? scL->data() + pos : nullptr, scR ? scR->data() + pos : nullptr };
        e.process (io, 2, scL ? sc : nullptr, m);
    }
}

static Params compOnly()
{
    Params p; p.satOn = false; p.spatOn = false; p.knee = 0.0f; return p;
}

int main()
{
    const double fs = 48000.0;
    const int N = 96000;

    //================================================================ neutro
    {
        std::mt19937 rng (7); std::uniform_real_distribution<float> d (-0.5f, 0.5f);
        Buf l ((size_t) N), r ((size_t) N); for (int i = 0; i < N; ++i) { l[(size_t) i] = d (rng); r[(size_t) i] = d (rng); }
        Buf l0 = l, r0 = r;
        Params p; p.compOn = p.satOn = p.spatOn = false;
        Engine e; e.prepare (48000.0); e.setParams (p); run (e, l, r);
        check (l == l0 && r == r0, "secoes desligadas: saida identica bit a bit");

        l = l0; r = r0; Params q; Engine e2; e2.prepare (48000.0); e2.setParams (q); run (e2, l, r);
        double md = 0.0; for (int i = 0; i < N; ++i) md = std::max (md, (double) std::abs (l[(size_t) i] - l0[(size_t) i]));
        check (md < 1e-6, "ajuste inicial (tudo ligado, neutro): transparente", 20.0 * std::log10 (md + 1e-12), "dB erro");
    }

    //================================================================ curva estatica
    auto staticGr = [&] (float ratio, float range = 24.0f, float knee = 0.0f, float inDb = -10.0f)
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.ratio = ratio; p.rangeDb = range; p.knee = knee; p.attackMs = 1.0f; p.releaseMs = 100.0f;
        Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 1000.0, inDb, N), r = l; run (e, l, r);
        return (double) inDb - toneDb (l, fs, 1000.0, N / 2, N);
    };
    { double g = staticGr (4.0f);  check (std::abs (g - 15.0) < 0.15, "ratio 4:1, 20 dB acima: reducao 15 dB", g, "dB"); }
    { double g = staticGr (2.0f);  check (std::abs (g - 10.0) < 0.15, "ratio 2:1: reducao 10 dB", g, "dB"); }
    { double g = staticGr (10.0f); check (std::abs (g - 18.0) < 0.15, "ratio 10:1: reducao 18 dB", g, "dB"); }
    { double g = staticGr (1.3f);  check (std::abs (g - 4.615) < 0.15, "ratio 1.3:1: reducao 4.6 dB", g, "dB"); }
    { double g = staticGr (4.0f, 6.0f); check (std::abs (g - 6.0) < 0.1, "RANGE 6 dB limita a reducao", g, "dB"); }
    { double g = staticGr (4.0f, 0.0f); check (std::abs (g) < 0.05, "RANGE 0 dB: nao reduz", g, "dB"); }
    { double g = staticGr (4.0f, 24.0f, 1.0f, -30.0f); check (std::abs (g - 1.125) < 0.15, "joelho suave: 1.1 dB no threshold", g, "dB"); }
    { double g = staticGr (4.0f, 24.0f, 0.0f, -30.5f); check (std::abs (g) < 0.05, "joelho duro: nada abaixo do threshold", g, "dB"); }

    //================================================================ attack / release
    auto timing = [&] (float atk, float rel, bool autoRel, double burstSec, double& atkMs, double& relMs)
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.ratio = 4.0f; p.attackMs = atk; p.releaseMs = rel; p.autoRel = autoRel;
        const int n = (int) (fs * (burstSec + 8.0)), on = (int) (fs * 0.5), off = on + (int) (fs * burstSec);
        Buf l ((size_t) n), r;
        for (int i = 0; i < n; ++i)
            l[(size_t) i] = (float) (((i >= on && i < off) ? 0.316228 : 0.003) * std::sin (2.0 * kPi * 5000.0 * i / fs));
        r = l; Buf in = l;
        Engine e; e.prepare (48000.0); e.setParams (p); run (e, l, r, nullptr, nullptr, 32);
        // ganho por janela curta
        auto gainAt = [&] (int i) { return peakDb (l, i - 10, i + 10) - peakDb (in, i - 10, i + 10); };
        atkMs = relMs = -1.0;
        for (int i = on + 10; i < off; ++i) if (gainAt (i) <= -15.0 * 0.632) { atkMs = 1000.0 * (i - on) / fs; break; }
        for (int i = off + 10; i < n - 10; ++i) if (gainAt (i) >= -15.0 * 0.368) { relMs = 1000.0 * (i - off) / fs; break; }
    };
    {
        double a, r;
        timing (1.0f, 300.0f, false, 1.0, a, r);   check (a > 0.6 && a < 1.6, "attack 1 ms", a, "ms");
        check (r > 280.0 && r < 340.0, "release 300 ms", r, "ms");
        timing (10.0f, 100.0f, false, 1.0, a, r);  check (a > 9.0 && a < 11.5, "attack 10 ms", a, "ms");
        check (r > 95.0 && r < 125.0, "release 100 ms", r, "ms");
        timing (30.0f, 1200.0f, false, 1.0, a, r); check (a > 28.0 && a < 33.0, "attack 30 ms", a, "ms");
        check (r > 1150.0 && r < 1260.0, "release 1.2 s", r, "ms");
        double rShort, rLong;
        timing (1.0f, 300.0f, true, 0.03, a, rShort);
        timing (1.0f, 300.0f, true, 3.0, a, rLong);
        check (rShort > 0 && rShort < 200.0, "AUTO: solta rapido apos pico curto", rShort, "ms");
        check (rLong > 800.0, "AUTO: solta devagar apos compressao longa", rLong, "ms");
    }

    //================================================================ make-up / mix
    {
        Params p = compOnly(); p.makeupDb = 6.0f;
        Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 1000.0, -20.0, N), r = l; run (e, l, r);
        const double g = toneDb (l, fs, 1000.0, N / 2, N) + 20.0;
        check (std::abs (g - 6.0) < 0.01, "make-up +6 dB", g, "dB");
    }
    {
        Params p = compOnly(); p.threshDb = -40.0f; p.makeupDb = 10.0f; p.compMix = 0.0f;
        Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 1000.0, -10.0, N), r = l, in = l; run (e, l, r);
        check (l == in, "mix do compressor 0%: original bit a bit");
        p.compMix = 50.0f; p.makeupDb = 0.0f; p.threshDb = -30.0f; p.ratio = 4.0f; p.attackMs = 1.0f;
        Engine e2; e2.prepare (48000.0); e2.setParams (p); l = in; r = in; run (e2, l, r);
        const double g = toneDb (l, fs, 1000.0, N / 2, N) + 10.0;
        const double expect = 20.0 * std::log10 (0.5 * (1.0 + std::pow (10.0, -15.0 / 20.0)));
        check (std::abs (g - expect) < 0.1, "mix do compressor 50%: soma exata", g, "dB");
    }

    //================================================================ sidechain
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.attackMs = 1.0f;
        Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 40.0, -10.0, N), r = l; run (e, l, r);
        const double g0 = -10.0 - toneDb (l, fs, 40.0, N / 2, N);
        p.scHpfHz = 500.0f;
        Engine e2; e2.prepare (48000.0); e2.setParams (p); l = sine (fs, 40.0, -10.0, N); r = l; run (e2, l, r);
        const double g1 = -10.0 - toneDb (l, fs, 40.0, N / 2, N);
        check (g0 > 14.0 && g1 < 0.6, "low cut 500 Hz tira o grave de 40 Hz do detector", g1, "dB (antes 15)");
    }
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.attackMs = 1.0f; p.scTilt = 1.0f;
        Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 100.0, -20.0, N), r = l; run (e, l, r);
        const double gLow = -20.0 - toneDb (l, fs, 100.0, N / 2, N);
        Engine e2; e2.prepare (48000.0); e2.setParams (p); l = sine (fs, 8000.0, -20.0, N); r = l; run (e2, l, r);
        const double gHigh = -20.0 - toneDb (l, fs, 8000.0, N / 2, N);
        check (gHigh - gLow > 7.0, "tone shift agudo: comprime mais o agudo que o grave", gHigh - gLow, "dB");
    }
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.attackMs = 1.0f;
        Buf l = sine (fs, 1000.0, -40.0, N), r = l, sc = sine (fs, 200.0, -6.0, N);
        Engine e; e.prepare (48000.0); e.setParams (p); run (e, l, r, &sc, &sc);
        const double g = -40.0 - toneDb (l, fs, 1000.0, N / 2, N);
        check (std::abs (g - 18.0) < 0.3, "sidechain externo comanda a reducao", g, "dB");
    }
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.attackMs = 1.0f; p.link = 100.0f;
        Buf l = sine (fs, 1000.0, -10.0, N), r = sine (fs, 1500.0, -40.0, N);
        Engine e; e.prepare (48000.0); e.setParams (p); run (e, l, r);
        const double gr1 = -40.0 - toneDb (r, fs, 1500.0, N / 2, N);
        p.link = 0.0f; l = sine (fs, 1000.0, -10.0, N); r = sine (fs, 1500.0, -40.0, N);
        Engine e2; e2.prepare (48000.0); e2.setParams (p); run (e2, l, r);
        const double gr0 = -40.0 - toneDb (r, fs, 1500.0, N / 2, N);
        check (std::abs (gr1 - 15.0) < 0.3 && std::abs (gr0) < 0.05, "stereo link: casado 15 dB / dual mono 0 dB no canal fraco", gr1, "dB");
    }

    //================================================================ oversampling
    {
        Halfband2x h; double worst = 0.0;
        for (double f : { 100.0, 1000.0, 10000.0, 18000.0, 20000.0 })
        {
            Buf in = sine (fs, f, 0.0, 48000), out (in.size());
            Halfband2x hb;
            for (size_t i = 0; i < in.size(); ++i) { float a, b; hb.up (in[i], a, b); out[i] = hb.down (a, b); }
            worst = std::max (worst, std::abs (toneDb (out, fs, f, 24000, 48000)));
        }
        check (worst < 0.05, "oversampling: plano ate 20 kHz", worst, "dB desvio");

        // tom em 30 kHz na taxa dobrada dobraria para 18 kHz
        Halfband2x hb; Buf out (48000);
        for (int i = 0; i < 48000; ++i)
            out[(size_t) i] = hb.down ((float) std::sin (2.0 * kPi * 30000.0 * (2 * i) / 96000.0), (float) std::sin (2.0 * kPi * 30000.0 * (2 * i + 1) / 96000.0));
        const double rej = toneDb (out, fs, 18000.0, 24000, 48000);
        check (rej < -60.0, "oversampling: rejeita o que dobraria (alias)", rej, "dB");
        (void) h;
    }

    //================================================================ saturador
    auto satRun = [&] (bool tom, bool harm, float amount, double f, double inDb, float tone, Buf& out, float mix = 100.0f)
    {
        Params p; p.compOn = false; p.spatOn = false; p.satAmount = amount; p.satTom = tom; p.satHarm = harm; p.satTone = tone; p.satMix = mix;
        Engine e; e.prepare (48000.0); e.setParams (p); out = sine (fs, f, inDb, N); Buf r = out; run (e, out, r);
    };
    {
        Buf o; satRun (true, false, 0.0f, 1000.0, -6.0, 0.0f, o);
        check (o == sine (fs, 1000.0, -6.0, N), "saturador em 0: original bit a bit");

        satRun (false, false, 6.0f, 1000.0, -6.0, 0.0f, o);
        const double h2n = toneDb (o, fs, 2000.0, N / 2, N), h3n = toneDb (o, fs, 3000.0, N / 2, N), f1 = toneDb (o, fs, 1000.0, N / 2, N);
        check (h3n - f1 > -30.0 && h2n - f1 < -70.0, "curva limpa: so harmonicos impares", h3n - f1, "dB (3o)");

        satRun (true, false, 6.0f, 1000.0, -6.0, 0.0f, o);
        const double h2t = toneDb (o, fs, 2000.0, N / 2, N) - toneDb (o, fs, 1000.0, N / 2, N);
        check (h2t > -35.0, "TOM: acrescenta 2o harmonico", h2t, "dB");
        double dc = 0.0; for (int i = N / 2; i < N; ++i) dc += o[(size_t) i]; dc /= (N / 2);
        check (std::abs (dc) < 1e-3, "TOM: sem DC na saida", dc * 1000.0, "x1e-3");

        Buf oh; satRun (false, false, 3.0f, 1000.0, -12.0, 0.0f, o); satRun (false, true, 3.0f, 1000.0, -12.0, 0.0f, oh);
        const double t3 = toneDb (o, fs, 3000.0, N / 2, N) - toneDb (o, fs, 1000.0, N / 2, N);
        const double c3 = toneDb (oh, fs, 3000.0, N / 2, N) - toneDb (oh, fs, 1000.0, N / 2, N);
        check (c3 > -60.0 && t3 > -60.0, "HRMC: 3o harmonico presente", c3, "dB (limpa abaixo)");
        std::printf ("        (3o harmonico: curva limpa %.1f dB, HRMC %.1f dB)\n", t3, c3);

        // sinal pequeno: resposta plana mesmo com TONE SHIFT no maximo
        Buf a, b, c;
        satRun (true, false, 5.0f, 100.0, -60.0, 1.0f, a); satRun (true, false, 5.0f, 1000.0, -60.0, 1.0f, b); satRun (true, false, 5.0f, 8000.0, -60.0, 1.0f, c);
        const double ga = toneDb (a, fs, 100.0, N / 2, N), gb = toneDb (b, fs, 1000.0, N / 2, N), gc = toneDb (c, fs, 8000.0, N / 2, N);
        check (std::abs (ga - gb) < 0.15 && std::abs (gc - gb) < 0.15, "tone shift do saturador: timbre plano em sinal baixo", std::max (std::abs (ga - gb), std::abs (gc - gb)), "dB");

        // tone shift muda ONDE satura
        satRun (false, false, 6.0f, 5000.0, -12.0, 1.0f, a); satRun (false, false, 6.0f, 5000.0, -12.0, -1.0f, b);
        const double hi = toneDb (a, fs, 15000.0, N / 2, N) - toneDb (a, fs, 5000.0, N / 2, N);
        const double lo = toneDb (b, fs, 15000.0, N / 2, N) - toneDb (b, fs, 5000.0, N / 2, N);
        check (hi - lo > 10.0, "tone shift agudo satura mais os agudos", hi - lo, "dB a mais de 3o");

        // aliasing: 3o harmonico de 9 kHz = 27 kHz dobraria para 21 kHz
        satRun (false, false, 8.0f, 9000.0, -6.0, 0.0f, o);
        const double al = toneDb (o, fs, 21000.0, N / 2, N) - toneDb (o, fs, 9000.0, N / 2, N);
        check (al < -55.0, "saturador: alias controlado pelo 2x", al, "dB");

        satRun (true, false, 10.0f, 1000.0, 0.0, 0.0f, o);
        check (peakDb (o, N / 2, N) < 0.0, "saturador no maximo com entrada em 0 dBFS nao passa de 0", peakDb (o, N / 2, N), "dBFS");

        satRun (true, true, 6.0f, 1000.0, -6.0, 0.0f, o, 0.0f);
        Buf ref = sine (fs, 1000.0, -6.0, N);
        check (o == ref, "mix do saturador 0%: original bit a bit");
    }
    {   // posicao PRE: o compressor enxerga o sinal ja saturado
        Params p; p.spatOn = false; p.threshDb = -30.0f; p.attackMs = 1.0f; p.knee = 0.0f; p.satAmount = 8.0f; p.satTom = false;
        p.satPre = false; Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 1000.0, -34.0, N), r = l; run (e, l, r);
        const double grPost = e.grMeter.load();
        p.satPre = true; Engine e2; e2.prepare (48000.0); e2.setParams (p); l = sine (fs, 1000.0, -34.0, N); r = l; run (e2, l, r);
        const double grPre = e2.grMeter.load();
        check (grPost < 0.01 && grPre > 1.0, "PRE COMP: saturador antes empurra o compressor", grPre, "dB");
    }

    //================================================================ espacial
    {
        Params p; p.compOn = false; p.satOn = false;
        p.width = 0.0f; Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 1000.0, -6.0, N), r = sine (fs, 1700.0, -6.0, N); run (e, l, r);
        double md = 0.0; for (int i = 0; i < N; ++i) md = std::max (md, (double) std::abs (l[(size_t) i] - r[(size_t) i]));
        check (md < 1e-6, "width MONO: L = R");

        p.width = 200.0f; Engine e2; e2.prepare (48000.0); e2.setParams (p); l = sine (fs, 1000.0, -6.0, N); r = l; run (e2, l, r);
        check (peakDb (l, N / 2, N) < -100.0, "width LADOS: o centro some", peakDb (l, N / 2, N), "dB");

        p.width = 100.0f; p.monoBassHz = 300.0f; Engine e3; e3.prepare (48000.0); e3.setParams (p);
        l = sine (fs, 50.0, -6.0, N); r = l; for (auto& v : r) v = -v; run (e3, l, r);
        const double lowSide = toneDb (l, fs, 50.0, N / 2, N) + 6.0;
        Engine e4; e4.prepare (48000.0); e4.setParams (p); l = sine (fs, 3000.0, -6.0, N); r = l; for (auto& v : r) v = -v; run (e4, l, r);
        const double hiSide = toneDb (l, fs, 3000.0, N / 2, N) + 6.0;
        check (lowSide < -25.0 && std::abs (hiSide) < 0.1, "mono bass 300 Hz: grave vai para o centro, agudo fica", lowSide, "dB (lados em 50 Hz)");

        p.monoBassHz = 20.0f; p.air = 100.0f;
        Engine e5; e5.prepare (48000.0); e5.setParams (p); l = sine (fs, 15000.0, -20.0, N); r = l; run (e5, l, r);
        const double ah = toneDb (l, fs, 15000.0, N / 2, N) + 20.0;
        Engine e6; e6.prepare (48000.0); e6.setParams (p); l = sine (fs, 1000.0, -20.0, N); r = l; run (e6, l, r);
        const double am = toneDb (l, fs, 1000.0, N / 2, N) + 20.0;
        check (ah > 6.0 && ah < 8.5 && std::abs (am) < 0.3, "AIR no maximo: +7 dB em 15 kHz, medio intacto", ah, "dB");
    }

    //================================================================ saida
    {
        Params p; p.outDb = 6.0f; Engine e; e.prepare (48000.0); e.setParams (p); Buf l = sine (fs, 1000.0, -20.0, N), r = l; run (e, l, r);
        check (std::abs (toneDb (l, fs, 1000.0, N / 2, N) + 14.0) < 0.01, "output +6 dB", toneDb (l, fs, 1000.0, N / 2, N) + 20.0, "dB");
        p.outDb = -60.0f; Engine e2; e2.prepare (48000.0); e2.setParams (p); l = sine (fs, 1000.0, -6.0, N); r = l; run (e2, l, r);
        check (peakDb (l, N / 2, N) < -200.0, "output no minimo: silencio");
    }

    //================================================================ sem clique ao ligar/desligar e mexer
    {
        Params p; p.threshDb = -30.0f; p.makeupDb = 6.0f; p.satAmount = 6.0f; p.width = 150.0f; p.air = 50.0f;
        Engine e; e.prepare (fs); e.setParams (p);
        Buf l = sine (fs, 100.0, -12.0, N), r = l;
        std::mt19937 rng (3);
        for (int pos = 0, k = 0; pos < N; pos += 512, ++k)
        {
            if (k % 8 == 3) p.compOn = ! p.compOn;
            if (k % 8 == 5) p.satOn = ! p.satOn;
            if (k % 8 == 7) p.spatOn = ! p.spatOn;
            if (k % 5 == 0) { p.threshDb = -40.0f + (float) (rng() % 30); p.outDb = (float) (rng() % 6) - 3.0f; p.satMix = (float) (rng() % 100); p.width = (float) (rng() % 200); }
            e.setParams (p);
            float* io[2] = { l.data() + pos, r.data() + pos };
            e.process (io, 2, nullptr, std::min (512, N - pos));
        }
        double maxStep = 0.0; bool finite = true;
        for (int i = 1; i < N; ++i) { maxStep = std::max (maxStep, (double) std::abs (l[(size_t) i] - l[(size_t) i - 1])); finite = finite && std::isfinite (l[(size_t) i]); }
        check (finite && maxStep < 0.08, "ligar/desligar secoes e girar knobs: sem clique", maxStep, "maior salto");
    }

    //================================================================ outras taxas, mono, CPU
    for (double sr : { 44100.0, 96000.0 })
    {
        Params p = compOnly(); p.threshDb = -30.0f; p.attackMs = 1.0f;
        Engine e; e.prepare (sr); e.setParams (p);
        const int n = (int) sr * 2; Buf l = sine (sr, 1000.0, -10.0, n);
        float* io[1] = { l.data() };
        e.process (io, 1, nullptr, n);
        const double g = -10.0 - toneDb (l, sr, 1000.0, n / 2, n);
        check (std::abs (g - 15.0) < 0.15, sr < 50000.0 ? "44.1 kHz mono: 15 dB" : "96 kHz mono: 15 dB", g, "dB");
    }
    {
        Params p; p.threshDb = -30.0f; p.autoRel = true; p.satAmount = 7.0f; p.satHarm = true; p.satTone = 0.5f; p.scHpfHz = 100.0f; p.scTilt = 0.3f;
        p.air = 60.0f; p.width = 140.0f; p.monoBassHz = 120.0f;
        Engine e; e.prepare (48000.0); e.setParams (p);
        std::mt19937 rng (1); std::uniform_real_distribution<float> d (-0.5f, 0.5f);
        const int n = 48000 * 20; Buf l ((size_t) n), r ((size_t) n); for (int i = 0; i < n; ++i) { l[(size_t) i] = d (rng); r[(size_t) i] = d (rng); }
        const auto t0 = std::chrono::steady_clock::now();
        run (e, l, r);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool finite = true; for (float v : l) finite = finite && std::isfinite (v);
        check (finite && sec / 20.0 < 0.10, "CPU com tudo ligado (48 kHz estereo)", 100.0 * sec / 20.0, "% de um nucleo");
    }

    std::printf ("\n%d passaram, %d falharam\n", passed, failed);
    return failed == 0 ? 0 : 1;
}
