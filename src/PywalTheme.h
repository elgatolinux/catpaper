#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QObject>

class PywalTheme : public QObject {
    Q_OBJECT
public:
    explicit PywalTheme(QObject *parent = nullptr);

    Q_PROPERTY(QColor base READ base NOTIFY themeChanged)
    Q_PROPERTY(QColor text READ text NOTIFY themeChanged)
    Q_PROPERTY(QColor textDim READ textDim NOTIFY themeChanged)
    Q_PROPERTY(QColor mantle READ mantle NOTIFY themeChanged)
    Q_PROPERTY(QColor surface0 READ surface0 NOTIFY themeChanged)
    Q_PROPERTY(QColor surface1 READ surface1 NOTIFY themeChanged)
    Q_PROPERTY(QColor surface2 READ surface2 NOTIFY themeChanged)

    QColor base() const { return m_base; }
    QColor text() const { return m_text; }
    QColor textDim() const;
    QColor mantle() const;
    QColor surface0() const;
    QColor surface1() const;
    QColor surface2() const;

signals:
    void themeChanged();

private:
    void load();
    void onFileChanged();

    QColor m_base;
    QColor m_text;
    QFileSystemWatcher m_watcher;
};
