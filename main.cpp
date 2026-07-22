#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QStyleHints>
#include <QVariant>
#include <QVariantMap>
#include <QWindow>
#include <QtMath>

#include "library/bookcoverprovider.h"
#include "diagnostics/diagnosticservice.h"
#include "library/bookmetadataservice.h"
#include "library/librarymodel.h"
#include "library/libraryrepository.h"
#include "library/libraryscanservice.h"
#include "localization/localizationcontroller.h"
#include "notes/notescentermodel.h"
#include "platform/desktopintegration.h"
#include "reader/readingsearchcontroller.h"
#include "reader/readingdocumentformatter.h"
#include "readercontroller.h"
#include "search/librarysearchindex.h"
#include "search/librarysearchmodel.h"
#include "storage/localstatestore.h"
#include "storage/profilearchiveservice.h"
#include "storage/readingannotationstore.h"
#include "sync/onedrivelibraryservice.h"

namespace {

constexpr int normalWheelScrollLines = 6;

int wheelScrollLinesForSpeed(int scrollSpeed)
{
    return qMax(1, qRound(normalWheelScrollLines * scrollSpeed / 100.0));
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
    QCoreApplication::setOrganizationName(QStringLiteral("SZHBooks"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("szhbooks.local"));
    QCoreApplication::setApplicationName(QStringLiteral("SZHBooks"));
    QCoreApplication::setApplicationVersion(QStringLiteral(SZHBOOKS_VERSION));

    QGuiApplication app(argc, argv);
    app.setWindowIcon(QIcon(QStringLiteral(
        ":/qt/qml/SZHBooks/assets/branding/szhbooks-icon.png")));

    DiagnosticService diagnosticService;
    DesktopIntegration desktopIntegration(BookMetadataService::supportedSuffixes());
    LocalStateStore localState;
    if (localState.profileRecoveryState() != QLatin1String("healthy")) {
        qWarning().noquote() << localState.profileRecoveryMessage();
    }
    ProfileArchiveService profileArchiveService(&localState);
    const auto applyScrollSpeed = [&app, &localState]() {
        // This hint is shared by every Qt Quick scroll view, including the PDF table view.
        app.styleHints()->setWheelScrollLines(wheelScrollLinesForSpeed(localState.scrollSpeed()));
    };
    applyScrollSpeed();
    QObject::connect(&localState,
                     &LocalStateStore::scrollSpeedChanged,
                     &app,
                     applyScrollSpeed);
    BookCoverProvider coverProvider;
    BookMetadataService metadataService(&coverProvider);
    LibraryRepository libraryRepository(&localState, &metadataService);
    LibraryScanService libraryScanService(&metadataService);
    LibraryModel libraryModel(&libraryRepository);
    libraryModel.setScanService(&libraryScanService);
    ReaderController reader;
    ReadingDocumentFormatter documentFormatter;
    ReadingSearchController searchController;
    ReadingAnnotationStore annotationStore(localState.profileDatabase());
    NotesCenterModel notesCenterModel(localState.profileDatabase());
    LibrarySearchModel librarySearchModel(
        &libraryRepository,
        LibrarySearchIndex::databasePathForProfile(localState.databaseFilePath()));
    OneDriveLibraryService oneDriveLibraryService(&localState, &libraryRepository);

    QObject::connect(&localState,
                     &LocalStateStore::profileReplaced,
                     &annotationStore,
                     &ReadingAnnotationStore::reload);
    QObject::connect(&localState,
                     &LocalStateStore::profileReplaced,
                     &notesCenterModel,
                     &NotesCenterModel::refresh);
    QObject::connect(&libraryRepository,
                     &LibraryRepository::changed,
                     &notesCenterModel,
                     &NotesCenterModel::refresh);
    QObject::connect(&annotationStore,
                     &ReadingAnnotationStore::annotationsChanged,
                     &notesCenterModel,
                     &NotesCenterModel::refresh);
    QObject::connect(&notesCenterModel,
                     &NotesCenterModel::annotationChanged,
                     &annotationStore,
                     [&annotationStore](const QUrl &sourceUrl) {
                         if (annotationStore.documentUrl() == sourceUrl) {
                             annotationStore.reload();
                         }
                     });
    QObject::connect(&annotationStore,
                     &ReadingAnnotationStore::profileChanged,
                     &oneDriveLibraryService,
                     &OneDriveLibraryService::scheduleSynchronization);
    QObject::connect(&notesCenterModel,
                     &NotesCenterModel::profileChanged,
                     &oneDriveLibraryService,
                     &OneDriveLibraryService::scheduleSynchronization);

    QObject::connect(&reader,
                     &ReaderController::documentOpened,
                     &localState,
                     [&localState, &libraryRepository, &reader](const QUrl &sourceUrl) {
                         if (!sourceUrl.isEmpty()) {
                             localState.setLastBookUrl(sourceUrl);
                             libraryRepository.recordBookOpened(sourceUrl,
                                                                reader.title(),
                                                                reader.author(),
                                                                reader.formatName());
                         }
                     });
    QObject::connect(&app,
                     &QCoreApplication::aboutToQuit,
                     &app,
                     [&annotationStore,
                      &desktopIntegration,
                      &localState,
                      &oneDriveLibraryService]() {
                         annotationStore.sync();
                         desktopIntegration.saveWindowState();
                         localState.sync();
                         if (oneDriveLibraryService.configured()) {
                             oneDriveLibraryService.synchronizeNow();
                         }
                     });

    QQmlApplicationEngine engine;
    LocalizationController localizationController(&localState, &engine);
    engine.setInitialProperties({
        {QStringLiteral("readerController"),
         QVariant::fromValue(static_cast<QObject *>(&reader))},
        {QStringLiteral("localStateStore"),
         QVariant::fromValue(static_cast<QObject *>(&localState))},
        {QStringLiteral("profileArchiveService"),
         QVariant::fromValue(static_cast<QObject *>(&profileArchiveService))},
        {QStringLiteral("localizationController"),
         QVariant::fromValue(static_cast<QObject *>(&localizationController))},
        {QStringLiteral("libraryModel"),
         QVariant::fromValue(static_cast<QObject *>(&libraryModel))},
        {QStringLiteral("readingDocumentFormatter"),
         QVariant::fromValue(static_cast<QObject *>(&documentFormatter))},
        {QStringLiteral("readingSearchController"),
         QVariant::fromValue(static_cast<QObject *>(&searchController))},
        {QStringLiteral("readingAnnotationStore"),
         QVariant::fromValue(static_cast<QObject *>(&annotationStore))},
        {QStringLiteral("notesCenterModel"),
         QVariant::fromValue(static_cast<QObject *>(&notesCenterModel))},
        {QStringLiteral("librarySearchModel"),
         QVariant::fromValue(static_cast<QObject *>(&librarySearchModel))},
        {QStringLiteral("oneDriveLibraryService"),
         QVariant::fromValue(static_cast<QObject *>(&oneDriveLibraryService))},
        {QStringLiteral("desktopIntegration"),
         QVariant::fromValue(static_cast<QObject *>(&desktopIntegration))},
        {QStringLiteral("diagnosticService"),
         QVariant::fromValue(static_cast<QObject *>(&diagnosticService))}
    });

    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("SZHBooks", "Main");

    if (!engine.rootObjects().isEmpty()) {
        desktopIntegration.attachWindow(
            qobject_cast<QWindow *>(engine.rootObjects().constFirst()));
        desktopIntegration.setReady();
    }

    return app.exec();
}
