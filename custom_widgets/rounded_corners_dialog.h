#ifndef ROUNDEDCORNERSDIALOG_H
#define ROUNDEDCORNERSDIALOG_H

#include <QColor>
#include <QDialog>

class QWidget;

namespace YACReader {
class RoundedCornersDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RoundedCornersDialog(QWidget *parent = nullptr);

protected:
    QWidget *dialogSurface() const;
    void setContentFixedSize(const QSize &size);
    void setBackgroundColor(const QColor &color);

private:
    QWidget *m_dialogSurface;
};
}

#endif // ROUNDEDCORNERSDIALOG_H
