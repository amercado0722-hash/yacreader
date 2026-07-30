#ifndef SEARCH_SYNTAX_DIALOG_H
#define SEARCH_SYNTAX_DIALOG_H

#include <QDialog>

class SearchSyntaxDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SearchSyntaxDialog(QWidget *parent = nullptr);

private:
    QWidget *createQuickGuideTab();
    QWidget *createFieldsTab();
    QWidget *createExamplesTab();
};

#endif // SEARCH_SYNTAX_DIALOG_H
