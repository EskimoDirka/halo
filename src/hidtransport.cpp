#include "hidtransport.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#include "protocol.h"

HidTransport::HidTransport(const QString &sysfsRoot, const QString &deviceRoot, QObject *parent)
    : Transport(parent), m_sysfsRoot(sysfsRoot), m_deviceRoot(deviceRoot) {
    // Polling sysfs every two seconds is cheap and needs no udev monitor;
    // it stops while the receiver is open.
    m_scan.setInterval(2000);
    connect(&m_scan, &QTimer::timeout, this, &HidTransport::scan);
    m_scan.start();
    scan();
}

HidTransport::~HidTransport() { close(); }

QString HidTransport::findReceiver(const QString &sysfsRoot) {
    static const QRegularExpression id(R"(^HID_ID=[0-9A-Fa-f]+:([0-9A-Fa-f]+):([0-9A-Fa-f]+)$)",
                                       QRegularExpression::MultilineOption);
    const auto nodes = QDir(sysfsRoot).entryList({"hidraw*"}, QDir::Dirs | QDir::System | QDir::NoDotAndDotDot,
                                                 QDir::Name);
    for (const auto &node : nodes) {
        QFile uevent(sysfsRoot + "/" + node + "/device/uevent");
        if (!uevent.open(QIODevice::ReadOnly))
            continue;
        const auto match = id.match(QString::fromLatin1(uevent.readAll()));
        if (!match.hasMatch() || match.captured(1).toUInt(nullptr, 16) != Protocol::VendorId)
            continue;
        const auto product = match.captured(2).toUInt(nullptr, 16);
        for (auto known : Protocol::ProductIds)
            if (product == known)
                return node;
    }
    return {};
}

void HidTransport::scan() {
    const QString node = findReceiver(m_sysfsRoot);
    if (node.isEmpty()) {
        setState(Missing);
        return;
    }
    m_path = m_deviceRoot + "/" + node;
    m_fd = ::open(QFile::encodeName(m_path).constData(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
    if (m_fd < 0) {
        // Without halo's udev rule only root may open the node; say so
        // instead of quietly showing nothing.
        setState(errno == EACCES || errno == EPERM ? NoPermission : Missing);
        return;
    }
    m_scan.stop();
    m_notifier = std::make_unique<QSocketNotifier>(m_fd, QSocketNotifier::Read);
    connect(m_notifier.get(), &QSocketNotifier::activated, this, &HidTransport::readReady);
    setState(Open);
}

void HidTransport::close() {
    m_notifier.reset();
    if (m_fd >= 0)
        ::close(m_fd);
    m_fd = -1;
}

void HidTransport::readReady() {
    char buffer[Protocol::ReportSize];
    for (;;) {
        const ssize_t n = ::read(m_fd, buffer, sizeof buffer);
        if (n > 0) {
            emit received(QByteArray(buffer, int(n)));
            continue;
        }
        if (n < 0 && (errno == EAGAIN || errno == EINTR))
            return;
        // Unplugged: the node reports an error or end of file.
        close();
        setState(Missing);
        m_scan.start();
        return;
    }
}

bool HidTransport::write(const QByteArray &report) {
    if (m_fd < 0)
        return false;
    if (::write(m_fd, report.constData(), size_t(report.size())) == report.size())
        return true;
    close();
    setState(Missing);
    m_scan.start();
    return false;
}

void HidTransport::setState(State state) {
    if (state == m_state)
        return;
    m_state = state;
    emit stateChanged();
}
