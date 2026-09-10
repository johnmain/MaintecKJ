#include <QApplication>
#include <QLabel>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qDebug() << "Qt Widgets application starting";
    
    QLabel label("Hello Qt Widgets!");
    label.show();
    
    qDebug() << "Label shown";
    return app.exec();
}