#include "ThemeManager.h"
#include <QSettings>
#include <QGuiApplication>
#include <QStyleHints>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
    , m_theme("system")
    , m_isDarkMode(false)
    , m_primaryColor("#007AFF")
{
    loadSettings();
    if (m_theme == "system") {
        applySystemTheme();
    }
    updateColors();
}

QString ThemeManager::theme() const
{
    return m_theme;
}

void ThemeManager::setTheme(const QString &theme)
{
    if (m_theme != theme) {
        m_theme = theme;
        if (m_theme == "dark") {
            m_isDarkMode = true;
        } else if (m_theme == "light") {
            m_isDarkMode = false;
        } else if (m_theme == "system") {
            applySystemTheme();
        }
        saveSettings();
        emit themeChanged();
    }
}

bool ThemeManager::isDarkMode() const
{
    return m_isDarkMode;
}

void ThemeManager::setDarkMode(bool dark)
{
    if (m_isDarkMode != dark) {
        m_isDarkMode = dark;
        m_theme = dark ? "dark" : "light";
        saveSettings();
        emit themeChanged();
    }
}

QString ThemeManager::primaryColor() const
{
    return m_primaryColor;
}

void ThemeManager::setPrimaryColor(const QString &color)
{
    if (m_primaryColor != color) {
        m_primaryColor = color;
        saveSettings();
        emit themeChanged();
    }
}

QColor ThemeManager::backgroundColor() const
{
    return m_isDarkMode ? QColor("#1e1e1e") : QColor("#ffffff");
}

QColor ThemeManager::surfaceColor() const
{
    return m_isDarkMode ? QColor("#252526") : QColor("#f5f5f5");
}

QColor ThemeManager::textColor() const
{
    return m_isDarkMode ? QColor("#ffffff") : QColor("#1e1e1e");
}

QColor ThemeManager::textSecondaryColor() const
{
    return m_isDarkMode ? QColor("#cccccc") : QColor("#666666");
}

QColor ThemeManager::accentColor() const
{
    return QColor(m_primaryColor);
}

QColor ThemeManager::borderColor() const
{
    return m_isDarkMode ? QColor("#3f3f46") : QColor("#e0e0e0");
}

QColor ThemeManager::messageBubbleOwn() const
{
    return QColor(m_primaryColor);
}

QColor ThemeManager::messageBubbleOther() const
{
    return m_isDarkMode ? QColor("#2d2d30") : QColor("#e5e5ea");
}

QColor ThemeManager::successColor() const
{
    return QColor("#34c759");
}

QColor ThemeManager::warningColor() const
{
    return QColor("#ff9500");
}

QColor ThemeManager::errorColor() const
{
    return QColor("#ff3b30");
}

void ThemeManager::toggleTheme()
{
    if (m_theme == "system") {
        setDarkMode(!m_isDarkMode);
    } else {
        setDarkMode(!m_isDarkMode);
    }
}

void ThemeManager::applySystemTheme()
{
    if (m_theme == "system") {
        if (qApp) {
            QStyleHints *hints = qApp->styleHints();
            if (hints) {
                m_isDarkMode = hints->colorScheme() == Qt::ColorScheme::Dark;
            }
        }
        emit themeChanged();
    }
}

void ThemeManager::loadSettings()
{
    QSettings settings;
    m_theme = settings.value("theme/theme", "system").toString();
    m_primaryColor = settings.value("theme/primaryColor", "#007AFF").toString();
    m_isDarkMode = settings.value("theme/isDarkMode", false).toBool();
}

void ThemeManager::saveSettings()
{
    QSettings settings;
    settings.setValue("theme/theme", m_theme);
    settings.setValue("theme/primaryColor", m_primaryColor);
    settings.setValue("theme/isDarkMode", m_isDarkMode);
    settings.sync();
}

void ThemeManager::updateColors()
{
    // Colors are computed on demand via getters
}
