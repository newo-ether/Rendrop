// drop_file_tip.h

#ifndef DROPFILETIP_H
#define DROPFILETIP_H

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

#endif // DROPFILETIP_H
