#pragma once

#include <QSocketNotifier>
#include <QTimer>
#include <memory>

#include "transport.h"

// Talks to the receiver through /dev/hidrawN. Looks the receiver up in sysfs,
// and keeps looking while it's unplugged so the app picks it up again.
class HidTransport : public Transport {
    Q_OBJECT

public:
    explicit HidTransport(const QString &sysfsRoot = "/sys/class/hidraw", const QString &deviceRoot = "/dev",
                          QObject *parent = nullptr);
    ~HidTransport() override;

    State state() const override { return m_state; }
    bool write(const QByteArray &report) override;
    QString devicePath() const { return m_path; }

    // The hidraw node name (hidraw5) of the receiver, or empty.
    static QString findReceiver(const QString &sysfsRoot);

private:
    void scan();
    void close();
    void readReady();
    void setState(State state);

    QString m_sysfsRoot, m_deviceRoot, m_path;
    int m_fd = -1;
    State m_state = Missing;
    std::unique_ptr<QSocketNotifier> m_notifier;
    QTimer m_scan;
};
