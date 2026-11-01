#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>

/** Theme preference: "system" (follows the OS), "light" or "dark". */
class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool isDarkMode READ isDarkMode NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);

    QString theme() const { return m_theme; }
    void setTheme(const QString &theme);
    bool isDarkMode() const { return m_isDark; }

signals:
    void themeChanged();

private:
    void recompute();

    QString m_theme = QStringLiteral("system");
    bool m_isDark = false;
};

#endif // THEMEMANAGER_H
