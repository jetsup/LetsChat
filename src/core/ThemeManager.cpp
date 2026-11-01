#include "ThemeManager.h"
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    m_theme = QSettings().value("app/theme", "system").toString();
    if (m_theme != "light" && m_theme != "dark")
        m_theme = "system";
    recompute();

    // Follow the operating system when the preference is "system".
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_theme == "system") {
            const bool before = m_isDark;
            recompute();
            if (before != m_isDark)
                emit themeChanged();
        }
    });
}

void ThemeManager::setTheme(const QString &theme)
{
    const QString t = (theme == "light" || theme == "dark") ? theme : QStringLiteral("system");
    if (t == m_theme)
        return;
    m_theme = t;
    QSettings().setValue("app/theme", m_theme);
    recompute();
    emit themeChanged();
}

void ThemeManager::recompute()
{
    if (m_theme == "dark")
        m_isDark = true;
    else if (m_theme == "light")
        m_isDark = false;
    else
        m_isDark = QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
}
