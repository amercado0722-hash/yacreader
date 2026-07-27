#ifndef ORGANIZE_FILES_DIALOG_H
#define ORGANIZE_FILES_DIALOG_H

#include <QDialog>

class QLineEdit;
class QLabel;
class QCheckBox;
class QSettings;

class OrganizeFilesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit OrganizeFilesDialog(const QString &libraryRoot,
                                 const QString &selectedFolderPath,
                                 QSettings *settings = nullptr,
                                 QWidget *parent = nullptr);

    QString formatPattern() const;

    bool relativeToRoot() const;

    static QString defaultPattern();

    static QString buildRelativePath(const QString &pattern,
                                     const QString &publisher,
                                     const QString &series,
                                     const QString &number,
                                     const QString &title,
                                     const QString &volume,
                                     const QString &year,
                                     const QString &extension,
                                     int numberPadding = 0);

    static QString sanitizeSegment(QString segment);

    static QString padNumber(const QString &number, int width);

private slots:
    void updatePreview();

private:
    QLineEdit *patternEdit;
    QLabel *previewLabel;
    QCheckBox *relativeToRootCheck;

    QString libraryRoot;
    QString selectedFolderPath;
    QSettings *settings;

    void setupUI();
};

#endif // ORGANIZE_FILES_DIALOG_H
