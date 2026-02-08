#ifndef FRAME_RANGE_DIALOG_H
#define FRAME_RANGE_DIALOG_H

#include <QDialog>

namespace Ui {
class FrameRangeDialog;
}

class FrameRangeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit FrameRangeDialog(int start, int end, int step, QWidget *parent = nullptr);
    ~FrameRangeDialog();

    int getStart() const;
    int getEnd() const;
    int getStep() const;

private:
    Ui::FrameRangeDialog *ui;
};

#endif // FRAME_RANGE_DIALOG_H