#include "screenshot.h"

#include <QDateTime>
#include <QDir>
#include <QGuiApplication>
#include <QImage>
#include <QScreen>
#include <QStandardPaths>
#include <qpa/qplatformnativeinterface.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

#include <wayland-client.h>

#include "wayland-lipstick-recorder-client-protocol.h"

namespace {

static QImage::Format shmFormatToQImage(uint32_t format)
{
    switch (format) {
    case WL_SHM_FORMAT_ARGB8888:
        return QImage::Format_ARGB32;
    case WL_SHM_FORMAT_XRGB8888:
        return QImage::Format_RGB32;
    case WL_SHM_FORMAT_RGBA8888:
        return QImage::Format_RGBA8888;
    case WL_SHM_FORMAT_RGBX8888:
        return QImage::Format_RGB32;
    default:
        return QImage::Format_Invalid;
    }
}

static const int kPoolBufferCount = 3;

} // namespace

struct CaptureBuffer
{
    static CaptureBuffer *create(wl_shm *shm, int width, int height, int stride, uint32_t format)
    {
        const QImage::Format qfmt = shmFormatToQImage(format);
        if (qfmt == QImage::Format_Invalid)
            return nullptr;

        const int size = stride * height;
        if (size <= 0)
            return nullptr;

        char filename[] = "/tmp/screentapshot-lipstick-shm-XXXXXX";
        const int fd = mkstemp(filename);
        if (fd < 0)
            return nullptr;

        int flags = fcntl(fd, F_GETFD);
        if (flags != -1)
            fcntl(fd, F_SETFD, flags | FD_CLOEXEC);

        if (ftruncate(fd, size) < 0) {
            close(fd);
            return nullptr;
        }

        uchar *map = static_cast<uchar *>(mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0));
        unlink(filename);
        if (map == MAP_FAILED) {
            close(fd);
            return nullptr;
        }

        auto *buf = new CaptureBuffer;
        buf->data = map;
        buf->mapSize = static_cast<size_t>(size);
        buf->width = width;
        buf->height = height;
        buf->stride = stride;
        buf->wlFormat = format;

        wl_shm_pool *pool = wl_shm_create_pool(shm, fd, size);
        buf->buffer = wl_shm_pool_create_buffer(pool, 0, width, height, stride, format);
        wl_buffer_set_user_data(buf->buffer, buf);
        wl_shm_pool_destroy(pool);
        close(fd);
        return buf;
    }

    ~CaptureBuffer()
    {
        if (buffer) {
            wl_buffer_destroy(buffer);
            buffer = nullptr;
        }
        if (data && mapSize) {
            munmap(data, mapSize);
            data = nullptr;
            mapSize = 0;
        }
    }

    wl_buffer *buffer = nullptr;
    uchar *data = nullptr;
    size_t mapSize = 0;
    int width = 0;
    int height = 0;
    int stride = 0;
    uint32_t wlFormat = 0;
    bool busy = false;
};

Screenshot::Screenshot(QObject *parent)
    : QObject(parent)
{
}

Screenshot::~Screenshot()
{
    destroyRecorder();
    if (m_registry) {
        wl_registry_destroy(m_registry);
        m_registry = nullptr;
    }
    m_display = nullptr;
}

void Screenshot::beginWaylandInit()
{
    if (m_waylandInitStarted)
        return;
    m_waylandInitStarted = true;

    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    m_display = static_cast<wl_display *>(native->nativeResourceForIntegration("display"));
    if (!m_display) {
        m_initFailed = true;
        m_capturePending = false;
        Q_EMIT captureFailed(QStringLiteral("No Wayland display"));
        return;
    }

    m_registry = wl_display_get_registry(m_display);
    static const wl_registry_listener registryListener = {
        registryGlobal,
        registryGlobalRemove
    };
    wl_registry_add_listener(m_registry, &registryListener, this);

    wl_callback *cb = wl_display_sync(m_display);
    static const wl_callback_listener callbackListener = {
        syncDone
    };
    wl_callback_add_listener(cb, &callbackListener, this);
    wl_display_flush(m_display);
}

void Screenshot::tryCreateRecorder()
{
    if (m_recorder || m_initFailed)
        return;

    if (!m_manager || !m_shm) {
        m_initFailed = true;
        m_capturePending = false;
        Q_EMIT captureFailed(QStringLiteral("lipstick_recorder_manager or wl_shm not available"));
        return;
    }

    QScreen *screen = QGuiApplication::screens().value(0);
    if (!screen) {
        m_initFailed = true;
        m_capturePending = false;
        Q_EMIT captureFailed(QStringLiteral("No screen"));
        return;
    }

    QPlatformNativeInterface *native = QGuiApplication::platformNativeInterface();
    wl_output *output = static_cast<wl_output *>(
        native->nativeResourceForScreen(QByteArrayLiteral("output"), screen));
    if (!output) {
        m_initFailed = true;
        m_capturePending = false;
        Q_EMIT captureFailed(QStringLiteral("No wl_output for screen"));
        return;
    }

    m_recorder = lipstick_recorder_manager_create_recorder(m_manager, output);
    static const lipstick_recorder_listener recorderListener = {
        recorderSetup,
        recorderFrame,
        recorderFailed,
        recorderCancelled
    };
    lipstick_recorder_add_listener(m_recorder, &recorderListener, this);
    wl_display_flush(m_display);
}

void Screenshot::destroyRecorder()
{
    if (m_recorder) {
        lipstick_recorder_destroy(m_recorder);
        m_recorder = nullptr;
    }
    clearBuffers();
}

void Screenshot::clearBuffers()
{
    qDeleteAll(m_buffers);
    m_buffers.clear();
}

void Screenshot::capture()
{
    QString folder = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (_useSubfolder)
        folder.append(QStringLiteral("/Screenshots"));

    QDir dir;
    if (!dir.exists(folder) && !dir.mkpath(folder)) {
        Q_EMIT captureFailed(QStringLiteral("Cannot create folder: %1").arg(folder));
        return;
    }

    pendingScreenshot = QStringLiteral("%1/Screenshot-%2.png")
            .arg(folder)
            .arg(QDateTime::currentDateTime().toString(QStringLiteral("yy-MM-dd-hh-mm-ss")));

    if (m_initFailed) {
        Q_EMIT captureFailed(QStringLiteral("Screen capture unavailable"));
        return;
    }

    if (m_waitingFrame) {
        Q_EMIT captureFailed(QStringLiteral("Capture already in progress"));
        return;
    }

    if (!m_waylandInitStarted) {
        m_capturePending = true;
        beginWaylandInit();
        return;
    }

    if (!m_recorder || m_buffers.isEmpty()) {
        m_capturePending = true;
        return;
    }

    submitCapture();
}

void Screenshot::submitCapture()
{
    if (!m_recorder || m_buffers.isEmpty() || pendingScreenshot.isEmpty()) {
        m_capturePending = false;
        return;
    }

    if (m_waitingFrame)
        return;

    CaptureBuffer *buf = nullptr;
    for (CaptureBuffer *b : m_buffers) {
        if (!b->busy) {
            buf = b;
            break;
        }
    }
    if (!buf) {
        Q_EMIT captureFailed(QStringLiteral("No free buffer"));
        m_capturePending = false;
        return;
    }

    lipstick_recorder_record_frame(m_recorder, buf->buffer);
    lipstick_recorder_repaint(m_recorder);
    wl_display_flush(m_display);
    buf->busy = true;
    m_waitingFrame = true;
    m_capturePending = false;
}

void Screenshot::handleSetup(int32_t width, int32_t height, int32_t stride, int32_t format, bool retryAfterInterruptedWait)
{
    clearBuffers();

    const uint32_t ufmt = static_cast<uint32_t>(format);
    for (int i = 0; i < kPoolBufferCount; ++i) {
        CaptureBuffer *b = CaptureBuffer::create(m_shm, width, height, stride, ufmt);
        if (!b) {
            clearBuffers();
            m_initFailed = true;
            m_capturePending = false;
            Q_EMIT captureFailed(QStringLiteral("Failed to create SHM buffer"));
            return;
        }
        m_buffers.append(b);
    }

    const bool needSubmit = m_capturePending || retryAfterInterruptedWait;
    m_capturePending = false;
    if (needSubmit && !pendingScreenshot.isEmpty())
        submitCapture();
}

void Screenshot::registryGlobal(void *data, wl_registry *registry, uint32_t id, const char *interface, uint32_t version)
{
    Q_UNUSED(registry)

    Screenshot *s = static_cast<Screenshot *>(data);
    if (strcmp(interface, "lipstick_recorder_manager") == 0) {
        s->m_manager = static_cast<lipstick_recorder_manager *>(
            wl_registry_bind(registry, id, &lipstick_recorder_manager_interface, qMin(version, 1u)));
    } else if (strcmp(interface, "wl_shm") == 0) {
        s->m_shm = static_cast<wl_shm *>(wl_registry_bind(registry, id, &wl_shm_interface, qMin(version, 1u)));
    }
}

void Screenshot::registryGlobalRemove(void *data, wl_registry *registry, uint32_t id)
{
    Q_UNUSED(data)
    Q_UNUSED(registry)
    Q_UNUSED(id)
}

void Screenshot::syncDone(void *data, wl_callback *cb, uint32_t serial)
{
    Q_UNUSED(serial)
    wl_callback_destroy(cb);

    Screenshot *s = static_cast<Screenshot *>(data);
    s->m_registrySyncDone = true;
    s->tryCreateRecorder();
}

void Screenshot::recorderSetup(void *data, lipstick_recorder *recorder, int32_t width, int32_t height, int32_t stride, int32_t format)
{
    Q_UNUSED(recorder)

    Screenshot *s = static_cast<Screenshot *>(data);
    const bool retry = s->m_waitingFrame;
    s->m_waitingFrame = false;
    s->handleSetup(width, height, stride, format, retry);
}

void Screenshot::recorderFrame(void *data, lipstick_recorder *recorder, wl_buffer *buffer, uint32_t timestamp, int32_t transform)
{
    Q_UNUSED(recorder)
    Q_UNUSED(timestamp)

    Screenshot *s = static_cast<Screenshot *>(data);
    if (!buffer)
        return;

    CaptureBuffer *buf = static_cast<CaptureBuffer *>(wl_buffer_get_user_data(buffer));
    if (!buf) {
        s->m_waitingFrame = false;
        return;
    }

    buf->busy = false;
    s->m_waitingFrame = false;

    const QImage::Format qfmt = shmFormatToQImage(buf->wlFormat);
    if (qfmt == QImage::Format_Invalid || !buf->data) {
        Q_EMIT s->captureFailed(QStringLiteral("Unsupported pixel format"));
        return;
    }

    QImage img(buf->data, buf->width, buf->height, buf->stride, qfmt);
    img = img.copy();
    if (transform == LIPSTICK_RECORDER_TRANSFORM_Y_INVERTED)
        img = img.mirrored(false, true);

    if (!img.save(s->pendingScreenshot, "PNG")) {
        Q_EMIT s->captureFailed(QStringLiteral("Failed to save PNG"));
        return;
    }

    Q_EMIT s->captured(s->pendingScreenshot);
}

void Screenshot::recorderFailed(void *data, lipstick_recorder *recorder, int32_t result, wl_buffer *buffer)
{
    Q_UNUSED(recorder)

    Screenshot *s = static_cast<Screenshot *>(data);
    s->m_waitingFrame = false;
    if (buffer) {
        CaptureBuffer *buf = static_cast<CaptureBuffer *>(wl_buffer_get_user_data(buffer));
        if (buf)
            buf->busy = false;
    }
    Q_EMIT s->captureFailed(QStringLiteral("Compositor capture failed (%1)").arg(result));
}

void Screenshot::recorderCancelled(void *data, lipstick_recorder *recorder, wl_buffer *buffer)
{
    Q_UNUSED(recorder)
    Q_UNUSED(data)

    if (buffer) {
        CaptureBuffer *buf = static_cast<CaptureBuffer *>(wl_buffer_get_user_data(buffer));
        if (buf)
            buf->busy = false;
    }
}

bool Screenshot::useSubfolder() const
{
    return _useSubfolder;
}

void Screenshot::setUseSubfolder(bool value)
{
    if (_useSubfolder != value) {
        _useSubfolder = value;
        Q_EMIT useSubfolderChanged();
    }
}
