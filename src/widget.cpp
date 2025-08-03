// widget.cpp

#include <QPushButton>
#include <QString>
#include <vector>

#include "widget.h"
#include "ui_widget.h"

#include "file_bar.h"
#include "add_file_button.h"

#include "drop_shadow_widget.h"
#include "drop_shadow_renderer.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::widget)
{
    ui->setupUi(this);

    dropShadowRenderer = new DropShadowRenderer();

    for (int i = 0; i < 100; i++)
    {
        newFileBar("File_" + QString::number(i));
    }

    addFileButton = new AddFileButton(ui->ContentWidget, dropShadowRenderer);
    ui->scrollArea->installEventFilter(this);

    ui->scrollAreaSizeWidget->setAttribute(Qt::WA_TransparentForMouseEvents);
    ui->scrollAreaSizeWidget->installEventFilter(this);

    createDropShadowWidget(ui->ContentWidget, ui->infoWidget, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->scrollArea, 12, 4, 4, 0.3f, 15);
    createDropShadowWidget(ui->ContentWidget, ui->renderButton, 6, 4, 4, 0.3f, 10);
}

Widget::~Widget()
{
    for (auto dropShadowWidget : dropShadowWidgets)
    {
        delete dropShadowWidget;
    }

    delete addFileButton;

    for (auto fileBar: fileBars)
    {
        delete fileBar;
    }

    delete ui;
    delete dropShadowRenderer;
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollArea && event->type() == QEvent::Resize)
    {
        QWidget *scrollArea = ui->scrollArea;
        int x = scrollArea->x() + scrollArea->width();
        int y = scrollArea->y() + scrollArea->height();
        addFileButton->move(x - 100, y - 100);
        addFileButton->raise();
    }

    if (watched == ui->scrollAreaSizeWidget && event->type() == QEvent::Resize)
    {
        float borderRadius = 16;
        float offsetX = 2;
        float offsetY = 2;
        float alphaMax = 0.3f;
        float blurRadius = 10;

        int fileBarWidth = ui->scrollAreaSizeWidget->width();
        int fileBarHeight = 50;

        float marginX = std::abs(offsetX) + blurRadius * 0.5f;
        float marginY = std::abs(offsetY) + blurRadius * 0.5f;

        fileBarShadowImage = dropShadowRenderer->render(
            fileBarWidth + marginX * 2,
            fileBarHeight + marginY * 2,
            borderRadius,
            offsetX,
            offsetY,
            alphaMax,
            blurRadius
        );
    }

    return QWidget::eventFilter(watched, event);
}

void Widget::newFileBar(QString filename)
{
    FileBar *fileBar = new FileBar(ui->scrollAreaWidgetContents, dropShadowRenderer);
    fileBar->setFilename(filename);

    DropShadowWidget *fileBarShadow = fileBar->getDropShadowWidget();
    fileBarShadow->setShadowUpdateEnabled(false);
    fileBarShadow->setShadowImage(&fileBarShadowImage);

    QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
    layout->insertWidget(std::max(layout->count() - 2, 0), fileBar);

    fileBars.push_back(fileBar);
}

void Widget::createDropShadowWidget(
    QWidget *parent,
    QWidget *target,
    float borderRadius,
    float offsetX,
    float offsetY,
    float alphaMax,
    float blurRadius
) {
    DropShadowWidget *dropShadowWidget = new DropShadowWidget(parent, target, dropShadowRenderer);
    dropShadowWidget->setBorderRadius(borderRadius);
    dropShadowWidget->setOffsetX(offsetX);
    dropShadowWidget->setOffsetY(offsetY);
    dropShadowWidget->setAlphaMax(alphaMax);
    dropShadowWidget->setBlurRadius(blurRadius);

    dropShadowWidgets.push_back(dropShadowWidget);
}
