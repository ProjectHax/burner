#include "QtRipEngine.hpp"
#include "RipWorker.hpp"

#include <QThread>
#include <QDebug>

namespace Burner::Engine {

class QtRipEngine::Impl {
public:
    QThread* workerThread{nullptr};
    RipWorker* worker{nullptr};

    bool ripping{false};
    double progress{0.0};
    QString status;
};

QtRipEngine::QtRipEngine(QObject* parent)
    : QObject(parent)
    , m_impl(std::make_unique<Impl>()) {

    qRegisterMetaType<Burner::Core::AudioFormat>("Burner::Core::AudioFormat");
    qRegisterMetaType<QList<int>>("QList<int>");
}

QtRipEngine::~QtRipEngine() {
    if (m_impl->workerThread) {
        m_impl->workerThread->quit();
        m_impl->workerThread->wait();
    }
}

bool QtRipEngine::isRipping() const {
    return m_impl->ripping;
}

double QtRipEngine::progress() const {
    return m_impl->progress;
}

QString QtRipEngine::status() const {
    return m_impl->status;
}

bool QtRipEngine::isInitialized() const {
    return true;
}

void QtRipEngine::startWorker() {
    if (m_impl->workerThread) {
        return;
    }

    m_impl->workerThread = new QThread(this);
    m_impl->worker = new RipWorker();
    m_impl->worker->moveToThread(m_impl->workerThread);

    // Connect worker signals
    connect(m_impl->worker, &RipWorker::scanComplete,
            this, &QtRipEngine::scanComplete, Qt::QueuedConnection);

    connect(m_impl->worker, &RipWorker::metadataComplete,
            this, &QtRipEngine::metadataComplete, Qt::QueuedConnection);

    connect(m_impl->worker, &RipWorker::metadataMatches,
            this, &QtRipEngine::metadataMatches, Qt::QueuedConnection);

    connect(m_impl->worker, &RipWorker::coverArtFetched,
            this, &QtRipEngine::coverArtFetched, Qt::QueuedConnection);

    connect(m_impl->worker, &RipWorker::progressUpdated,
            this, &QtRipEngine::onWorkerProgress, Qt::QueuedConnection);

    connect(m_impl->worker, &RipWorker::ripComplete,
            this, &QtRipEngine::onWorkerComplete, Qt::QueuedConnection);

    connect(m_impl->worker, &RipWorker::error,
            this, &QtRipEngine::ripError, Qt::QueuedConnection);

    connect(m_impl->workerThread, &QThread::finished,
            m_impl->worker, &QObject::deleteLater);

    m_impl->workerThread->start();
}

void QtRipEngine::scanDisc(const QString& device) {
    startWorker();

    m_impl->status = tr("Scanning disc...");
    emit statusChanged(m_impl->status);

    QMetaObject::invokeMethod(m_impl->worker, "scanDisc",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device));
}

void QtRipEngine::lookupMetadata(const QString& device, const QString& discId) {
    startWorker();

    m_impl->status = tr("Looking up metadata...");
    emit statusChanged(m_impl->status);

    QMetaObject::invokeMethod(m_impl->worker, "lookupMetadata",
                               Qt::QueuedConnection,
                               Q_ARG(QString, device),
                               Q_ARG(QString, discId));
}

void QtRipEngine::fetchCoverArt(const QString& releaseId) {
    startWorker();

    m_impl->status = tr("Fetching cover art...");
    emit statusChanged(m_impl->status);

    QMetaObject::invokeMethod(m_impl->worker, "fetchCoverArt",
                               Qt::QueuedConnection,
                               Q_ARG(QString, releaseId));
}

void QtRipEngine::ripTracks(const QString& device,
                             const QString& outputDir,
                             Burner::Core::AudioFormat format,
                             int quality,
                             const QString& pattern,
                             const QList<int>& trackNumbers,
                             bool embedCoverArt,
                             bool saveCoverFile,
                             const QString& albumTitle,
                             const QString& albumArtist,
                             int year,
                             const QString& genre,
                             const QString& releaseId,
                             const QStringList& trackTitles,
                             const QStringList& trackArtists) {
    startWorker();

    m_impl->ripping = true;
    m_impl->progress = 0.0;
    m_impl->status = tr("Starting rip...");

    emit rippingChanged(true);
    emit progressChanged(0.0);
    emit statusChanged(m_impl->status);

    // Queue a lambda rather than using Q_ARG: before Qt 6.5, the string-based
    // invokeMethod() accepts at most 10 arguments
    RipWorker* worker = m_impl->worker;
    QMetaObject::invokeMethod(worker, [=]() {
        worker->ripTracksWithMetadata(device, outputDir, format, quality, pattern,
                                      trackNumbers, embedCoverArt, saveCoverFile,
                                      albumTitle, albumArtist, year, genre,
                                      releaseId, trackTitles, trackArtists);
    }, Qt::QueuedConnection);
}

void QtRipEngine::cancel() {
    if (m_impl->worker) {
        // Use DirectConnection because the worker thread is blocked in rip()
        // and won't process queued events. This is safe because cancel()
        // only sets atomic flags.
        QMetaObject::invokeMethod(m_impl->worker, "cancel", Qt::DirectConnection);
    }
}

void QtRipEngine::onWorkerProgress(double overallPercent, int currentTrack,
                                    int totalTracks, double trackPercent,
                                    const QString& operation) {
    m_impl->progress = overallPercent;
    m_impl->status = operation;

    emit progressChanged(overallPercent);
    emit statusChanged(operation);
    emit ripProgress(overallPercent, currentTrack, totalTracks, trackPercent, operation);
}

void QtRipEngine::onWorkerComplete(bool success, int tracksRipped,
                                    int tracksFailed, const QString& message) {
    m_impl->ripping = false;
    m_impl->progress = 100.0;
    m_impl->status = message;

    emit rippingChanged(false);
    emit progressChanged(100.0);
    emit statusChanged(message);
    emit ripComplete(success, tracksRipped, tracksFailed, message);
}

void QtRipEngine::cleanupWorker() {
    if (m_impl->workerThread) {
        m_impl->workerThread->quit();
        m_impl->workerThread->wait();
        m_impl->workerThread->deleteLater();
        m_impl->workerThread = nullptr;
        m_impl->worker = nullptr;
    }
}

} // namespace Burner::Engine
