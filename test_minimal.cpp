#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QDebug>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    qDebug() << "Application created";
    
    QQmlApplicationEngine engine;
    qDebug() << "Engine created";
    
    engine.load(QUrl("qrc:/MaintecKJ/Main.qml"));
    qDebug() << "Load attempted";
    
    if (engine.rootObjects().isEmpty()) {
        qDebug() << "Failed to load QML";
        return -1;
    }
    
    qDebug() << "QML loaded successfully";
    return app.exec();
}