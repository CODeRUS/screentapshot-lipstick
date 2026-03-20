#ifndef SCREENSHOT_H
#define SCREENSHOT_H

#include <QObject>
#include <QList>
#include <QString>

struct wl_display;
struct wl_registry;
struct wl_shm;
struct wl_buffer;
struct wl_callback;
struct lipstick_recorder_manager;
struct lipstick_recorder;

struct CaptureBuffer;

class Screenshot : public QObject
{
    Q_OBJECT
public:
    explicit Screenshot(QObject *parent = nullptr);
    ~Screenshot();

    Q_PROPERTY(bool useSubfolder READ useSubfolder WRITE setUseSubfolder NOTIFY useSubfolderChanged)

public slots:
    void capture();

signals:
    void useSubfolderChanged();
    void captured(const QString &path);
    void captureFailed(const QString &message);

private:
    bool useSubfolder() const;
    void setUseSubfolder(bool value);

    void beginWaylandInit();
    void tryCreateRecorder();
    void destroyRecorder();
    void clearBuffers();
    void submitCapture();
    void handleSetup(int32_t width, int32_t height, int32_t stride, int32_t format, bool retryAfterInterruptedWait);

    static void registryGlobal(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version);
    static void registryGlobalRemove(void *data, wl_registry *registry, uint32_t id);
    static void syncDone(void *data, struct wl_callback *cb, uint32_t serial);
    static void recorderSetup(void *data, lipstick_recorder *recorder, int32_t width, int32_t height, int32_t stride, int32_t format);
    static void recorderFrame(void *data, lipstick_recorder *recorder, wl_buffer *buffer, uint32_t time, int32_t transform);
    static void recorderFailed(void *data, lipstick_recorder *recorder, int32_t result, wl_buffer *buffer);
    static void recorderCancelled(void *data, lipstick_recorder *recorder, wl_buffer *buffer);

    bool _useSubfolder = false;
    QString pendingScreenshot;

    wl_display *m_display = nullptr;
    wl_registry *m_registry = nullptr;
    wl_shm *m_shm = nullptr;
    lipstick_recorder_manager *m_manager = nullptr;
    lipstick_recorder *m_recorder = nullptr;

    QList<CaptureBuffer *> m_buffers;

    bool m_waylandInitStarted = false;
    bool m_registrySyncDone = false;
    bool m_capturePending = false;
    bool m_waitingFrame = false;
    bool m_initFailed = false;
};

#endif // SCREENSHOT_H
