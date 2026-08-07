#ifndef YACREADER_SETTINGS_WIDGET_H
#define YACREADER_SETTINGS_WIDGET_H

#include <QIcon>
#include <QSize>
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

    QSize sizeHint() const override;

private:
    int preferredNavigationWidth() const;
    int preferredContentWidth() const;
    void updateNavigationSize();

    QListWidget *navigation;
    QStackedWidget *pages;
    QSplitter *splitter;
};

#endif // YACREADER_SETTINGS_WIDGET_H
