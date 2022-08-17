#include "about.h"
#include "ui_about.h"

#include "cmake.h"
#include "global.h"

#include <QAbstractButton>
#include <QIcon>

About::About(const QString &FIOVersion, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::About)
{
    ui->setupUi(this);

    ui->label_Version->setText(qApp->applicationVersion());
    ui->label_FIO->setText(FIOVersion);

    ui->label_Icon->setPixmap(QIcon::fromTheme(QStringLiteral("kdiskmark")).pixmap(128, 128));
}

About::~About()
{
    delete ui;
}

void About::on_buttonBox_clicked(QAbstractButton *)
{
    close();
}
