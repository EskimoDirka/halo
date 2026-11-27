#pragma once

#include <QStringList>

// Commands that need no display, so scripts and agents can drive the headset:
//
//   halo status [--json]
//   halo set volume <0-100> [--json]
//   halo set prompts <on|off> [--json]
//   halo watch            keep settings across power-ons, without a window
//   halo skill install    install the agent skill bundled in the binary
//
// Each exits 0 on success and 1 with the error on stderr.
namespace Cli {
bool handles(const QStringList &args);
int run(const QStringList &args);
}
