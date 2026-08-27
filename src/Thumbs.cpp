#include "Thumbs.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QSet>

namespace thumbs {

static const QSet<QString> &imgExts() {
    static const QSet<QString> set = {".jpg", ".jpeg", ".png", ".webp", ".gif"};
    return set;
}

static const QSet<QString> &vidExts() {
    static const QSet<QString> set = {".mp4", ".mkv", ".mov", ".webm", ".m4v"};
    return set;
}

QStringList imageExtensions() { return QStringList(imgExts().begin(), imgExts().end()); }
QStringList videoExtensions() { return QStringList(vidExts().begin(), vidExts().end()); }

bool makeThumb(const QString &src, const QString &dst) {
    QImageReader reader(src);
    QImage img = reader.read();
    if (img.isNull())
        return false;
    if (img.height() > 420)
        img = img.scaledToHeight(420, Qt::SmoothTransformation);
    QDir().mkpath(QFileInfo(dst).absolutePath());
    return img.convertToFormat(QImage::Format_RGB32).save(dst, "JPEG", 82);
}

QString dominantHex(const QString &thumbPath) {
    QImage img(thumbPath);
    if (img.isNull())
        return {};
    const QColor c = img.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).pixelColor(0, 0);
    int h, s, v;
    c.getHsv(&h, &s, &v);
    s = qMin(255, s * 2);
    QColor boosted = QColor::fromHsv(h, s, v);
    return QString("%1%2%3")
        .arg(boosted.red(), 2, 16, QChar('0'))
        .arg(boosted.green(), 2, 16, QChar('0'))
        .arg(boosted.blue(), 2, 16, QChar('0'))
        .toUpper();
}

static bool videoThumb(const QString &src, const QString &dst) {
    QProcess p;
    p.start("ffmpeg", {"-y", "-ss", "0", "-i", src, "-frames:v", "1",
                       "-vf", "scale=420:-2", "-f", "image2", dst});
    if (!p.waitForStarted(2000))
        return false;
    if (!p.waitForFinished(20000))
        return false;
    return QFile::exists(dst);
}

struct Entry {
    QString name;
    QString thumbName;
    bool isVideo = false;
};

int extractWork(const QString &srcDir, const QString &thumbsDir, const QString &markersDir) {
    QDir sd(srcDir);
    QDir td(thumbsDir);
    QDir md(markersDir);
    td.mkpath(".");
    md.mkpath(".");

    QList<Entry> entries;
    for (const QString &name : sd.entryList(QDir::Files | QDir::NoDotAndDotDot)) {
        if (name.startsWith(QChar('.')))
            continue;
        const QString ext = QFileInfo(name).suffix().toLower().prepend(".");
        const bool isVideo = vidExts().contains(ext) || name.startsWith("000_");
        if (!isVideo && !imgExts().contains(ext))
            continue;
        entries.append({name, isVideo ? "000_" + name : name, isVideo});
    }
    std::sort(entries.begin(), entries.end(),
              [](const Entry &a, const Entry &b) { return a.name < b.name; });

    int done = 0;
    for (const Entry &e : entries) {
        const QString thumb = td.filePath(e.thumbName);
        const bool markerExists = !md.entryList({e.thumbName + "_HEX_*"}, QDir::Files).isEmpty();

        if (!QFile::exists(thumb)) {
            if (e.isVideo)
                videoThumb(sd.filePath(e.name), thumb);
            else
                makeThumb(sd.filePath(e.name), thumb);
        }
        if (!QFile::exists(thumb) || markerExists)
            continue;
        const QString hex = dominantHex(thumb);
        if (hex.isEmpty())
            continue;
        QFile m(md.filePath(e.thumbName + "_HEX_" + hex));
        if (m.open(QIODevice::WriteOnly | QIODevice::NewOnly))
            m.close();
        ++done;
    }
    return done;
}

} // namespace thumbs
