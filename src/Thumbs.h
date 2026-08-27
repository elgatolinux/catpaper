#pragma once

#include <QString>
#include <QStringList>

namespace thumbs {

bool makeThumb(const QString &src, const QString &dst);
QString dominantHex(const QString &thumbPath);
int extractWork(const QString &srcDir, const QString &thumbsDir, const QString &markersDir);
QStringList imageExtensions();
QStringList videoExtensions();

} // namespace thumbs
