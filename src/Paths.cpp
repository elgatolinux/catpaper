#include "Paths.h"

#include <QDir>
#include <QStandardPaths>

QDir Paths::cacheRoot() {
    return QDir(QDir::home().filePath(".cache/wallpaper_picker"));
}

QString Paths::cacheRootStr() {
    return cacheRoot().absolutePath();
}

QString Paths::configPathStr() {
    return QDir::home().filePath(".config/wallpaper_picker/config.conf");
}

Paths::Paths(QObject *parent) : QObject(parent) {}

QString Paths::getCacheDir(const QString &name) const {
    return QDir::home().filePath(".cache") + "/" + name;
}

QString Paths::getRunDir(const QString &name) const {
    return cacheRootStr() + "/" + name;
}

QString Paths::logDir() { return cacheRootStr(); }
QString Paths::homeDir() { return "file://" + QDir::homePath(); }
QString Paths::thumbDir() { return "file://" + cacheRootStr() + "/thumbs"; }
QString Paths::searchDir() { return "file://" + cacheRootStr() + "/search_thumbs"; }
QString Paths::markerDir() { return "file://" + cacheRootStr() + "/colors_markers"; }
