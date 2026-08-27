#pragma once

#include <QFileSystemWatcher>
#include <QHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <atomic>

class Search;

class Backend : public QObject {
    Q_OBJECT
public:
    explicit Backend(QObject *parent = nullptr);

    Q_PROPERTY(QString notice READ notice NOTIFY noticeChanged)
    Q_PROPERTY(bool isDownloading READ isDownloading NOTIFY downloadStateChanged)
    Q_PROPERTY(QString downloadingName READ downloadingName NOTIFY downloadStateChanged)
    Q_PROPERTY(QString homeDir READ homeDir CONSTANT)
    Q_PROPERTY(QString thumbDir READ thumbDir CONSTANT)
    Q_PROPERTY(QString searchDir READ searchDir CONSTANT)
    Q_PROPERTY(QString markerDir READ markerDir CONSTANT)
    Q_PROPERTY(QString wallpaperDir READ wallpaperDir NOTIFY wallpaperDirChanged)

    QString notice() const { return m_notice; }
    bool isDownloading() const { return m_downloading; }
    QString downloadingName() const { return m_downloadingName; }
    QString homeDir() const;
    QString thumbDir() const;
    QString searchDir() const;
    QString markerDir() const;
    QString wallpaperDir() const { return m_wallpaperDir; }

public slots:
    void setNotice(const QString &msg);
    void setDownloading(bool value, const QString &name = {});
    void clearNotice();
    void ensureDirs();
    void loadMonitors();
    void applyWallpaper(const QString &safeName, bool isVideo, const QString &outputs);
    void chooseWallpaperDir();
    void setSearchPaused(bool paused);
    void search(const QString &query);
    void extractColors();
    void shutdown();

signals:
    void monitorsReady(const QVariantList &names);
    void wallpaperApplied(const QString &name);
    void noticeChanged();
    void downloadStateChanged();
    void wallpaperDirChanged();
    void localCacheReset();
    void thumbsRefreshed();
    void searchRefreshed();
    void markersRefreshed();
    void srcRefreshed();

private:
    void queueNotice(const QString &msg);
    void queueDownloading(bool value, const QString &name);
    void reloadConfig();
    QString configValue(const QString &key, const QString &fallback = {}) const;
    void setupWatchers();
    void persistConfig();
    void resetLocalCache();
    bool applyImage(const QString &path, const QString &outputs);
    bool applyVideo(const QString &path, const QString &outputs);
    void runWal(const QString &img);
    QString mapUrl(const QString &name) const;
    void downloadFull(const QString &safeName, const QString &url);
    QStringList detectMonitors() const;
    struct Setter { QString exe; QString mode; };
    Setter detectSetter() const;
    bool ensureDaemon(const QString &exe) const;
    static void pkill(const QString &name);

    QNetworkAccessManager *m_nam;
    Search *m_search;
    QHash<QString, QString> m_cfg;
    QString m_wallpaperDir;
    QString m_notice;
    bool m_downloading = false;
    QString m_downloadingName;
    QTimer m_noticeTimer;
    QFileSystemWatcher m_watcher;
    std::atomic<bool> m_extracting{false};
    QPointer<QNetworkReply> m_downloadReply;
    QString m_pendingApplyName;
    bool m_pendingApplyVideo = false;
    QString m_pendingApplyOutputs;
};
