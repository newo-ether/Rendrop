// drop_file_tip.h

#ifndef DROP_FILE_TIP_H
#define DROP_FILE_TIP_H

#include <QWidget>

namespace Ui {
class DropFileTip;
}

class DropFileTip : public QWidget
{
    Q_OBJECT

public:
    explicit DropFileTip(QWidget *parent = nullptr);
    ~DropFileTip();

private:
    Ui::DropFileTip *ui;
};

#endif // DROP_FILE_TIP_H
