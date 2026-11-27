#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

// Moves reports to and from the receiver. The Headset only sees this
// interface, so tests can drive it with a fake headset.
class Transport : public QObject {
    Q_OBJECT

public:
    enum State { Missing, NoPermission, Open };
    Q_ENUM(State)

    using QObject::QObject;

    virtual State state() const = 0;
    virtual bool write(const QByteArray &report) = 0;

signals:
    void stateChanged();
    void received(const QByteArray &report);
};
