#include "PersistentSettings.h"

PersistentSettings::PersistentSettings(QObject *parent)
    : QObject(parent), m_qs(QStringLiteral("catpaper"), QStringLiteral("WallpaperPicker")) {
    m_query = m_qs.value("query", "").toString();
    m_searched = m_qs.value("searched", false).toBool();
    m_lastName = m_qs.value("lastName", "").toString();
}

void PersistentSettings::setQuery(const QString &v) {
    if (v == m_query)
        return;
    m_query = v;
    m_qs.setValue("query", v);
    emit queryChanged();
}

void PersistentSettings::setSearched(bool v) {
    if (v == m_searched)
        return;
    m_searched = v;
    m_qs.setValue("searched", v);
    emit searchedChanged();
}

void PersistentSettings::setLastName(const QString &v) {
    if (v == m_lastName)
        return;
    m_lastName = v;
    m_qs.setValue("lastName", v);
    emit lastNameChanged();
}
