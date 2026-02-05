#include "skinmanager.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>

#include <miniz.h>

Q_LOGGING_CATEGORY(lcSkin, "bridge.skin")

const QString SkinManager::SKIN_ZIP_URL =
    QStringLiteral("https://github.com/Kulitorum/streamline_project/archive/refs/heads/main.zip");

SkinManager::SkinManager(QObject *parent)
    : QObject(parent)
{
}

void SkinManager::initialize()
{
    m_nam = new QNetworkAccessManager(this);
    loadMetadata();

    // If we already have a cached skin, make it available immediately
    QDir dir(skinDir());
    if (dir.exists() && QFile::exists(skinDir() + "/index.html")) {
        m_skinAvailable = true;
        qCInfo(lcSkin) << "Cached skin found at" << skinDir();
        emit skinReady();
    } else {
        // No cached skin — extract the bundled version for immediate use
        extractBundledSkin();
    }

    // Check for updates in background
    checkForUpdate();
}

void SkinManager::extractBundledSkin()
{
    QFile bundled(":/assets/skin.zip");
    if (!bundled.open(QIODevice::ReadOnly)) {
        qCWarning(lcSkin) << "No bundled skin.zip resource found";
        return;
    }

    QByteArray data = bundled.readAll();
    qCInfo(lcSkin) << "Extracting bundled skin (" << data.size() << "bytes)";

    if (extractZipFromMemory(data, skinDir())) {
        m_skinAvailable = true;
        qCInfo(lcSkin) << "Bundled skin extracted to" << skinDir();
        emit skinReady();
    } else {
        qCWarning(lcSkin) << "Failed to extract bundled skin";
    }
}

QString SkinManager::skinRootPath() const
{
    return skinDir();
}

QString SkinManager::skinBaseDir() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/skins";
}

QString SkinManager::skinDir() const
{
    return skinBaseDir() + "/streamline_project";
}

QString SkinManager::metadataPath() const
{
    return skinBaseDir() + "/metadata.json";
}

QString SkinManager::zipTempPath() const
{
    return skinBaseDir() + "/download.zip";
}

void SkinManager::loadMetadata()
{
    QFile file(metadataPath());
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject obj = doc.object();
    m_etag = obj["etag"].toString();
    m_lastModified = obj["lastModified"].toString();
}

void SkinManager::saveMetadata()
{
    QDir().mkpath(skinBaseDir());

    QJsonObject obj;
    obj["etag"] = m_etag;
    obj["lastModified"] = m_lastModified;
    obj["extractedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    QFile file(metadataPath());
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
}

void SkinManager::checkForUpdate()
{
    qCInfo(lcSkin) << "Checking for skin updates...";

    QNetworkRequest request{QUrl(SKIN_ZIP_URL)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_nam->head(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcSkin) << "HEAD request failed:" << reply->errorString();
            if (!m_skinAvailable) {
                emit skinUpdateFailed(reply->errorString());
            }
            return;
        }

        QString newEtag = QString::fromLatin1(reply->rawHeader("ETag"));
        QString newLastModified = QString::fromLatin1(reply->rawHeader("Last-Modified"));

        // Check if skin has changed
        if (!newEtag.isEmpty() && newEtag == m_etag && m_skinAvailable) {
            qCInfo(lcSkin) << "Skin is up to date";
            return;
        }

        if (newEtag.isEmpty() && !newLastModified.isEmpty() &&
            newLastModified == m_lastModified && m_skinAvailable) {
            qCInfo(lcSkin) << "Skin is up to date (by Last-Modified)";
            return;
        }

        // Skin has changed or first download
        m_etag = newEtag;
        m_lastModified = newLastModified;
        downloadSkin();
    });
}

void SkinManager::downloadSkin()
{
    qCInfo(lcSkin) << "Downloading skin from" << SKIN_ZIP_URL;

    QNetworkRequest request{QUrl(SKIN_ZIP_URL)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    auto *reply = m_nam->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();

        if (reply->error() != QNetworkReply::NoError) {
            qCWarning(lcSkin) << "Download failed:" << reply->errorString();
            emit skinUpdateFailed(reply->errorString());
            return;
        }

        QByteArray data = reply->readAll();
        qCInfo(lcSkin) << "Downloaded" << data.size() << "bytes";

        // Save zip to temp file
        QDir().mkpath(skinBaseDir());
        QFile zipFile(zipTempPath());
        if (!zipFile.open(QIODevice::WriteOnly)) {
            qCWarning(lcSkin) << "Failed to write zip file:" << zipTempPath();
            emit skinUpdateFailed("Failed to write zip file");
            return;
        }
        zipFile.write(data);
        zipFile.close();

        // Extract
        if (!extractZip(zipTempPath(), skinDir())) {
            qCWarning(lcSkin) << "Failed to extract skin zip";
            emit skinUpdateFailed("Failed to extract zip");
            return;
        }

        saveMetadata();
        m_skinAvailable = true;
        qCInfo(lcSkin) << "Skin installed at" << skinDir();
        emit skinReady();
    });
}

// Shared extraction logic — works on an already-initialized mz_zip_archive
static bool extractZipEntries(mz_zip_archive &zip, const QString &destDir)
{
    mz_uint numFiles = mz_zip_reader_get_num_files(&zip);

    // First pass: find the top-level directory prefix to strip
    // GitHub zips always have a single top-level dir like "repo-branch/"
    QString topLevelDir;
    if (numFiles > 0) {
        mz_zip_archive_file_stat stat;
        if (mz_zip_reader_file_stat(&zip, 0, &stat)) {
            QString name = QString::fromUtf8(stat.m_filename);
            int slashIdx = name.indexOf('/');
            if (slashIdx > 0) {
                topLevelDir = name.left(slashIdx + 1);
            }
        }
    }

    // Extract all files
    for (mz_uint i = 0; i < numFiles; i++) {
        mz_zip_archive_file_stat stat;
        if (!mz_zip_reader_file_stat(&zip, i, &stat)) continue;

        QString name = QString::fromUtf8(stat.m_filename);

        // Strip top-level directory
        if (!topLevelDir.isEmpty() && name.startsWith(topLevelDir)) {
            name = name.mid(topLevelDir.length());
        }
        if (name.isEmpty()) continue;

        QString outPath = destDir + "/" + name;

        if (stat.m_is_directory || name.endsWith('/')) {
            QDir().mkpath(outPath);
        } else {
            QDir().mkpath(QFileInfo(outPath).absolutePath());

            size_t size = 0;
            void *data = mz_zip_reader_extract_to_heap(&zip, i, &size, 0);
            if (data) {
                QFile outFile(outPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(static_cast<const char*>(data), static_cast<qint64>(size));
                    outFile.close();
                }
                mz_free(data);
            }
        }
    }

    mz_zip_reader_end(&zip);
    return QFile::exists(destDir + "/index.html");
}

bool SkinManager::extractZip(const QString &zipPath, const QString &destDir)
{
    QDir(destDir).removeRecursively();
    QDir().mkpath(destDir);

    QByteArray zipPathUtf8 = zipPath.toUtf8();

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_file(&zip, zipPathUtf8.constData(), 0)) {
        qCWarning(lcSkin) << "Failed to open zip:" << mz_zip_get_error_string(mz_zip_get_last_error(&zip));
        return false;
    }

    bool ok = extractZipEntries(zip, destDir);

    // Clean up temp zip
    QFile::remove(zipPath);

    return ok;
}

bool SkinManager::extractZipFromMemory(const QByteArray &data, const QString &destDir)
{
    QDir(destDir).removeRecursively();
    QDir().mkpath(destDir);

    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));

    if (!mz_zip_reader_init_mem(&zip, data.constData(), data.size(), 0)) {
        qCWarning(lcSkin) << "Failed to read zip from memory:" << mz_zip_get_error_string(mz_zip_get_last_error(&zip));
        return false;
    }

    return extractZipEntries(zip, destDir);
}
