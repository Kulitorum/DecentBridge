#ifndef SKINMANAGER_H
#define SKINMANAGER_H

#include <QObject>
#include <QNetworkAccessManager>

/**
 * @brief Downloads and caches WebUI skins from GitHub
 *
 * On initialize(), checks for a cached skin on disk. If found, emits skinReady()
 * immediately. Then sends a HEAD request to GitHub to check if the skin has been
 * updated (via ETag). If updated, downloads the new zip, extracts it, and emits
 * skinReady() again.
 *
 * Skin files are stored in QStandardPaths::AppDataLocation/skins/streamline_project/
 */
class SkinManager : public QObject
{
    Q_OBJECT

public:
    explicit SkinManager(QObject *parent = nullptr);

    void initialize();

    bool hasSkin() const { return m_skinAvailable; }
    QString skinRootPath() const;

signals:
    void skinReady();
    void skinUpdateFailed(const QString &error);

private:
    void extractBundledSkin();
    void checkForUpdate();
    void downloadSkin();
    bool extractZip(const QString &zipPath, const QString &destDir);
    bool extractZipFromMemory(const QByteArray &data, const QString &destDir);
    void loadMetadata();
    void saveMetadata();

    QString skinBaseDir() const;
    QString skinDir() const;
    QString metadataPath() const;
    QString zipTempPath() const;

    QNetworkAccessManager *m_nam = nullptr;
    QString m_etag;
    QString m_lastModified;
    bool m_skinAvailable = false;

    static const QString SKIN_ZIP_URL;
};

#endif // SKINMANAGER_H
