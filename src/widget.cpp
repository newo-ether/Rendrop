// widget.cpp

#include <QPushButton>
#include <vector>

#include "widget.h"
#include "ui_widget.h"

#include "file_bar.h"
#include "add_file_button.h"
#include "drop_shadow_renderer.h"

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::widget)
{
    ui->setupUi(this);

    dropShadowRenderer = new DropShadowRenderer();

    for (int i = 0; i < 100; i++)
    {
        newFileBar();
    }

    addFileButton = new AddFileButton(this, dropShadowRenderer);
    ui->scrollArea->installEventFilter(this);
}

Widget::~Widget()
{
    delete addFileButton;
    for (auto fileBar: fileBars) {
        delete fileBar;
    }
    delete ui;
    delete dropShadowRenderer;
}

bool Widget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == ui->scrollArea && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        QWidget *scrollArea = ui->scrollArea;
        int x = scrollArea->x() + scrollArea->width();
        int y = scrollArea->y() + scrollArea->height();
        addFileButton->move(x - 80, y - 80);
        addFileButton->raise();
    }
    return QWidget::eventFilter(watched, event);
}

void Widget::newFileBar()
{
    FileBar *fileBar = new FileBar(ui->scrollAreaWidgetContents, dropShadowRenderer);

    QVBoxLayout *layout = ui->scrollAreaVerticalLayout;
    layout->insertWidget(std::max(layout->count() - 1, 0), fileBar);

    fileBars.push_back(fileBar);
}
