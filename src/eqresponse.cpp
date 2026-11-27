#include "eqresponse.h"

#include <cmath>
#include <complex>

namespace EqResponse {

static constexpr double SampleRate = 48000.0;

QList<double> frequencies(int points) {
    QList<double> list;
    const double low = std::log10(double(Protocol::EqMinFrequency));
    const double high = std::log10(double(Protocol::EqMaxFrequency));
    for (int i = 0; i < points; ++i)
        list << std::pow(10.0, low + (high - low) * i / double(qMax(1, points - 1)));
    return list;
}

double bandGainAt(int type, double frequency, double gainDb, double q, double at) {
    const double A = std::pow(10.0, gainDb / 40.0);
    const double w0 = 2 * M_PI * frequency / SampleRate;
    const double cosw = std::cos(w0), alpha = std::sin(w0) / (2 * std::max(q, Protocol::EqMinQ));
    const double rootA = std::sqrt(A);
    double b0, b1, b2, a0, a1, a2;
    switch (type) {
    case 1: // low shelf
        b0 = A * ((A + 1) - (A - 1) * cosw + 2 * rootA * alpha);
        b1 = 2 * A * ((A - 1) - (A + 1) * cosw);
        b2 = A * ((A + 1) - (A - 1) * cosw - 2 * rootA * alpha);
        a0 = (A + 1) + (A - 1) * cosw + 2 * rootA * alpha;
        a1 = -2 * ((A - 1) + (A + 1) * cosw);
        a2 = (A + 1) + (A - 1) * cosw - 2 * rootA * alpha;
        break;
    case 2: // high shelf
        b0 = A * ((A + 1) + (A - 1) * cosw + 2 * rootA * alpha);
        b1 = -2 * A * ((A - 1) + (A + 1) * cosw);
        b2 = A * ((A + 1) + (A - 1) * cosw - 2 * rootA * alpha);
        a0 = (A + 1) - (A - 1) * cosw + 2 * rootA * alpha;
        a1 = 2 * ((A - 1) - (A + 1) * cosw);
        a2 = (A + 1) - (A - 1) * cosw - 2 * rootA * alpha;
        break;
    case 3: // low pass
        b0 = (1 - cosw) / 2, b1 = 1 - cosw, b2 = (1 - cosw) / 2;
        a0 = 1 + alpha, a1 = -2 * cosw, a2 = 1 - alpha;
        break;
    case 4: // high pass
        b0 = (1 + cosw) / 2, b1 = -(1 + cosw), b2 = (1 + cosw) / 2;
        a0 = 1 + alpha, a1 = -2 * cosw, a2 = 1 - alpha;
        break;
    case 5: // notch
        b0 = 1, b1 = -2 * cosw, b2 = 1;
        a0 = 1 + alpha, a1 = -2 * cosw, a2 = 1 - alpha;
        break;
    default: // peak
        b0 = 1 + alpha * A, b1 = -2 * cosw, b2 = 1 - alpha * A;
        a0 = 1 + alpha / A, a1 = -2 * cosw, a2 = 1 - alpha / A;
        break;
    }
    const std::complex<double> z = std::polar(1.0, -2 * M_PI * at / SampleRate);
    const std::complex<double> h = (b0 + b1 * z + b2 * z * z) / (a0 + a1 * z + a2 * z * z);
    return 20 * std::log10(std::max(std::abs(h), 1e-6));
}

QList<double> response(const Protocol::Equalizer &eq, int points) {
    const QList<double> at = frequencies(points);
    QList<double> list(points, 0.0);
    for (int band = 0; band < eq.frequencies.size(); ++band)
        for (int i = 0; i < points; ++i)
            list[i] += bandGainAt(eq.types.value(band), eq.frequencies[band], eq.gains.value(band),
                                  eq.q.value(band, 1.0), at[i]);
    return list;
}

} // namespace EqResponse
