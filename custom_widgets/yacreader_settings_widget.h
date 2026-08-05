#ifndef YACREADER_SETTINGS_WIDGET_H
#define YACREADER_SETTINGS_WIDGET_H

#include <QIcon>
#include <QString>
#include <QWidget>

class QListWidget;
class QSplitter;
class QStackedWidget;

class YACReaderSettingsWidget : public QWidget
{
public:
    explicit YACReaderSettingsWidget(QWidget *parent = nullptr);

    int addPage(QWidget *page, const QString &title, const QIcon &icon = { });

private:
    void updateNavigationSize();

    QListWidget *navigation;
    QStackedWidget *pages;
    QSplitter *splitter;
};

#endif // YACREADER_SETTINGS_WIDGET_H
