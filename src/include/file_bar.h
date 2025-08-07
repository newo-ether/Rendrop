// file_bar.h

#ifndef FILEBAR_H
#define FILEBAR_H

#include <QWidget>
#include <QString>
#include <QTimer>
#include <QElapsedTimer>

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class fileBar;
}
QT_END_NAMESPACE

class FileBar : public QWidget
{
    Q_OBJECT
public:
    FileBar(
        QWidget *parent,
        DropShadowRenderer *dropShadowRenderer,
        unsigned int id
    );
    ~FileBar();

    void setFileName(QString fileName);
    QString getFileName() const;
    DropShadowWidget *getDropShadowWidget() const;
    void setID(unsigned int id);
    unsigned int getID() const;
    void setProgressBar(float value);

private:
    Ui::fileBar *ui;
    DropShadowWidget *dropShadowWidget;
    QTimer *timer;
    QElapsedTimer *elapsedTimer;
    qint64 lastElapsed;
    float value, targetValue, velocity;
    float stiffness, damping;
    unsigned int id;

signals:
    void upButtonClicked(FileBar *fileBar);
    void downButtonClicked(FileBar *fileBar);
    void deleteButtonClicked(FileBar *fileBar);

private slots:
    void onUpButtonClicked();
    void onDownButtonClicked();
    void onDeleteButtonClicked();
    void updateProgressBar();
};

#endif // FILEBAR_H
