#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include <QVariant>
#include <QVariantMap>
#include <QtMath>

#include "library/librarymodel.h"
#include "reader/readingsearchcontroller.h"
#include "reader/readingdocumentformatter.h"
#include "readercontroller.h"
#include "storage/localstatestore.h"
#include "storage/readingannotationstore.h"

namespace {

constexpr int normalWheelScrollLines = 6;

int wheelScrollLinesForSpeed(int scrollSpeed)
{
    return qMax(1, qRound(normalWheelScrollLines * scrollSpeed / 100.0));
}

} // namespace

int main(int argc, char *argv[])
{
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCoreApplication::setOrganizationName(QStringLiteral("SZHBooks"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("szhbooks.local"));
    QCoreApplication::setApplicationName(QStringLiteral("SZHBooks"));

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(
        ":/qt/qml/SZHBooks/assets/branding/szhbooks-icon.png")));

    LocalStateStore localState;
    const auto applyScrollSpeed = [&app, &localState]() {
        // This hint is shared by every Qt Quick scroll view, including the PDF table view.
        app.styleHints()->setWheelScrollLines(wheelScrollLinesForSpeed(localState.scrollSpeed()));
    };
    applyScrollSpeed();
    QObject::connect(&localState,
                     &LocalStateStore::scrollSpeedChanged,
                     &app,
                     applyScrollSpeed);
    LibraryModel libraryModel(&localState);
    ReaderController reader;
    ReadingDocumentFormatter documentFormatter;
    ReadingSearchController searchController;
    ReadingAnnotationStore annotationStore(localState.settingsFilePath());

    QObject::connect(&reader,
                     &ReaderController::documentOpened,
                     &localState,
                     [&localState, &reader](const QUrl &sourceUrl) {
                         if (!sourceUrl.isEmpty()) {
                             localState.setLastBookUrl(sourceUrl);
                             localState.recordBookOpened(sourceUrl,
                                                         reader.title(),
                                                         reader.author(),
                                                         reader.formatName());
                         }
                     });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &localState, &LocalStateStore::sync);

    QQmlApplicationEngine engine;
    engine.setInitialProperties({
        {QStringLiteral("readerController"),
         QVariant::fromValue(static_cast<QObject *>(&reader))},
        {QStringLiteral("localStateStore"),
         QVariant::fromValue(static_cast<QObject *>(&localState))},
        {QStringLiteral("libraryModel"),
         QVariant::fromValue(static_cast<QObject *>(&libraryModel))},
        {QStringLiteral("readingDocumentFormatter"),
         QVariant::fromValue(static_cast<QObject *>(&documentFormatter))},
        {QStringLiteral("readingSearchController"),
         QVariant::fromValue(static_cast<QObject *>(&searchController))},
        {QStringLiteral("readingAnnotationStore"),
         QVariant::fromValue(static_cast<QObject *>(&annotationStore))}
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("SZHBooks", "Main");

    return app.exec();
}
