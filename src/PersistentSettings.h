#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

class PersistentSettings : public QObject {
    Q_OBJECT
public:
    explicit PersistentSettings(QObject *parent = nullptr);

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool searched READ searched WRITE setSearched NOTIFY searchedChanged)
    Q_PROPERTY(QString lastName READ lastName WRITE setLastName NOTIFY lastNameChanged)

    QString query() const { return m_query; }
    bool searched() const { return m_searched; }
    QString lastName() const { return m_lastName; }

    void setQuery(const QString &v);
    void setSearched(bool v);
    void setLastName(const QString &v);

signals:
    void queryChanged();
    void searchedChanged();
    void lastNameChanged();

private:
    QSettings m_qs;
    QString m_query;
    bool m_searched = false;
    QString m_lastName;
};
