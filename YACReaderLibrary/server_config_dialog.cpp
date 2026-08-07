#include "server_config_dialog.h"

#include "ip_config_helper.h"
#include "qrcodegen.hpp"
#include "yacreader_global_gui.h"
#include "yacreader_http_server.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QPainter>
#include <QPalette>
#include <QSettings>
#include <QSignalBlocker>
#include <QUrl>
#include <QVBoxLayout>

extern YACReaderHttpServer *httpServer;

ServerConfigDialog::ServerConfigDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Server connectivity"));
    setFixedSize(770, 595);

    backgroundDecoration = new QLabel(this);
    backgroundDecoration->setGeometry(0, 0, 329, 595);
    backgroundDecoration->setScaledContents(true);

    qrCode = new QLabel(this);
    qrCode->setGeometry(64, 112, 200, 200);
    qrCode->setScaledContents(true);

    qrMessageLabel = new QLabel(tr("Scan to connect"), this);
    qrMessageLabel->setGeometry(43, 312, 243, 208);
    qrMessageLabel->setAlignment(Qt::AlignCenter);
    qrMessageLabel->setWordWrap(true);

    auto detailsWidget = new QWidget(this);
    detailsWidget->setGeometry(332, 0, 410, 595);

    auto detailsLayout = new QVBoxLayout(detailsWidget);
    detailsLayout->setContentsMargins(0, 48, 0, 43);
    detailsLayout->setSpacing(0);

    titleLabel = new QLabel(tr("Server connectivity"), detailsWidget);
    detailsLayout->addWidget(titleLabel);
    detailsLayout->addSpacing(7);

    descriptionLabel = new QLabel(tr("Devices on this network can reach your library at the address below."), detailsWidget);
    descriptionLabel->setWordWrap(true);
    detailsLayout->addWidget(descriptionLabel);
    detailsLayout->addSpacing(25);

    auto formLayout = new QGridLayout;
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(0);
    formLayout->setVerticalSpacing(7);
    formLayout->setColumnStretch(0, 9);
    formLayout->setColumnMinimumWidth(1, 16);
    formLayout->setColumnStretch(2, 4);
    formLayout->setColumnMinimumWidth(3, 7);

    ipLabel = new QLabel(tr("IP address"), detailsWidget);
    portLabel = new QLabel(tr("Port"), detailsWidget);
    formLayout->addWidget(ipLabel, 0, 0);
    formLayout->addWidget(portLabel, 0, 2);

    ip = new QComboBox(detailsWidget);
    ip->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ip->setMinimumWidth(100);
    port = new QLineEdit(QStringLiteral("8080"), detailsWidget);
    port->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    port->setMinimumWidth(55);
    port->setValidator(new QIntValidator(1024, 65535, this));
    accept = new QPushButton(tr("Set port"), detailsWidget);
    accept->setObjectName(QStringLiteral("primaryButton"));
    accept->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);

    formLayout->addWidget(ip, 1, 0);
    formLayout->addWidget(port, 1, 2);
    formLayout->addWidget(accept, 1, 4);
    detailsLayout->addLayout(formLayout);
    detailsLayout->addSpacing(26);

    connectionCard = new QFrame(detailsWidget);
    connectionCard->setObjectName(QStringLiteral("connectionCard"));
    auto cardLayout = new QVBoxLayout(connectionCard);
    cardLayout->setContentsMargins(16, 14, 16, 14);
    cardLayout->setSpacing(5);

    webInterfaceLabel = new QLabel(tr("Web interface").toUpper(), connectionCard);
    webInterfaceLabel->setObjectName(QStringLiteral("sectionLabel"));
    cardLayout->addWidget(webInterfaceLabel);

    webInterfaceUrlLabel = new QLabel(connectionCard);
    webInterfaceUrlLabel->setObjectName(QStringLiteral("webInterfaceUrl"));
    webInterfaceUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(webInterfaceUrlLabel);
    cardLayout->addSpacing(7);

    auto buttonsLayout = new QHBoxLayout;
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(8);
    buttonsLayout->addStretch();

    copyLinkButton = new QPushButton(tr("Copy link"), connectionCard);
    copyLinkButton->setObjectName(QStringLiteral("secondaryButton"));
    openWebUiButton = new QPushButton(tr("Open web UI"), connectionCard);
    openWebUiButton->setObjectName(QStringLiteral("primaryButton"));
    buttonsLayout->addWidget(copyLinkButton);
    buttonsLayout->addWidget(openWebUiButton);
    cardLayout->addLayout(buttonsLayout);
    detailsLayout->addWidget(connectionCard);
    detailsLayout->addSpacing(25);

    check = new QCheckBox(tr("Enable the server"), detailsWidget);
    detailsLayout->addWidget(check);
    detailsLayout->addStretch();

    propagandaText = tr("YACReader is available for iOS and Android. Discover it for <a href='https://ios.yacreader.com'>iOS</a> or <a href='https://android.yacreader.com'>Android</a>.");
    propagandaLabel = new QLabel(detailsWidget);
    propagandaLabel->setOpenExternalLinks(true);
    propagandaLabel->setWordWrap(true);
    detailsLayout->addWidget(propagandaLabel);

    connect(ip, &QComboBox::currentTextChanged, this, &ServerConfigDialog::regenerateQR);
    connect(port, &QLineEdit::textChanged, this, [this] {
        accept->setEnabled(check->isChecked() && port->hasAcceptableInput() && port->text() != httpServer->getPort());
    });
    connect(accept, &QPushButton::clicked, this, &ServerConfigDialog::updatePort);
    connect(copyLinkButton, &QPushButton::clicked, this, [this] {
        QApplication::clipboard()->setText(webInterfaceUrl());
    });
    connect(openWebUiButton, &QPushButton::clicked, this, [this] {
        QDesktopServices::openUrl(QUrl(webInterfaceUrl()));
    });

    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("libraryConfig"));
    const bool serverEnabled = settings.value(SERVER_ON, true).toBool();
    settings.endGroup();

    initTheme(this);
    accept->setMinimumWidth(qMax(102, accept->sizeHint().width()));

    check->setChecked(serverEnabled);
    connect(check, &QCheckBox::stateChanged, this, &ServerConfigDialog::enableServer);
    setConnectionControlsEnabled(serverEnabled);
    if (serverEnabled)
        generateQR();
    else {
        qrCode->clear();
        refreshWebInterface();
    }
}

void ServerConfigDialog::applyTheme(const Theme &theme)
{
    setStyleSheet(theme.serverConfigDialog.dialogQSS);
    titleLabel->setStyleSheet(theme.serverConfigDialog.titleLabelQSS);
    qrMessageLabel->setStyleSheet(theme.serverConfigDialog.qrMessageLabelQSS);
    propagandaLabel->setStyleSheet(theme.serverConfigDialog.propagandaLabelQSS);
    const QColor linkColor = theme.serverConfigDialog.linkColor;
    QString themedPropagandaText = propagandaText;
    themedPropagandaText.replace(QStringLiteral("<a "), QStringLiteral("<a style='color:%1' ").arg(linkColor.name()));
    propagandaLabel->setText(themedPropagandaText);
    auto propagandaPalette = propagandaLabel->palette();
    propagandaPalette.setColor(QPalette::Link, linkColor);
    propagandaPalette.setColor(QPalette::LinkVisited, linkColor);
    propagandaLabel->setPalette(propagandaPalette);
    descriptionLabel->setStyleSheet(theme.serverConfigDialog.textLabelQSS);
    ipLabel->setStyleSheet(theme.serverConfigDialog.secondaryLabelQSS);
    portLabel->setStyleSheet(theme.serverConfigDialog.secondaryLabelQSS);
    check->setStyleSheet(theme.serverConfigDialog.checkBoxQSS);

    backgroundDecoration->setPixmap(theme.serverConfigDialog.backgroundDecoration);
    backgroundDecoration->lower();

    generateQR();
}

void ServerConfigDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    generateQR();
}

void ServerConfigDialog::enableServer(int status)
{
    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("libraryConfig"));

    const bool enabled = status == Qt::Checked;
    setConnectionControlsEnabled(enabled);
    if (enabled) {
        httpServer->start();
        generateQR();
    } else {
        httpServer->stop();
        qrCode->clear();
        refreshWebInterface();
    }
    settings.setValue(SERVER_ON, enabled);
    settings.endGroup();
}

void ServerConfigDialog::generateQR()
{
    if (!httpServer->isRunning()) {
        refreshWebInterface();
        return;
    }

    const QSignalBlocker blocker(ip);
    const auto addresses = getIpAddresses();
    ip->clear();
    ip->addItems(addresses);
    port->setText(httpServer->getPort());

    if (!addresses.isEmpty())
        generateQR(addresses.first() + ":" + httpServer->getPort());
    else
        refreshWebInterface();
}

void ServerConfigDialog::generateQR(const QString &serverAddress)
{
    qrCode->clear();

    const auto backgroundColor = theme.serverConfigDialog.qrBackgroundColor;
    const auto foregroundColor = theme.serverConfigDialog.qrForegroundColor;
    const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(
            serverAddress.toLocal8Bit(),
            qrcodegen::QrCode::Ecc::LOW);

    const int qrSize = code.getSize();
    QPixmap qrPixmap(qrSize, qrSize);
    qrPixmap.fill(backgroundColor);

    QPainter painter(&qrPixmap);
    painter.setPen(foregroundColor);
    painter.setBrush(foregroundColor);
    for (int x = 0; x < qrSize; ++x) {
        for (int y = 0; y < qrSize; ++y) {
            if (code.getModule(x, y))
                painter.drawPoint(x, y);
        }
    }
    painter.end();

    qrPixmap = qrPixmap.scaled(qrCode->size() * devicePixelRatioF(), Qt::KeepAspectRatio, Qt::FastTransformation);
    qrPixmap.setDevicePixelRatio(devicePixelRatioF());
    qrCode->setPixmap(qrPixmap);
    refreshWebInterface();
}

void ServerConfigDialog::regenerateQR(const QString &address)
{
    if (httpServer->isRunning() && !address.isEmpty())
        generateQR(address + ":" + httpServer->getPort());
    else
        refreshWebInterface();
}

void ServerConfigDialog::updatePort()
{
    if (!port->hasAcceptableInput()) {
        port->setText(httpServer->getPort());
        return;
    }
    if (port->text() == httpServer->getPort())
        return;

    QSettings settings(YACReader::getSettingsPath() + "/YACReaderLibrary.ini", QSettings::IniFormat);
    settings.beginGroup(QStringLiteral("listener"));
    settings.setValue(QStringLiteral("port"), port->text().toInt());
    settings.endGroup();

    httpServer->stop();
    httpServer->start();
    generateQR();
}

QString ServerConfigDialog::webInterfaceUrl() const
{
    if (ip->currentText().isEmpty())
        return { };
    return QStringLiteral("http://%1:%2/webui").arg(ip->currentText(), httpServer->getPort());
}

void ServerConfigDialog::refreshWebInterface()
{
    const QString url = httpServer->isRunning() ? webInterfaceUrl() : QString();
    webInterfaceUrlLabel->setText(url.isEmpty() ? QStringLiteral("—") : url);
    copyLinkButton->setEnabled(!url.isEmpty());
    openWebUiButton->setEnabled(!url.isEmpty());
}

void ServerConfigDialog::setConnectionControlsEnabled(bool enabled)
{
    ip->setEnabled(enabled);
    port->setEnabled(enabled);
    accept->setEnabled(enabled && port->hasAcceptableInput() && port->text() != httpServer->getPort());
    if (!enabled) {
        copyLinkButton->setEnabled(false);
        openWebUiButton->setEnabled(false);
    }
}
