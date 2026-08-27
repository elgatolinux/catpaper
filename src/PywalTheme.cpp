#include "PywalTheme.h"

#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

static QColor blend(const QColor &a, const QColor &b, double p) {
    auto f = [p](int x, int y) { return static_cast<int>(x + (double)(y - x) * p); };
    return QColor(f(a.red(), b.red()), f(a.green(), b.green()), f(a.blue(), b.blue()));
}

static QColor darken(const QColor &c, double f) {
    return QColor(int(c.red() * f), int(c.green() * f), int(c.blue() * f));
}

PywalTheme::PywalTheme(QObject *parent) : QObject(parent) {
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this,
            [this](const QString &) {
                m_watcher.addPath(QDir::home().filePath(".cache/wal/colors.json"));
                load();
                emit themeChanged();
            });
    load();
}

QColor PywalTheme::textDim() const {
    return QColor(m_text.red(), m_text.green(), m_text.blue(), 170);
}

QColor PywalTheme::mantle() const { return darken(m_base, 0.8); }
QColor PywalTheme::surface0() const { return blend(m_base, m_text, 0.07); }
QColor PywalTheme::surface1() const { return blend(m_base, m_text, 0.12); }
QColor PywalTheme::surface2() const { return blend(m_base, m_text, 0.18); }

void PywalTheme::load() {
    const QString path = QDir::home().filePath(".cache/wal/colors.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    const QJsonObject data = QJsonDocument::fromJson(f.readAll()).object();

    const QJsonObject special = data.value("special").toObject();
    const QJsonObject colors = data.value("colors").toObject();

    auto readCol = [&](const char *key, const QJsonObject &obj) -> QColor {
        QString s = data.value(key).toString();
        if (s.isEmpty())
            s = obj.value(key).toString();
        QColor c(s);
        return c;
    };

    const QColor bg = readCol("background", special);
    const QColor fg = readCol("foreground", special);
    m_base = bg.isValid() && bg.alpha() >= 0 ? bg : QColor("#0c0c1e");
    m_text = fg.isValid() ? fg : QColor("#c2c2c6");

    // keep a reference so future palette expansion is easy
    for (int i = 0; i < 16; ++i) {
        const QColor c = readCol(QString("color%1").arg(i).toUtf8().constData(), colors);
        Q_UNUSED(c);
    }

    if (!m_watcher.files().contains(path))
        m_watcher.addPath(path);
}
