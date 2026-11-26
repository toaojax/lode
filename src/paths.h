#pragma once

#include <QString>

// Where everything lives. Games share one libraries/assets/versions tree laid
// out like the official launcher's, so Forge and NeoForge installers can write
// into it, and each instance keeps only its own game directory.
namespace Paths {
// Overridden by tests and by LODE_HOME.
void setRoot(const QString &root);
QString root();
QString libraries();
QString assets();
QString versions();
QString instances();
QString java();
QString cache();
QString configFile();
QString accountsFile();

// group:artifact:version[:classifier][@ext] to its Maven repository path.
QString mavenPath(const QString &coordinate);

// Writes beside the target, then renames, so a crash never leaves half a file.
bool writeAtomically(const QString &path, const QByteArray &data, bool privateFile = false);
QByteArray readAll(const QString &path);

// A folder name made from a display name, unique inside parent.
QString uniqueFolder(const QString &parent, const QString &name);
}
