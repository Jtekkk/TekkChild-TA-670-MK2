// Offline characterisation harness for the TekkChild TA-670 MK2.
//
// Runs the real processing chain and measures the things the product is sold
// on -- the static compression curve, the harmonic profile of the tube/iron
// colour, and the frequency response -- writing both CSV data and standalone
// SVG plots (no external plotting dependency, so it also runs in CI).
//
// These are characterisation curves of the current model: they are the data
// you calibrate against reference-hardware measurements, and they let the
// "identical compression curve" / "LF-weighted harmonics" claims be checked
// rather than asserted.
//
//   usage: TekkChild670Measure [output-directory]   (default: ./measurements)

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <juce_dsp/juce_dsp.h>

#include "PluginProcessor.h"

namespace
{

constexpr double kPi = 3.14159265358979323846;

//==============================================================================
// Minimal standalone SVG line-plotter (white-background measurement style).
class SvgPlot
{
public:
    SvgPlot (int w, int h, std::string title, std::string xLabel, std::string yLabel,
             bool logX = false)
        : width (w), height (h), titleText (std::move (title)),
          xLabelText (std::move (xLabel)), yLabelText (std::move (yLabel)), logXAxis (logX)
    {
    }

    void setXRange (double lo, double hi) { xLo = lo; xHi = hi; }
    void setYRange (double lo, double hi) { yLo = lo; yHi = hi; }

    void addSeries (const std::string& name, const std::string& colour,
                    std::vector<std::pair<double, double>> points)
    {
        series.push_back ({ name, colour, std::move (points) });
    }

    juce::String toSvg() const
    {
        const double plotL = 78.0, plotR = width - 170.0;
        const double plotT = 52.0, plotB = height - 64.0;

        auto mapX = [&] (double x)
        {
            const double t = logXAxis
                ? (std::log10 (x) - std::log10 (xLo)) / (std::log10 (xHi) - std::log10 (xLo))
                : (x - xLo) / (xHi - xLo);
            return plotL + t * (plotR - plotL);
        };
        auto mapY = [&] (double y)
        {
            const double t = (y - yLo) / (yHi - yLo);
            return plotB - t * (plotB - plotT);
        };

        std::ostringstream s;
        s << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << width
          << "\" height=\"" << height << "\" viewBox=\"0 0 " << width << " " << height
          << "\" font-family=\"Helvetica,Arial,sans-serif\">\n";
        s << "<rect width=\"" << width << "\" height=\"" << height << "\" fill=\"#ffffff\"/>\n";

        // title
        s << "<text x=\"" << width / 2 << "\" y=\"28\" text-anchor=\"middle\" "
          << "font-size=\"19\" font-weight=\"bold\" fill=\"#1d1d20\">" << esc (titleText)
          << "</text>\n";

        // plot frame
        s << rect (plotL, plotT, plotR - plotL, plotB - plotT, "none", "#cccccc", 1.0);

        // gridlines + ticks
        for (double xt : logXAxis ? logTicks (xLo, xHi) : linTicks (xLo, xHi))
        {
            const double px = mapX (xt);
            s << line (px, plotT, px, plotB, "#eeeeee", 1.0);
            s << line (px, plotB, px, plotB + 5, "#888888", 1.0);
            s << text (px, plotB + 18, fmt (xt), "middle", 12, "#444444");
        }
        for (double yt : linTicks (yLo, yHi))
        {
            const double py = mapY (yt);
            s << line (plotL, py, plotR, py, "#eeeeee", 1.0);
            s << line (plotL - 5, py, plotL, py, "#888888", 1.0);
            s << text (plotL - 9, py + 4, fmt (yt), "end", 12, "#444444");
        }

        // axis labels
        s << text ((plotL + plotR) / 2, height - 22, esc (xLabelText), "middle", 13, "#1d1d20");
        s << "<text x=\"22\" y=\"" << (plotT + plotB) / 2
          << "\" text-anchor=\"middle\" font-size=\"13\" fill=\"#1d1d20\" transform=\"rotate(-90 22 "
          << (plotT + plotB) / 2 << ")\">" << esc (yLabelText) << "</text>\n";

        // series + legend
        double legendY = plotT + 6;
        for (const auto& ser : series)
        {
            std::ostringstream pts;
            bool penDown = false;
            for (const auto& p : ser.points)
            {
                if (! std::isfinite (p.second) || p.first < xLo || p.first > xHi)
                {
                    penDown = false;
                    continue;
                }
                const double px = mapX (p.first);
                const double py = mapY (std::clamp (p.second, yLo, yHi));
                pts << (penDown ? " L" : " M") << r1 (px) << "," << r1 (py);
                penDown = true;
            }
            s << "<path d=\"" << pts.str() << "\" fill=\"none\" stroke=\"" << ser.colour
              << "\" stroke-width=\"2\"/>\n";

            s << line (plotR + 14, legendY, plotR + 38, legendY, ser.colour, 2.5);
            s << text (plotR + 44, legendY + 4, esc (ser.name), "start", 12, "#1d1d20");
            legendY += 20;
        }

        s << "</svg>\n";
        return juce::String (s.str());
    }

private:
    struct Series
    {
        std::string name, colour;
        std::vector<std::pair<double, double>> points;
    };

    static std::string esc (const std::string& in)
    {
        std::string o;
        for (char c : in)
            o += c == '&' ? "&amp;" : c == '<' ? "&lt;" : c == '>' ? "&gt;" : std::string (1, c);
        return o;
    }

    static std::string r1 (double v)
    {
        std::ostringstream o; o.precision (1); o << std::fixed << v; return o.str();
    }

    static std::string fmt (double v)
    {
        std::ostringstream o;
        if (std::abs (v - std::round (v)) < 1.0e-6 && std::abs (v) < 1.0e6)
            o << (long) std::llround (v);
        else if (std::abs (v) < 10.0)
        { o.precision (2); o << std::fixed << v; }
        else
        { o.precision (1); o << std::fixed << v; }
        return o.str();
    }

    static std::string line (double x1, double y1, double x2, double y2,
                             const std::string& c, double w)
    {
        std::ostringstream o;
        o << "<line x1=\"" << r1 (x1) << "\" y1=\"" << r1 (y1) << "\" x2=\"" << r1 (x2)
          << "\" y2=\"" << r1 (y2) << "\" stroke=\"" << c << "\" stroke-width=\"" << w << "\"/>\n";
        return o.str();
    }

    static std::string rect (double x, double y, double w, double h,
                             const std::string& fill, const std::string& stroke, double sw)
    {
        std::ostringstream o;
        o << "<rect x=\"" << r1 (x) << "\" y=\"" << r1 (y) << "\" width=\"" << r1 (w)
          << "\" height=\"" << r1 (h) << "\" fill=\"" << fill << "\" stroke=\"" << stroke
          << "\" stroke-width=\"" << sw << "\"/>\n";
        return o.str();
    }

    static std::string text (double x, double y, const std::string& t,
                             const std::string& anchor, int size, const std::string& c)
    {
        std::ostringstream o;
        o << "<text x=\"" << r1 (x) << "\" y=\"" << r1 (y) << "\" text-anchor=\"" << anchor
          << "\" font-size=\"" << size << "\" fill=\"" << c << "\">" << t << "</text>\n";
        return o.str();
    }

    static std::vector<double> linTicks (double lo, double hi, int target = 6)
    {
        std::vector<double> t;
        const double range = hi - lo;
        if (range <= 0.0)
            return { lo };

        const double raw  = range / target;
        const double mag  = std::pow (10.0, std::floor (std::log10 (raw)));
        const double norm = raw / mag;
        const double step = (norm < 1.5 ? 1.0 : norm < 3.0 ? 2.0 : norm < 7.0 ? 5.0 : 10.0) * mag;

        for (double v = std::ceil (lo / step) * step; v <= hi + step * 1.0e-6; v += step)
            t.push_back (std::abs (v) < step * 1.0e-6 ? 0.0 : v);
        return t;
    }

    static std::vector<double> logTicks (double lo, double hi)
    {
        std::vector<double> t;
        for (int d = (int) std::floor (std::log10 (lo)); d <= (int) std::ceil (std::log10 (hi)); ++d)
            for (double m : { 1.0, 2.0, 5.0 })
            {
                const double v = m * std::pow (10.0, d);
                if (v >= lo * 0.999 && v <= hi * 1.001)
                    t.push_back (v);
            }
        return t;
    }

    int width, height;
    std::string titleText, xLabelText, yLabelText;
    bool logXAxis;
    double xLo = 0, xHi = 1, yLo = 0, yHi = 1;
    std::vector<Series> series;
};

//==============================================================================
constexpr double kFs        = 96000.0; // headroom to characterise up to ~45 kHz
constexpr int    kBlockSize = 512;

void setParam (TekkChild670Processor& proc, const juce::String& id, float plain)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (plain));
}

// Puts the unit in a neutral characterisation state: 0 dB in/out, 100% wet,
// no sidechain filter, DC Threshold centred, channels independent.
void configureNeutral (TekkChild670Processor& proc, bool agcIn, float thresholdKnob,
                       int timeConstant, int engine)
{
    setParam (proc, "mode", 0.0f);                 // Left / Right (independent)
    setParam (proc, "quality", (float) engine);
    setParam (proc, "purist", 0.0f);
    setParam (proc, "bypass", 0.0f);

    for (const char* side : { "A", "B" })
    {
        setParam (proc, juce::String ("input") + side, 0.0f);
        setParam (proc, juce::String ("output") + side, 0.0f);
        setParam (proc, juce::String ("mix") + side, 100.0f);
        setParam (proc, juce::String ("dcthresh") + side, 0.0f);
        setParam (proc, juce::String ("schpf") + side, 0.0f);
        setParam (proc, juce::String ("threshold") + side, thresholdKnob);
        setParam (proc, juce::String ("timeconst") + side, (float) timeConstant);
        setParam (proc, juce::String ("compin") + side, agcIn ? 1.0f : 0.0f);
    }
}

// Drives a steady sine through the processor, discards settleSamples, then
// returns captureSamples of channel-0 output (steady state).
std::vector<float> capture (TekkChild670Processor& proc, double freq, double amplitude,
                            int settleSamples, int captureSamples)
{
    juce::AudioBuffer<float> buffer (2, kBlockSize);
    juce::MidiBuffer midi;

    std::vector<float> out;
    out.reserve ((size_t) captureSamples);

    const double inc = 2.0 * kPi * freq / kFs;
    double phase = 0.0;
    int produced = 0;
    const int total = settleSamples + captureSamples;

    while (produced < total)
    {
        for (int i = 0; i < kBlockSize; ++i)
        {
            const float s = (float) (amplitude * std::sin (phase));
            phase += inc;
            buffer.setSample (0, i, s);
            buffer.setSample (1, i, s);
        }

        proc.processBlock (buffer, midi);

        for (int i = 0; i < kBlockSize && produced < total; ++i, ++produced)
            if (produced >= settleSamples)
                out.push_back (buffer.getSample (0, i));
    }

    return out;
}

double rms (const std::vector<float>& x)
{
    double sum = 0.0;
    for (float v : x) sum += (double) v * v;
    return std::sqrt (sum / std::max<size_t> (1, x.size()));
}

double toDb (double lin) { return 20.0 * std::log10 (std::max (lin, 1.0e-9)); }

//==============================================================================
// FFT magnitude spectrum of exactly (1 << order) samples. With a bin-aligned
// tone the frame holds whole periods, so no window is needed.
struct Spectrum
{
    explicit Spectrum (const std::vector<float>& frame, int order)
        : fftOrder (order), fftSize (1 << order)
    {
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) fftSize * 2, 0.0f);
        const int n = std::min (fftSize, (int) frame.size());
        for (int i = 0; i < n; ++i)
            data[(size_t) i] = frame[(size_t) i];

        fft.performFrequencyOnlyForwardTransform (data.data());
        mag.assign (data.begin(), data.begin() + fftSize / 2);
    }

    int binFor (double freq) const { return (int) std::llround (freq * fftSize / kFs); }

    // THD from a fundamental at the given bin: RMS of harmonics over fundamental.
    double thdPercent (int fundamentalBin) const
    {
        if (fundamentalBin <= 0 || fundamentalBin >= (int) mag.size())
            return 0.0;

        const double f = mag[(size_t) fundamentalBin];
        double harmonics = 0.0;
        for (int h = 2; h * fundamentalBin < (int) mag.size(); ++h)
            harmonics += (double) mag[(size_t) (h * fundamentalBin)]
                       * (double) mag[(size_t) (h * fundamentalBin)];

        return f > 1.0e-9 ? 100.0 * std::sqrt (harmonics) / f : 0.0;
    }

    double magnitude (int bin) const
    {
        return (bin >= 0 && bin < (int) mag.size()) ? (double) mag[(size_t) bin] : 0.0;
    }

    int fftOrder, fftSize;
    std::vector<float> mag;
};

//==============================================================================
const char* kAmber = "#d49a3a";
const char* kBlue  = "#3a78c4";
const char* kRed   = "#b03a2e";
const char* kGreen = "#3a9a5a";
const char* kGray  = "#888888";

void writeFile (const juce::File& dir, const juce::String& name, const juce::String& contents)
{
    dir.getChildFile (name).replaceWithText (contents);
}

//==============================================================================
// 1) Static compression curve: steady-state output level vs input level, for a
//    few THRESHOLD settings. Reveals the threshold, the soft knee and the
//    progressive (variable-mu) ratio.
void measureStaticCurve (TekkChild670Processor& proc, const juce::File& dir)
{
    struct Setting { float knob; const char* name; const char* colour; };
    const Setting settings[] = {
        { 3.0f, "Threshold 3", kBlue },
        { 5.0f, "Threshold 5", kAmber },
        { 7.0f, "Threshold 7", kRed },
    };

    const int settle  = (int) (0.40 * kFs);
    const int window  = (int) (0.10 * kFs);

    SvgPlot plot (840, 560, "Static compression curve (1 kHz, position 1)",
                  "Input level (dBFS)", "Output level (dBFS)");
    plot.setXRange (-48.0, 6.0);
    plot.setYRange (-48.0, 6.0);

    juce::String csv ("threshold_knob,input_dbfs,output_dbfs,gain_reduction_db\n");

    std::vector<std::pair<double, double>> unity;
    for (double d = -48.0; d <= 6.0; d += 6.0) unity.push_back ({ d, d });
    plot.addSeries ("Unity", kGray, unity);

    for (const auto& set : settings)
    {
        std::vector<std::pair<double, double>> curve;

        for (double inDb = -48.0; inDb <= 6.0 + 1.0e-6; inDb += 2.0)
        {
            configureNeutral (proc, true, set.knob, 0 /*pos 1*/, 1 /*Zero-Latency*/);
            proc.reset();
            const double amp = std::pow (10.0, inDb / 20.0);
            const auto out   = capture (proc, 1000.0, amp, settle, window);

            const double outDb = toDb (rms (out) * std::sqrt (2.0)); // sine RMS -> peak dBFS
            curve.push_back ({ inDb, outDb });
            csv << set.knob << "," << inDb << "," << outDb << "," << (inDb - outDb) << "\n";
        }

        plot.addSeries (set.name, set.colour, curve);
    }

    writeFile (dir, "static_curve.svg", plot.toSvg());
    writeFile (dir, "static_curve.csv", csv);
}

//==============================================================================
// 2) THD vs input level at 1 kHz with the compressor out, so it isolates the
//    tube + transformer colour. Shows where saturation becomes audible.
void measureThdVsLevel (TekkChild670Processor& proc, const juce::File& dir)
{
    constexpr int order = 14; // 16384-point analysis
    const int settle = 1 << 13;
    const double freq = 1000.0 * (kFs / (1 << order)) * std::round (1000.0 * (1 << order) / kFs)
                      / 1000.0; // snap 1 kHz to a bin

    SvgPlot plot (840, 560, "Harmonic distortion vs level (1 kHz, compressor out)",
                  "Input level (dBFS)", "THD (%)");
    plot.setXRange (-36.0, 6.0);
    plot.setYRange (0.0, 12.0);

    juce::String csv ("input_dbfs,thd_percent\n");
    std::vector<std::pair<double, double>> pts;

    Spectrum probe (std::vector<float> (1 << order, 0.0f), order);
    const int bin = probe.binFor (freq);

    for (double inDb = -36.0; inDb <= 6.0 + 1.0e-6; inDb += 2.0)
    {
        configureNeutral (proc, false /*AGC out*/, 5.0f, 2, 2 /*Studio*/);
            proc.reset();
        const double amp = std::pow (10.0, inDb / 20.0);
        const auto out   = capture (proc, freq, amp, settle, 1 << order);

        const double thd = Spectrum (out, order).thdPercent (bin);
        pts.push_back ({ inDb, thd });
        csv << inDb << "," << thd << "\n";
    }

    plot.addSeries ("THD %", kAmber, pts);
    writeFile (dir, "thd_vs_level.svg", plot.toSvg());
    writeFile (dir, "thd_vs_level.csv", csv);
}

//==============================================================================
// 3) THD vs frequency at a fixed hot level, compressor out. Tests the claim
//    that the iron saturates low frequencies first (LF-weighted harmonics).
void measureThdVsFrequency (TekkChild670Processor& proc, const juce::File& dir)
{
    constexpr int order = 15; // 32768-point: finer bins for low frequencies
    const int settle = 1 << 13;

    SvgPlot plot (840, 560, "Harmonic distortion vs frequency (-6 dBFS, compressor out)",
                  "Frequency (Hz)", "THD (%)", true /*log x*/);
    plot.setXRange (20.0, 10000.0);
    plot.setYRange (0.0, 10.0);

    juce::String csv ("frequency_hz,thd_percent\n");
    std::vector<std::pair<double, double>> pts;

    Spectrum probe (std::vector<float> (1 << order, 0.0f), order);

    for (int k = 0; k < 28; ++k)
    {
        const double target = 20.0 * std::pow (10000.0 / 20.0, k / 27.0);
        const int bin = probe.binFor (target);
        const double freq = (double) bin * kFs / (1 << order); // exact bin frequency
        if (bin < 2) continue;

        configureNeutral (proc, false, 5.0f, 2, 2 /*Studio*/);
            proc.reset();
        const auto out = capture (proc, freq, std::pow (10.0, -6.0 / 20.0), settle, 1 << order);

        const double thd = Spectrum (out, order).thdPercent (bin);
        pts.push_back ({ freq, thd });
        csv << freq << "," << thd << "\n";
    }

    plot.addSeries ("THD %", kRed, pts);
    writeFile (dir, "thd_vs_frequency.svg", plot.toSvg());
    writeFile (dir, "thd_vs_frequency.csv", csv);
}

//==============================================================================
// 4) Frequency response at a low (linear) level, compressor out: the band
//    edges contributed by the transformer models and DC blockers.
void measureFrequencyResponse (TekkChild670Processor& proc, const juce::File& dir)
{
    constexpr int order = 15;
    const int settle = 1 << 13;
    const double amp  = std::pow (10.0, -30.0 / 20.0); // stay in the linear regime

    SvgPlot plot (840, 560, "Frequency response (-30 dBFS, compressor out)",
                  "Frequency (Hz)", "Magnitude (dB)", true);
    plot.setXRange (10.0, 40000.0);
    plot.setYRange (-12.0, 3.0);

    juce::String csv ("frequency_hz,magnitude_db\n");
    std::vector<std::pair<double, double>> pts;

    Spectrum probe (std::vector<float> (1 << order, 0.0f), order);

    // reference magnitude at 1 kHz so the response is plotted relative to it
    double refDb = 0.0;
    {
        const int bin = probe.binFor (1000.0);
        const double f = (double) bin * kFs / (1 << order);
        configureNeutral (proc, false, 5.0f, 2, 2);
            proc.reset();
        refDb = toDb (Spectrum (capture (proc, f, amp, settle, 1 << order), order).magnitude (bin));
    }

    for (int k = 0; k < 40; ++k)
    {
        const double target = 10.0 * std::pow (40000.0 / 10.0, k / 39.0);
        const int bin = probe.binFor (target);
        const double freq = (double) bin * kFs / (1 << order);
        if (bin < 2 || freq >= kFs * 0.5) continue;

        configureNeutral (proc, false, 5.0f, 2, 2);
            proc.reset();
        const double mDb = toDb (Spectrum (capture (proc, freq, amp, settle, 1 << order),
                                           order).magnitude (bin)) - refDb;
        pts.push_back ({ freq, mDb });
        csv << freq << "," << mDb << "\n";
    }

    plot.addSeries ("Magnitude", kGreen, pts);
    writeFile (dir, "frequency_response.svg", plot.toSvg());
    writeFile (dir, "frequency_response.csv", csv);
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::File outDir = juce::File::getCurrentWorkingDirectory()
                                  .getChildFile (argc > 1 ? argv[1] : "measurements");
    outDir.createDirectory();

    TekkChild670Processor proc;
    proc.setPlayConfigDetails (2, 2, kFs, kBlockSize);
    proc.prepareToPlay (kFs, kBlockSize);

    std::cout << "TekkChild TA-670 MK2 -- characterisation harness\n"
              << "sample rate " << (int) kFs << " Hz, writing to "
              << outDir.getFullPathName() << "\n" << std::endl;

    std::cout << "  static compression curve..." << std::endl;
    measureStaticCurve (proc, outDir);
    std::cout << "  THD vs level..."             << std::endl;
    measureThdVsLevel (proc, outDir);
    std::cout << "  THD vs frequency..."         << std::endl;
    measureThdVsFrequency (proc, outDir);
    std::cout << "  frequency response..."       << std::endl;
    measureFrequencyResponse (proc, outDir);

    std::cout << "\nWrote static_curve, thd_vs_level, thd_vs_frequency, "
                 "frequency_response (.svg + .csv)" << std::endl;
    return 0;
}
