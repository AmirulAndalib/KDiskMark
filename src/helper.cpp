#include "helper.h"

#include <QCoreApplication>
#include <QtDBus>
#include <QFile>
#include <PolkitQt1/Authority>
#include <PolkitQt1/Subject>

#include <signal.h>

HelperAdaptor::HelperAdaptor(Helper *parent) :
    QDBusAbstractAdaptor(parent)
{
    m_parentHelper = parent;
}

QVariantMap HelperAdaptor::initSession()
{
    return m_parentHelper->initSession();
}

QVariantMap HelperAdaptor::endSession()
{
    return m_parentHelper->endSession();
}

QVariantMap HelperAdaptor::prepareBenchmarkFile(const QString &benchmarkFile, int fileSize, bool fillZeros)
{
    return m_parentHelper->prepareBenchmarkFile(benchmarkFile, fileSize, fillZeros);
}

QVariantMap HelperAdaptor::startBenchmarkTest(int measuringTime, int fileSize, int randomReadPercentage, bool fillZeros, bool cacheBypass,
                                              int blockSize, int queueDepth, int threads, const QString &rw)
{
    return m_parentHelper->startBenchmarkTest(measuringTime, fileSize, randomReadPercentage, fillZeros, cacheBypass, blockSize, queueDepth, threads, rw);
}

QVariantMap HelperAdaptor::flushPageCache()
{
    return m_parentHelper->flushPageCache();
}

QVariantMap HelperAdaptor::removeBenchmarkFile()
{
    return m_parentHelper->removeBenchmarkFile();
}

QVariantMap HelperAdaptor::stopCurrentTask()
{
    return m_parentHelper->stopCurrentTask();
}

Helper::Helper() : m_helperAdaptor(new HelperAdaptor(this))
{
    if (!QDBusConnection::systemBus().isConnected() || !QDBusConnection::systemBus().registerService(QStringLiteral("dev.jonmagon.kdiskmark.helperinterface")) ||
        !QDBusConnection::systemBus().registerObject(QStringLiteral("/Helper"), this)) {
        qWarning() << QDBusConnection::systemBus().lastError().message();
        qApp->quit();
    }

    m_serviceWatcher = new QDBusServiceWatcher(this);
    m_serviceWatcher->setConnection(QDBusConnection::systemBus());
    m_serviceWatcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);

    connect(m_serviceWatcher, &QDBusServiceWatcher::serviceUnregistered, qApp, [this](const QString &service) {
        m_serviceWatcher->removeWatchedService(service);
        if (m_serviceWatcher->watchedServices().isEmpty()) {
            qApp->quit();
        }
    });

    QObject::connect(this, &Helper::taskFinished, m_helperAdaptor, &HelperAdaptor::taskFinished);
}

QVariantMap Helper::initSession()
{
    if (!calledFromDBus()) {
        return {};
    }

    if (m_serviceWatcher->watchedServices().contains(message().service())) {
        return {{"success", true}};
    }

    if (!m_serviceWatcher->watchedServices().isEmpty()) {
        return {{"success", false}, {"error", "There are already registered DBus connection."}};
    }

    PolkitQt1::SystemBusNameSubject subject(message().service());
    PolkitQt1::Authority *authority = PolkitQt1::Authority::instance();

    PolkitQt1::Authority::Result result = PolkitQt1::Authority::No;
    QEventLoop e;
    connect(authority, &PolkitQt1::Authority::checkAuthorizationFinished, &e, [&e, &result](PolkitQt1::Authority::Result _result) {
        result = _result;
        e.quit();
    });

    authority->checkAuthorization(QStringLiteral("dev.jonmagon.kdiskmark.helper.init"), subject, PolkitQt1::Authority::AllowUserInteraction);
    e.exec();

    if (authority->hasError()) {
        qDebug() << "Encountered error while checking authorization, error code: " << authority->lastError() << authority->errorDetails();
        authority->clearError();
    }

    switch (result) {
    case PolkitQt1::Authority::Yes:
        // track who called into us so we can close when all callers have gone away
        m_serviceWatcher->addWatchedService(message().service());
        return {{"success", true}};
    default:
        sendErrorReply(QDBusError::AccessDenied);
        if (m_serviceWatcher->watchedServices().isEmpty())
            qApp->quit();
        return {};
    }
}

QVariantMap Helper::endSession()
{
    if (!isCallerAuthorized()) {
        return {};
    }

    qApp->exit();

    return {};
}

bool Helper::testFilePath(const QString &benchmarkPath)
{
    if (QFileInfo(benchmarkPath).isSymbolicLink()) {
        qWarning("The path should not be symbolic link.");
        return false;
    }

    if (benchmarkPath.startsWith("/dev")) {
        qWarning("Cannot specify a raw device.");
        return false;
    }

    return true;
}

QVariantMap Helper::prepareBenchmarkFile(const QString &benchmarkPath, int fileSize, bool fillZeros)
{
    if (!isCallerAuthorized()) {
        return {};
    }

    // If benchmarking has been done, but removeBenchmarkFile has not been called,
    // and benchmarking on a new file is called, then reject the request. The *previous* file must be removed first.
    if (!m_benchmarkFile.fileName().isNull()) {
        return {{"success", false}, {"error", "A new benchmark session should be started."}};
    }

    if (!testFilePath(benchmarkPath)) {
        return {{"success", false}, {"error", "The path to the file is incorrect."}};
    }

    m_benchmarkFile.setFileTemplate(QStringLiteral("%1/%2").arg(benchmarkPath).arg("kdiskmark-XXXXXX.tmp"));

    if (!m_benchmarkFile.open()) {
        return {{"success", false}, {"error", QString("An error occurred while creating the benchmark file: %1").arg(m_benchmarkFile.errorString())}};
    }

    m_process = new QProcess();
    m_process->start("fio", QStringList()
                     << QStringLiteral("--output-format=json")
                     << QStringLiteral("--create_only=1")
                     << QStringLiteral("--filename=%1").arg(m_benchmarkFile.fileName())
                     << QStringLiteral("--size=%1m").arg(fileSize)
                     << QStringLiteral("--zero_buffers=%1").arg(fillZeros)
                     << QStringLiteral("--name=prepare"));

    connect(m_process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [=] (int exitCode, QProcess::ExitStatus exitStatus) {
        emit taskFinished(exitStatus == QProcess::NormalExit, QString(m_process->readAllStandardOutput()), QString(m_process->readAllStandardError()));
    });

    return {{"success", true}};
}

QVariantMap Helper::startBenchmarkTest(int measuringTime, int fileSize, int randomReadPercentage, bool fillZeros, bool cacheBypass,
                                       int blockSize, int queueDepth, int threads, const QString &rw)
{
    if (!isCallerAuthorized()) {
        return {};
    }

    if (m_benchmarkFile.fileName().isNull() || !QFile(m_benchmarkFile.fileName()).exists()) {
        return {{"success", false}, {"error", "The benchmark file was not pre-created."}};
    }

    m_process = new QProcess();
    m_process->start("fio", QStringList()
                     << QStringLiteral("--output-format=json")
                     << QStringLiteral("--ioengine=libaio")
                     << QStringLiteral("--randrepeat=0")
                     << QStringLiteral("--refill_buffers")
                     << QStringLiteral("--end_fsync=1")
                     << QStringLiteral("--direct=%1").arg(cacheBypass)
                     << QStringLiteral("--rwmixread=%1").arg(randomReadPercentage)
                     << QStringLiteral("--filename=%1").arg(m_benchmarkFile.fileName())
                     << QStringLiteral("--name=%1").arg(rw)
                     << QStringLiteral("--size=%1m").arg(fileSize)
                     << QStringLiteral("--zero_buffers=%1").arg(fillZeros)
                     << QStringLiteral("--bs=%1k").arg(blockSize)
                     << QStringLiteral("--runtime=%1").arg(measuringTime)
                     << QStringLiteral("--rw=%1").arg(rw)
                     << QStringLiteral("--iodepth=%1").arg(queueDepth)
                     << QStringLiteral("--numjobs=%1").arg(threads));

    connect(m_process, static_cast<void(QProcess::*)(int, QProcess::ExitStatus)>(&QProcess::finished),
            [=] (int exitCode, QProcess::ExitStatus exitStatus) {
        emit taskFinished(exitStatus == QProcess::NormalExit, QString(m_process->readAllStandardOutput()), QString(m_process->readAllStandardError()));
    });

    return {{"success", true}};
}

QVariantMap Helper::flushPageCache()
{
    if (!isCallerAuthorized()) {
        return {};
    }

    if (m_benchmarkFile.fileName().isNull() || !QFile(m_benchmarkFile.fileName()).exists()) {
        return {{"success", false}, {"error", "A benchmark file must first be created."}};
    }

    QFile file("/proc/sys/vm/drop_caches");

    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write("1");
        file.close();
    }
    else {
        return {{"success", false}, {"error", file.errorString()}};
    }

    return {{"success", true}};
}

QVariantMap Helper::removeBenchmarkFile()
{
    if (!isCallerAuthorized()) {
        return {};
    }

    if (m_benchmarkFile.fileName().isNull() || !QFile(m_benchmarkFile.fileName()).exists()) {
        return {{"success", false}, {"error", "Cannot remove the benchmark file, because it doesn't exist."}};
    }

    m_benchmarkFile.close();

    return {{"success", m_benchmarkFile.remove()}};
}

QVariantMap Helper::stopCurrentTask()
{
    if (!isCallerAuthorized()) {
        return {};
    }

    if (!m_process) {
        return {{"success", false}, {"error", "The pointer to the process is empty."}};
    }

    if (m_process->state() == QProcess::Running || m_process->state() == QProcess::Starting) {
        m_process->terminate();
        m_process->waitForFinished(-1);
    }

    return {{"success", true}};
}

bool Helper::isCallerAuthorized()
{
    if (!calledFromDBus()) {
        return false;
    }

    return m_serviceWatcher->watchedServices().contains(message().service());
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    Helper helper;
    a.exec();
}
