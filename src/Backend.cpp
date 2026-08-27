#include "Backend.h"

#include "Paths.h"
#include "Search.h"
#include "Thumbs.h"

#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTextStream>
#include <QTime>
#include <thread>

namespace {
const QStringList TRANSITIONS = {"simple", "fade",   "left",   "right", "top",    "bottom",
                                 "wipe",   "grow",   "center", "outer", "random", "wave"};

QString cacheRootStr() {
    return QDir::home().filePath(".cache/wallpaper_picker");
}

QNetworkRequest netReq(const QUrl &url) {
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader,
                QStringLiteral("Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
                               "(KHTML, like Gecko) Chrome/126.0 Safari/537.36"));
    r.setTransferTimeout(90000);
    return r;
}

QString splitCmd(const QString &cmd) {
    // keep it simple: splitCommand from Qt (handles quotes)
    const auto parts = QProcess::splitCommand(cmd);
    if (parts.isEmpty())
        return {};
    return parts.value(0);
}
} // namespace

Backend::Backend(QObject *parent) : QObject(parent) {
    m_nam = new QNetworkAccessManager(this);
    m_search = new Search(this);
    connect(m_search, &Search::errorOccurred, this,
            [this](const QString &e) { queueNotice(tr("Búsqueda: %1").arg(e)); });

    reloadConfig();

    const QString envDir = qEnvironmentVariable("WALLPAPER_DIR");
    m_wallpaperDir = !envDir.isEmpty() ? envDir
                                       : configValue("wallpaper_dir",
                                                     QDir::home().filePath("Pictures/Wallpapers"));

    m_noticeTimer.setSingleShot(true);
    m_noticeTimer.setInterval(3500);
    connect(&m_noticeTimer, &QTimer::timeout, this, &Backend::clearNotice);

    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &path) {
        const QString t = QDir(cacheRootStr()).filePath("thumbs");
        const QString s = QDir(cacheRootStr()).filePath("search_thumbs");
        const QString m = QDir(cacheRootStr()).filePath("colors_markers");
        if (path == t)
            emit thumbsRefreshed();
        else if (path == s)
            emit searchRefreshed();
        else if (path == m)
            emit markersRefreshed();
        else if (QFileInfo(path).absoluteFilePath() == QFileInfo(m_wallpaperDir).absoluteFilePath())
            emit srcRefreshed();
    });
}

QString Backend::homeDir() const { return "file://" + QDir::homePath(); }
QString Backend::thumbDir() const { return "file://" + cacheRootStr() + "/thumbs"; }
QString Backend::searchDir() const { return "file://" + cacheRootStr() + "/search_thumbs"; }
QString Backend::markerDir() const { return "file://" + cacheRootStr() + "/colors_markers"; }

void Backend::reloadConfig() {
    m_cfg.clear();
    QFile f(Paths::configPathStr());
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
        const QString l = line.trimmed();
        if (l.isEmpty() || l.startsWith('#') || !l.contains('='))
            continue;
        const QString k = l.section('=', 0, 0).trimmed();
        const QString v = l.section('=', 1).trimmed();
        m_cfg.insert(k, v);
    }
}

QString Backend::configValue(const QString &key, const QString &fallback) const {
    return m_cfg.value(key, fallback);
}

void Backend::setNotice(const QString &msg) {
    m_notice = msg;
    emit noticeChanged();
    m_noticeTimer.start();
}

void Backend::queueNotice(const QString &msg) {
    QMetaObject::invokeMethod(this, "setNotice", Qt::QueuedConnection, Q_ARG(QString, msg));
}

void Backend::clearNotice() {
    m_notice.clear();
    emit noticeChanged();
}

void Backend::setDownloading(bool value, const QString &name) {
    m_downloading = value;
    m_downloadingName = name;
    emit downloadStateChanged();
}

void Backend::queueDownloading(bool value, const QString &name) {
    QMetaObject::invokeMethod(this, "setDownloading", Qt::QueuedConnection, Q_ARG(bool, value),
                              Q_ARG(QString, name));
}

void Backend::ensureDirs() {
    const QStringList dirs = {QDir(cacheRootStr()).filePath("thumbs"),
                              QDir(cacheRootStr()).filePath("search_thumbs"),
                              QDir(cacheRootStr()).filePath("colors_markers"), m_wallpaperDir};
    for (const QString &d : dirs)
        QDir().mkpath(d);
    setupWatchers();
}

void Backend::setupWatchers() {
    const QStringList dirs = {QDir(cacheRootStr()).filePath("thumbs"),
                              QDir(cacheRootStr()).filePath("search_thumbs"),
                              QDir(cacheRootStr()).filePath("colors_markers"), m_wallpaperDir};
    for (const QString &d : dirs)
        m_watcher.addPath(d);
}

void Backend::persistConfig() {
    QFile f(Paths::configPathStr());
    QDir().mkpath(QFileInfo(f).absolutePath());
    QStringList lines;
    if (f.open(QIODevice::ReadOnly))
        lines = QString::fromUtf8(f.readAll()).split('\n');
    f.close();
    QStringList out;
    bool found = false;
    for (const QString &line : lines) {
        if (line.trimmed().startsWith("wallpaper_dir")) {
            out << "wallpaper_dir=" + m_wallpaperDir;
            found = true;
        } else {
            out << line;
        }
    }
    if (!found)
        out << "wallpaper_dir=" + m_wallpaperDir;
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(out.join('\n').toUtf8());
}

void Backend::resetLocalCache() {
    QDir(QDir(cacheRootStr()).filePath("thumbs")).removeRecursively();
    QDir(QDir(cacheRootStr()).filePath("colors_markers")).removeRecursively();
    QDir().mkpath(QDir(cacheRootStr()).filePath("thumbs"));
    QDir().mkpath(QDir(cacheRootStr()).filePath("colors_markers"));
}

void Backend::chooseWallpaperDir() {
    const QString chosen = QFileDialog::getExistingDirectory(
        nullptr, QStringLiteral("Selecciona la carpeta de wallpapers"), m_wallpaperDir);
    if (chosen.isEmpty())
        return;
    if (QFileInfo(chosen).absoluteFilePath() == QFileInfo(m_wallpaperDir).absoluteFilePath())
        return;
    m_wallpaperDir = QFileInfo(chosen).absoluteFilePath();
    persistConfig();
    resetLocalCache();
    setupWatchers();
    emit wallpaperDirChanged();
    emit localCacheReset();
    extractColors();
}

void Backend::loadMonitors() {
    const QStringList names = detectMonitors();
    QVariantList list;
    for (const QString &n : names)
        list << n;
    emit monitorsReady(list);
}

QStringList Backend::detectMonitors() const {
    struct Cmd { QStringList argv; };
    const QList<Cmd> cmds = {
        {{"niri", "msg", "outputs"}},
        {{"hyprctl", "monitors", "-j"}},
        {{"swaymsg", "-t", "get_outputs", "-r"}},
    };
    QSet<QString> seen;
    QStringList names;
    for (const Cmd &c : cmds) {
        QProcess p;
        p.start(c.argv.value(0), c.argv.mid(1));
        if (!p.waitForStarted(2000))
            continue;
        if (!p.waitForFinished(2000) || p.exitStatus() != QProcess::NormalExit)
            continue;
        const QByteArray out = p.readAllStandardOutput().trimmed();
        if (out.isEmpty())
            continue;
        const QJsonDocument doc = QJsonDocument::fromJson(out);
        if (!doc.isArray())
            continue;
        for (const QJsonValue &v : doc.array()) {
            const QString name = v.toObject().value("name").toString();
            if (!name.isEmpty() && !seen.contains(name)) {
                seen.insert(name);
                names << name;
            }
        }
        if (!names.isEmpty())
            break;
    }
    return names;
}

void Backend::applyWallpaper(const QString &safeName, bool isVideo, const QString &outputs) {
    const QString local = QDir(m_wallpaperDir).filePath(safeName);
    if (QFileInfo::exists(local)) {
        const QString name = safeName;
        if (isVideo) {
            applyVideo(local, outputs);
        } else {
            applyImage(local, outputs);
        }
        runWal(isVideo ? QDir(cacheRootStr()).filePath("thumbs/" + safeName) : local);
        emit wallpaperApplied(name);
        return;
    }
    const QString url = mapUrl(safeName);
    if (url.isEmpty()) {
        queueNotice(tr("No se encuentra el archivo: %1").arg(safeName));
        return;
    }
    m_pendingApplyName = safeName;
    m_pendingApplyVideo = isVideo;
    m_pendingApplyOutputs = outputs;
    setDownloading(true, safeName);
    downloadFull(safeName, url);
}

QString Backend::mapUrl(const QString &name) const {
    QFile f(QDir(cacheRootStr()).filePath("search_map.txt"));
    if (!f.open(QIODevice::ReadOnly))
        return {};
    for (const QString &line : QString::fromUtf8(f.readAll()).split('\n')) {
        const QString fname = line.section('|', 0, 0).trimmed();
        if (fname == name.trimmed())
            return line.section('|', 1).trimmed();
    }
    return {};
}

void Backend::downloadFull(const QString &safeName, const QString &url) {
    if (m_downloadReply)
        m_downloadReply->abort();
    auto *reply = m_nam->get(netReq(QUrl(url)));
    m_downloadReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, safeName] {
        if (reply == m_downloadReply)
            m_downloadReply = nullptr;
        if (reply->error() != QNetworkReply::NoError) {
            setDownloading(false);
            queueNotice(tr("Descarga fallida: %1").arg(reply->errorString()));
            reply->deleteLater();
            return;
        }
        const QByteArray data = reply->readAll();
        reply->deleteLater();

        const QString target = QDir(m_wallpaperDir).filePath(safeName);
        const QString tmp = target + ".tmp";
        QFile out(tmp);
        if (!out.open(QIODevice::WriteOnly)) {
            setDownloading(false);
            queueNotice(tr("Descarga fallida: no se pudo crear el archivo"));
            return;
        }
        if (data.startsWith("RIFF") && data.mid(8, 4) == "WEBP") {
            QImage img;
            img.loadFromData(data);
            if (img.isNull()) {
                out.write(data);
            } else {
                QImage rgb = img.convertToFormat(QImage::Format_RGB32);
                rgb.save(&out, "JPEG", 92);
            }
        } else {
            out.write(data);
        }
        out.close();
        QFile::remove(target);
        QFile::rename(tmp, target);

        thumbs::makeThumb(target, QDir(cacheRootStr()).filePath("thumbs/" + safeName));
        setDownloading(false);

        const QString name = safeName;
        if (m_pendingApplyVideo) {
            applyVideo(target, m_pendingApplyOutputs);
        } else {
            applyImage(target, m_pendingApplyOutputs);
        }
        runWal(m_pendingApplyVideo ? QDir(cacheRootStr()).filePath("thumbs/" + safeName) : target);
        emit wallpaperApplied(name);
    });
}

bool Backend::applyImage(const QString &path, const QString &outputs) {
    const Setter st = detectSetter();
    QStringList cmd;
    if (st.mode == "custom") {
        cmd = QProcess::splitCommand(configValue("wpaper_cmd").replace(QStringLiteral("%s"), path));
    } else if (st.mode == "sway" || st.mode == "feh" || st.mode == "nitrogen") {
        pkill(st.mode == "sway" ? "swaybg" : st.mode);
        if (st.mode == "sway")
            cmd = {st.exe, "-i", path, "-m", "fill"};
        else if (st.mode == "feh")
            cmd = {st.exe, "--bg-fill", path};
        else
            cmd = {st.exe, "--set-scaled", path};
    } else { // awww / swww
        const QString transition =
            TRANSITIONS.at(QRandomGenerator::global()->bounded(TRANSITIONS.size()));
        cmd = {st.exe, "img", path, "--transition-type", transition,
               "--transition-pos", "0.5,0.5", "--transition-fps", "144", "--transition-duration",
               "1"};
        if (outputs != "all" && !outputs.isEmpty())
            cmd << "-o" << outputs;
    }
    if (cmd.isEmpty()) {
        queueNotice(tr("No se detectó ningún setter (awww/swww/swaybg/feh/nitrogen)"));
        return false;
    }
    QProcess::startDetached(cmd.value(0), cmd.mid(1));
    QFile::remove(QDir(cacheRootStr()).filePath("current_wallpaper.png"));
    QFile::copy(path, QDir(cacheRootStr()).filePath("current_wallpaper.png"));
    return true;
}

bool Backend::applyVideo(const QString &path, const QString &outputs) {
    const QString mpvpaper = QStandardPaths::findExecutable("mpvpaper");
    if (mpvpaper.isEmpty()) {
        queueNotice(tr("mpvpaper no está instalado"));
        return false;
    }
    const QString opts =
        "loop --no-audio --hwdec=auto --video-sync=display-resample --interpolation --tscale=oversample";
    QStringList targets;
    if (outputs.isEmpty() || outputs == "all")
        targets << "*";
    else
        targets = outputs.split(',');
    for (const QString &mon : targets)
        QProcess::startDetached(mpvpaper, {"-o", opts, mon, path});
    return true;
}

void Backend::runWal(const QString &img) {
    QString wal = QStandardPaths::findExecutable("wal");
    QStringList cmd;
    const QString userCmd = configValue("wal_cmd");
    if (!userCmd.isEmpty()) {
        QString c = userCmd;
        cmd = QProcess::splitCommand(c.replace(QStringLiteral("%s"), img));
    } else if (!wal.isEmpty())
        cmd = {wal, "-i", img, "-q"};
    if (cmd.isEmpty())
        return;

    const QString logPath = QDir(cacheRootStr()).filePath("wal.log");
    auto *p = new QProcess(this);
    p->setStandardOutputFile(logPath, QIODevice::Append);
    p->setStandardErrorFile(logPath, QIODevice::Append);
    p->setProcessChannelMode(QProcess::SeparateChannels);
    connect(p, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, p](int, QProcess::ExitStatus) { p->deleteLater(); });
    {
        QFile f(logPath);
        if (f.open(QIODevice::Append)) {
            QTextStream ts(&f);
            ts << "\n[" << QTime::currentTime().toString("HH:mm:ss") << "] "
               << cmd.join(' ') << "\n";
        }
    }
    p->start(cmd.value(0), cmd.mid(1));
}

Backend::Setter Backend::detectSetter() const {
    const QString wanted = configValue("setter", "auto").toLower();
    if (!configValue("wpaper_cmd").isEmpty())
        return {QString(), "custom"};

    QStringList order;
    if (wanted != "auto")
        order << wanted;
    else
        order << QStringList{"awww", "swww", "swaybg", "feh", "nitrogen"};

    for (const QString &name : order) {
        const QString exe = QStandardPaths::findExecutable(name);
        if (exe.isEmpty())
            continue;
        if (name == "awww" || name == "swww") {
            if (ensureDaemon(exe))
                return {exe, "awww"};
        } else if (name == "swaybg") {
            return {exe, "sway"};
        } else if (name == "feh") {
            return {exe, "feh"};
        } else if (name == "nitrogen") {
            return {exe, "nitrogen"};
        }
    }
    return {QString(), QString()};
}

bool Backend::ensureDaemon(const QString &exe) const {
    QProcess pgrep;
    pgrep.start("pgrep", {"-f", "awww-daemon|swww-daemon"});
    if (pgrep.waitForStarted(2000) && pgrep.waitForFinished(2000))
        if (pgrep.exitCode() == 0)
            return true;
    QProcess::startDetached(exe, {"init"});
    return true;
}

void Backend::pkill(const QString &name) {
    if (name.isEmpty())
        return;
    QProcess::startDetached("pkill", {"-f", name});
}

void Backend::setSearchPaused(bool paused) { m_search->setPaused(paused); }

void Backend::search(const QString &query) {
    if (query.trimmed().isEmpty())
        return;
    m_search->start(query, QDir(cacheRootStr()).filePath("search_thumbs"),
                    QDir(cacheRootStr()).filePath("search_map.txt"));
}

void Backend::extractColors() {
    if (m_extracting.exchange(true))
        return;
    const QString src = m_wallpaperDir;
    const QString thumbsDir = QDir(cacheRootStr()).filePath("thumbs");
    const QString markersDir = QDir(cacheRootStr()).filePath("colors_markers");
    std::thread([this, src, thumbsDir, markersDir] {
        thumbs::extractWork(src, thumbsDir, markersDir);
        m_extracting.store(false);
    }).detach();
}

void Backend::shutdown() {
    m_search->stop();
    if (m_downloadReply)
        m_downloadReply->abort();
    clearNotice();
}
