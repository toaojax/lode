#pragma once

#include <QStringList>

// Commands that need no window: list, create, launch, skill install.
// Each takes --json where it prints something, and exits 0 or 1 with errors
// on stderr.
namespace Cli {
bool wantsWindow(const QStringList &arguments);
int run(const QStringList &arguments);
}
