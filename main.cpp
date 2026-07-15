#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QVariant>
#include <QVariantMap>

#include "reader/readingdocumentformatter.h"
#include "readercontroller.h"
#include "storage/localstatestore.h"

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCoreApplication::setOrganizationName(QStringLiteral("Leaflit"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("leaflit.local"));
    QCoreApplication::setApplicationName(QStringLiteral("Leaflit"));

    QGuiApplication app(argc, argv);

    LocalStateStore localState;
    ReaderController reader;
    ReadingDocumentFormatter documentFormatter;

    QObject::connect(&reader,
                     &ReaderController::documentOpened,
                     &localState,
                     [&localState](const QUrl &sourceUrl) {
                         if (!sourceUrl.isEmpty()) {
                             localState.setLastBookUrl(sourceUrl);
                         }
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &localState, &LocalStateStore::sync);

    if (!localState.lastBookUrl().isEmpty()) {
        reader.openFile(localState.lastBookUrl());
    }

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("readerController"),
         QVariant::fromValue(static_cast<QObject *>(&reader))},
        {QStringLiteral("localStateStore"),
         QVariant::fromValue(static_cast<QObject *>(&localState))},
        {QStringLiteral("readingDocumentFormatter"),
         QVariant::fromValue(static_cast<QObject *>(&documentFormatter))}
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("Leaflit", "Main");

    return app.exec();
}
