#pragma once

#include <QDir>
#include <QObject>
#include <QString>

class Paths : public QObject {
    Q_OBJECT
public:
    explicit Paths(QObject *parent = nullptr);

    static QDir cacheRoot();
    static QString cacheRootStr();
    static QString configPathStr();

    Q_INVOKABLE QString getCacheDir(const QString &name) const;
    Q_INVOKABLE QString getRunDir(const QString &name) const;

    Q_PROPERTY(QString logDir READ logDir CONSTANT)
    Q_PROPERTY(QString homeDir READ homeDir CONSTANT)
    Q_PROPERTY(QString thumbDir READ thumbDir CONSTANT)
    Q_PROPERTY(QString searchDir READ searchDir CONSTANT)
    Q_PROPERTY(QString markerDir READ markerDir CONSTANT)

    static QString logDir();
    static QString homeDir();
    static QString thumbDir();
    static QString searchDir();
    static QString markerDir();
};
