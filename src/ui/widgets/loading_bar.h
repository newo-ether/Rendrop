// loading_bar.h

#ifndef LOADING_BAR_H
#define LOADING_BAR_H

#include <QWidget>
#include <QTimer>
#include <QElapsedTimer>
#include <QPaintEvent>
#include <QShowEvent>
#include <QHideEvent>

class LoadingBar : public QWidget
{
    Q_OBJECT

public:
    LoadingBar(QWidget *parent = nullptr);
    ~LoadingBar();

protected:
    void paintEvent(QPaintEvent *) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private slots:
    void updateProgress();

private:
    QTimer *timer;
    QElapsedTimer *elapsedTimer;
    qint64 lastElapsed;
    float progress;
};

#endif // LOADING_BAR_H
