#include "Search.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

static const char *const UA =
    "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 "
    "(KHTML, like Gecko) Chrome/126.0 Safari/537.36";

static QNetworkRequest req(const QUrl &url) {
    QNetworkRequest r(url);
    r.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(UA));
    r.setTransferTimeout(20000);
    return r;
}

Search::Search(QObject *parent) : QObject(parent) {}

void Search::start(const QString &query, const QString &outDir, const QString &mapFile,
                   int maxResults) {
    stop();
    m_query = query;
    m_outDir = outDir;
    m_mapFile = mapFile;
    m_maxResults = maxResults;
    m_index = 0;
    m_results = {};
    m_stopped = false;
    m_paused = false;
    m_running = true;

    QDir out(m_outDir);
    out.mkpath(".");
    const QStringList stale = out.entryList({"ddg_*"}, QDir::Files);
    for (const QString &f : stale)
        QFile::remove(out.filePath(f));

    QFile map(m_mapFile);
    if (map.open(QIODevice::WriteOnly | QIODevice::Truncate))
        map.close();

    fetchVqd();
}

void Search::stop() {
    m_stopped = true;
    m_running = false;
    for (auto &r : m_replies) {
        if (r)
            r->abort();
    }
    m_replies.clear();
}

void Search::setPaused(bool paused) { m_paused = paused; }

void Search::fetchVqd() {
    QUrl url(QStringLiteral("https://duckduckgo.com/"));
    QUrlQuery qq;
    qq.addQueryItem("q", m_query);
    qq.addQueryItem("iax", "images");
    qq.addQueryItem("ia", "images");
    url.setQuery(qq);

    auto *reply = m_nam.get(req(url));
    m_replies.append(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_replies.removeAll(reply);
        if (m_stopped) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("No se obtuvo la página de búsqueda"));
            finish();
            reply->deleteLater();
            return;
        }
        const QString html = QString::fromUtf8(reply->readAll());
        QRegularExpression re(QStringLiteral("\"vqd\":\"([^\"]+)\"|vqd=([\\d-]+)"));
        const QRegularExpressionMatch m = re.match(html);
        if (!m.hasMatch()) {
            emit errorOccurred(tr("No se obtuvo vqd de DuckDuckGo"));
            finish();
            reply->deleteLater();
            return;
        }
        const QString vqd = m.captured(1).isEmpty() ? m.captured(2) : m.captured(1);
        reply->deleteLater();
        fetchResults(vqd);
    });
}

void Search::fetchResults(const QString &vqd) {
    QUrl url(QStringLiteral("https://duckduckgo.com/i.js"));
    QUrlQuery qq;
    qq.addQueryItem("l", "us-en");
    qq.addQueryItem("o", "json");
    qq.addQueryItem("q", m_query);
    qq.addQueryItem("vqd", vqd);
    qq.addQueryItem("f", ",,,");
    qq.addQueryItem("p", "1");
    url.setQuery(qq);

    auto *reply = m_nam.get(req(url));
    m_replies.append(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply] {
        m_replies.removeAll(reply);
        if (m_stopped) {
            reply->deleteLater();
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            emit errorOccurred(tr("Búsqueda: error de red"));
            finish();
            reply->deleteLater();
            return;
        }
        const QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        const QJsonArray all = doc.object().value("results").toArray();
        m_results = QJsonArray();
        for (int i = 0; i < all.size() && i < m_maxResults; ++i)
            m_results.append(all.at(i));
        reply->deleteLater();
        if (m_results.isEmpty()) {
            emit errorOccurred(tr("Sin resultados"));
            finish();
            return;
        }
        startNext();
    });
}

void Search::startNext() {
    if (m_stopped)
        return;
    if (m_paused) {
        scheduleNext(200);
        return;
    }
    if (m_index >= m_results.size()) {
        finish();
        return;
    }
    const QJsonObject item = m_results.at(m_index).toObject();
    const QString name = QString("ddg_%1.jpg").arg(m_index, 4, 10, QChar('0'));
    const QString thumbUrl = item.value("thumbnail").toString();
    const QString imageUrl = item.value("image").toString();

    if (thumbUrl.isEmpty()) {
        ++m_index;
        startNext();
        return;
    }

    const QString path = QDir(m_outDir).filePath(name);
    if (QFile::exists(path)) {
        appendMap(name, imageUrl);
        ++m_index;
        startNext();
        return;
    }
    downloadThumb(name, thumbUrl, imageUrl);
}

void Search::downloadThumb(const QString &name, const QString &thumbUrl,
                           const QString &imageUrl) {
    auto *reply = m_nam.get(req(QUrl(thumbUrl)));
    m_replies.append(reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, name, imageUrl] {
        m_replies.removeAll(reply);
        if (m_stopped) {
            reply->deleteLater();
            return;
        }
        if (reply->error() == QNetworkReply::NoError) {
            QFile f(QDir(m_outDir).filePath(name));
            if (f.open(QIODevice::WriteOnly)) {
                f.write(reply->readAll());
                f.close();
                appendMap(name, imageUrl);
            }
        }
        ++m_index;
        reply->deleteLater();
        startNext();
    });
}

void Search::appendMap(const QString &name, const QString &imageUrl) {
    QFile f(m_mapFile);
    if (!f.open(QIODevice::Append | QIODevice::WriteOnly))
        return;
    f.write(QString("%1|%2\n").arg(name, imageUrl).toUtf8());
}

void Search::scheduleNext(int delayMs) {
    QTimer::singleShot(delayMs, this, [this] {
        if (!m_stopped)
            startNext();
    });
}

void Search::finish() {
    m_running = false;
    const bool wasPaused = m_paused;
    m_paused = false;
    Q_UNUSED(wasPaused);
}
