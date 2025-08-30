// drop_file_tip.h

#ifndef DROP_FILE_TIP_H
#define DROP_FILE_TIP_H

#include <QWidget>
#include <QString>

namespace Ui {
class DropFileTip;
}

class DropFileTip : public QWidget
{
    Q_OBJECT

public:
    DropFileTip(QWidget *parent = nullptr);
    ~DropFileTip();

    void changeTip(QString text);

private:
    Ui::DropFileTip *ui;
};

#endif // DROP_FILE_TIP_H
