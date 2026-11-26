#pragma once

#include <qdbusabstractadaptor.h>
#include <qdbusconnection.h>
#include <qdbusmessage.h>
#include <qdbusextratypes.h>
#include <qobject.h>
#include <qqmlintegration.h>
#include <qstring.h>
#include <qtimer.h>

class QDBusServiceWatcher;

namespace caelestia::services {

class BluetoothAgent;

class BluetoothAgentAdaptor : public QDBusAbstractAdaptor {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.bluez.Agent1")

public:
    explicit BluetoothAgentAdaptor(BluetoothAgent* agent);

public slots:
    Q_NOREPLY void Release();
    QString RequestPinCode(const QDBusObjectPath& device, const QDBusMessage& msg);
    void DisplayPinCode(const QDBusObjectPath& device, const QString& pincode);
    uint RequestPasskey(const QDBusObjectPath& device, const QDBusMessage& msg);
    void DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered);
    void RequestConfirmation(const QDBusObjectPath& device, uint passkey, const QDBusMessage& msg);
    void RequestAuthorization(const QDBusObjectPath& device, const QDBusMessage& msg);
    void AuthorizeService(const QDBusObjectPath& device, const QString& uuid, const QDBusMessage& msg);
    Q_NOREPLY void Cancel();

private:
    BluetoothAgent* m_agent;
};

class BluetoothAgent : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(bool registered READ registered NOTIFY registeredChanged)
    Q_PROPERTY(RequestType type READ type NOTIFY requestChanged)
    Q_PROPERTY(bool needsReply READ needsReply NOTIFY requestChanged)
    Q_PROPERTY(QString devicePath READ devicePath NOTIFY requestChanged)
    Q_PROPERTY(QString deviceName READ deviceName NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceAddress READ deviceAddress NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString deviceIcon READ deviceIcon NOTIFY deviceInfoChanged)
    Q_PROPERTY(QString code READ code NOTIFY requestChanged)
    Q_PROPERTY(int entered READ entered NOTIFY enteredChanged)
    Q_PROPERTY(QString service READ service NOTIFY requestChanged)

public:
    enum class RequestType : quint8 {
        None = 0,
        Confirm,
        Authorize,
        AuthorizeService,
        PinCode,
        Passkey,
        DisplayPinCode,
        DisplayPasskey,
    };
    Q_ENUM(RequestType)

    enum class FailReason : quint8 {
        Cancelled = 0,
        TimedOut,
    };
    Q_ENUM(FailReason)

    explicit BluetoothAgent(QObject* parent = nullptr);
    ~BluetoothAgent() override;

    [[nodiscard]] bool registered() const;
    [[nodiscard]] RequestType type() const;
    [[nodiscard]] bool needsReply() const;
    [[nodiscard]] QString devicePath() const;
    [[nodiscard]] QString deviceName() const;
    [[nodiscard]] QString deviceAddress() const;
    [[nodiscard]] QString deviceIcon() const;
    [[nodiscard]] QString code() const;
    [[nodiscard]] int entered() const;
    [[nodiscard]] QString service() const;

    Q_INVOKABLE void accept();
    Q_INVOKABLE void reject();
    Q_INVOKABLE bool submit(const QString& input);

    void beginRequest(RequestType type, const QDBusObjectPath& device, const QDBusMessage& msg,
        const QString& code = {}, const QString& service = {});
    void showCode(RequestType type, const QDBusObjectPath& device, const QString& code, int entered);
    void cancelFromBluez();
    void releasedByBluez();

signals:
    void registeredChanged();
    void requestChanged();
    void deviceInfoChanged();
    void enteredChanged();
    void failed(const QString& deviceName, caelestia::services::BluetoothAgent::FailReason reason);

private slots:
    void handleDeviceProperties(const QString& iface, const QVariantMap& changed, const QStringList& invalidated);

private:
    void registerWithBluez();
    void setRegistered(bool registered);
    void replyError(const QString& name, const QString& message);
    void clear();
    void finishReplied();
    void watchDevice(const QString& path);
    void unwatchDevice();
    void loadDeviceInfo(const QString& path);
    [[nodiscard]] bool isPaired(const QString& path) const;

    QDBusConnection m_bus;
    QDBusServiceWatcher* m_watcher;
    QTimer m_timeout;
    bool m_registered = false;
    bool m_exported = false;

    RequestType m_type = RequestType::None;
    QDBusMessage m_pending;
    bool m_hasPending = false;
    bool m_accepted = false;
    QString m_devicePath;
    QString m_watchedPath;
    QString m_infoPath;
    QString m_deviceName;
    QString m_deviceAddress;
    QString m_deviceIcon;
    QString m_code;
    int m_entered = 0;
    QString m_service;
};

} // namespace caelestia::services
