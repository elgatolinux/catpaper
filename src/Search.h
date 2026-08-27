#pragma once

#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QPointer>
#include <QString>

class Search : public QObject {
    Q_OBJECT
public:
    explicit Search(QObject *parent = nullptr);

    void start(const QString &query, const QString &outDir, const QString &mapFile,
               int maxResults = 24);
    void setPaused(bool paused);
    void stop();
    bool running() const { return m_running; }

signals:
    void errorOccurred(const QString &message);

private:
    void fetchVqd();
    void fetchResults(const QString &vqd);
    void startNext();
    void downloadThumb(const QString &name, const QString &thumbUrl, const QString &imageUrl);
    void scheduleNext(int delayMs);
    void appendMap(const QString &name, const QString &imageUrl);
    void finish();

    QNetworkAccessManager m_nam;
    QString m_query;
    QString m_outDir;
    QString m_mapFile;
    int m_maxResults = 24;
    int m_index = 0;
    QJsonArray m_results;
    bool m_paused = false;
    bool m_stopped = true;
    bool m_running = false;
    QVector<QPointer<QNetworkReply>> m_replies;
};
