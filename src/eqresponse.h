#pragma once

#include <QList>

#include "protocol.h"

// The curve's approximate frequency response, for drawing it. Each band is
// modelled as a standard biquad (the RBJ audio-EQ cookbook) at 48 kHz; the
// headset's own DSP may differ in detail.
namespace EqResponse {

// The points, in Hz, spaced evenly on a log scale from 20 Hz to 20 kHz.
QList<double> frequencies(int points);
// The summed gain of all five bands, in dB, at each of those points.
QList<double> response(const Protocol::Equalizer &eq, int points);
double bandGainAt(int type, double frequency, double gainDb, double q, double at);

} // namespace EqResponse
