#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "readercontroller.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;
    ReaderController reader;
    engine.rootContext()->setContextProperty(QStringLiteral("reader"), &reader);

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Leaflit", "Main");

    return app.exec();
}
