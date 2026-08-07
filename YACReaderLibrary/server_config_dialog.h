#ifndef __SERVER_CONFIG_DIALOG_H
#define __SERVER_CONFIG_DIALOG_H

#include "themable.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

class QFrame;

class ServerConfigDialog : public QDialog, protected Themable
{
    Q_OBJECT
public:
    explicit ServerConfigDialog(QWidget *parent = nullptr);
    void showEvent(QShowEvent *event) override;

protected:
    void applyTheme(const Theme &theme) override;

private:
    QString webInterfaceUrl() const;
    void refreshWebInterface();
    void setConnectionControlsEnabled(bool enabled);

    QComboBox *ip;
    QLineEdit *port;
    QCheckBox *check;
    QPushButton *accept;
    QPushButton *copyLinkButton;
    QPushButton *openWebUiButton;
    QLabel *qrCode;
    QLabel *titleLabel;
    QLabel *descriptionLabel;
    QLabel *qrMessageLabel;
    QLabel *propagandaLabel;
    QLabel *ipLabel;
    QLabel *portLabel;
    QLabel *webInterfaceLabel;
    QLabel *webInterfaceUrlLabel;
    QLabel *backgroundDecoration;
    QFrame *connectionCard;
    QString propagandaText;

public slots:
    void generateQR();
    void generateQR(const QString &serverAddress);
    void regenerateQR(const QString &address);
    void enableServer(int status);
    void updatePort();

signals:
    void portChanged(QString port);
};
#endif
