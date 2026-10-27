#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QColor>

class ThemeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(bool isDarkMode READ isDarkMode WRITE setDarkMode NOTIFY themeChanged)
    Q_PROPERTY(QString primaryColor READ primaryColor WRITE setPrimaryColor NOTIFY themeChanged)
    
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY themeChanged)
    Q_PROPERTY(QColor surfaceColor READ surfaceColor NOTIFY themeChanged)
    Q_PROPERTY(QColor textColor READ textColor NOTIFY themeChanged)
    Q_PROPERTY(QColor textSecondaryColor READ textSecondaryColor NOTIFY themeChanged)
    Q_PROPERTY(QColor accentColor READ accentColor NOTIFY themeChanged)
    Q_PROPERTY(QColor borderColor READ borderColor NOTIFY themeChanged)
    Q_PROPERTY(QColor messageBubbleOwn READ messageBubbleOwn NOTIFY themeChanged)
    Q_PROPERTY(QColor messageBubbleOther READ messageBubbleOther NOTIFY themeChanged)
    Q_PROPERTY(QColor successColor READ successColor NOTIFY themeChanged)
    Q_PROPERTY(QColor warningColor READ warningColor NOTIFY themeChanged)
    Q_PROPERTY(QColor errorColor READ errorColor NOTIFY themeChanged)

public:
    explicit ThemeManager(QObject *parent = nullptr);
    
    QString theme() const;
    void setTheme(const QString &theme);
    
    bool isDarkMode() const;
    void setDarkMode(bool dark);
    
    QString primaryColor() const;
    void setPrimaryColor(const QString &color);
    
    QColor backgroundColor() const;
    QColor surfaceColor() const;
    QColor textColor() const;
    QColor textSecondaryColor() const;
    QColor accentColor() const;
    QColor borderColor() const;
    QColor messageBubbleOwn() const;
    QColor messageBubbleOther() const;
    QColor successColor() const;
    QColor warningColor() const;
    QColor errorColor() const;
    
    Q_INVOKABLE void toggleTheme();
    Q_INVOKABLE void applySystemTheme();
    
signals:
    void themeChanged();
    
private:
    void loadSettings();
    void saveSettings();
    void updateColors();
    
    QString m_theme;
    bool m_isDarkMode;
    QString m_primaryColor;
};

#endif // THEMEMANAGER_H
