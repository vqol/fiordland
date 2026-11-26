#include "bluetoothagent.hpp"

#include <qdbuserror.h>
#include <qdbusmetatype.h>
#include <qdbuspendingcall.h>
#include <qdbuspendingreply.h>
#include <qdbusreply.h>
#include <qdbusservicewatcher.h>
#include <qloggingcategory.h>
#include <qregularexpression.h>

namespace {
Q_LOGGING_CATEGORY(lcBtAgent, "caelestia.services.bluetoothagent", QtInfoMsg)
}

namespace caelestia::services {
using Qt::StringLiterals::operator""_s;

namespace {
const QString k_service = u"org.bluez"_s;
const QString k_managerPath = u"/org/bluez"_s;
const QString k_managerIface = u"org.bluez.AgentManager1"_s;
const QString k_deviceIface = u"org.bluez.Device1"_s;
const QString k_propsIface = u"org.freedesktop.DBus.Properties"_s;
const QString k_agentPath = u"/org/fiordland/crest/BluetoothAgent"_s;
const QString k_capability = u"KeyboardDisplay"_s;
const QString k_errRejected = u"org.bluez.Error.Rejected"_s;
const QString k_errCanceled = u"org.bluez.Error.Canceled"_s;
constexpr int k_timeoutMs = 55000;

BluetoothAgent* s_active = nullptr;

QString addressFromPath(const QString& path) {
    const auto idx = path.lastIndexOf(u"/dev_"_s);
    if (idx < 0)
        return {};
    return path.mid(idx + 5).replace(u'_', u':');
}
} // namespace

BluetoothAgentAdaptor::BluetoothAgentAdaptor(BluetoothAgent* agent)
    : QDBusAbstractAdaptor(agent)
    , m_agent(agent) {}

void BluetoothAgentAdaptor::Release() {
    m_agent->releasedByBluez();
}

QString BluetoothAgentAdaptor::RequestPinCode(const QDBusObjectPath& device, const QDBusMessage& msg) {
    m_agent->beginRequest(BluetoothAgent::RequestType::PinCode, device, msg);
    return {};
}

void BluetoothAgentAdaptor::DisplayPinCode(const QDBusObjectPath& device, const QString& pincode) {
    m_agent->showCode(BluetoothAgent::RequestType::DisplayPinCode, device, pincode, 0);
}

uint BluetoothAgentAdaptor::RequestPasskey(const QDBusObjectPath& device, const QDBusMessage& msg) {
    m_agent->beginRequest(BluetoothAgent::RequestType::Passkey, device, msg);
    return 0;
}

void BluetoothAgentAdaptor::DisplayPasskey(const QDBusObjectPath& device, uint passkey, ushort entered) {
    m_agent->showCode(
        BluetoothAgent::RequestType::DisplayPasskey, device, u"%1"_s.arg(passkey, 6, 10, QChar(u'0')), entered);
}

void BluetoothAgentAdaptor::RequestConfirmation(const QDBusObjectPath& device, uint passkey, const QDBusMessage& msg) {
    m_agent->beginRequest(
        BluetoothAgent::RequestType::Confirm, device, msg, u"%1"_s.arg(passkey, 6, 10, QChar(u'0')));
}

void BluetoothAgentAdaptor::RequestAuthorization(const QDBusObjectPath& device, const QDBusMessage& msg) {
    m_agent->beginRequest(BluetoothAgent::RequestType::Authorize, device, msg);
}

void BluetoothAgentAdaptor::AuthorizeService(
    const QDBusObjectPath& device, const QString& uuid, const QDBusMessage& msg) {
    m_agent->beginRequest(BluetoothAgent::RequestType::AuthorizeService, device, msg, {}, uuid);
}

void BluetoothAgentAdaptor::Cancel() {
    m_agent->cancelFromBluez();
}

BluetoothAgent::BluetoothAgent(QObject* parent)
    : QObject(parent)
    , m_bus(QDBusConnection::systemBus())
    , m_watcher(new QDBusServiceWatcher(this)) {
    new BluetoothAgentAdaptor(this);

    m_timeout.setSingleShot(true);
    m_timeout.setInterval(k_timeoutMs);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        if (m_type == RequestType::None)
            return;
        qCInfo(lcBtAgent) << "Request for" << m_devicePath << "timed out";
        const bool replied = m_hasPending;
        const auto name = m_deviceName;
        replyError(k_errCanceled, u"Timed out waiting for the user"_s);
        clear();
        if (replied)
            emit failed(name, FailReason::TimedOut);
    });

    if (!m_bus.isConnected()) {
        qCWarning(lcBtAgent) << "No system bus, Bluetooth pairing agent disabled:" << m_bus.lastError().message();
        return;
    }

    if (s_active)
        s_active->m_exported = false;
    s_active = this;

    m_bus.unregisterObject(k_agentPath);
    if (!m_bus.registerObject(k_agentPath, this, QDBusConnection::ExportAdaptors)) {
        qCWarning(lcBtAgent) << "Failed to export agent object:" << m_bus.lastError().message();
        return;
    }
    m_exported = true;

    m_watcher->setConnection(m_bus);
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForRegistration | QDBusServiceWatcher::WatchForUnregistration);
    m_watcher->addWatchedService(k_service);
    connect(m_watcher, &QDBusServiceWatcher::serviceRegistered, this, [this] {
        qCInfo(lcBtAgent) << "bluetoothd appeared, registering agent";
        registerWithBluez();
    });
    connect(m_watcher, &QDBusServiceWatcher::serviceUnregistered, this, [this] {
        qCInfo(lcBtAgent) << "bluetoothd went away";
        m_hasPending = false;
        clear();
        setRegistered(false);
    });

    registerWithBluez();
}

BluetoothAgent::~BluetoothAgent() {
    if (m_hasPending)
        replyError(k_errCanceled, u"Agent shutting down"_s);

    if (s_active != this)
        return;
    s_active = nullptr;

    if (!m_bus.isConnected() || !m_exported)
        return;

    if (m_registered) {
        auto msg = QDBusMessage::createMethodCall(k_service, k_managerPath, k_managerIface, u"UnregisterAgent"_s);
        msg.setArguments({ QVariant::fromValue(QDBusObjectPath(k_agentPath)) });
        m_bus.call(msg, QDBus::Block, 1000);
    }
    m_bus.unregisterObject(k_agentPath);
}

void BluetoothAgent::registerWithBluez() {
    if (s_active != this || !m_exported)
        return;

    auto msg = QDBusMessage::createMethodCall(k_service, k_managerPath, k_managerIface, u"RegisterAgent"_s);
    msg.setArguments({ QVariant::fromValue(QDBusObjectPath(k_agentPath)), k_capability });

    auto* watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* self) {
        self->deleteLater();
        const QDBusPendingReply<> reply = *self;
        if (reply.isError() && reply.error().name() != u"org.bluez.Error.AlreadyExists"_s) {
            if (reply.error().type() == QDBusError::ServiceUnknown)
                qCInfo(lcBtAgent) << "bluetoothd is not running, will register when it starts";
            else
                qCWarning(lcBtAgent) << "RegisterAgent failed:" << reply.error().message();
            setRegistered(false);
            return;
        }

        auto def = QDBusMessage::createMethodCall(k_service, k_managerPath, k_managerIface, u"RequestDefaultAgent"_s);
        def.setArguments({ QVariant::fromValue(QDBusObjectPath(k_agentPath)) });
        auto* defWatcher = new QDBusPendingCallWatcher(m_bus.asyncCall(def), this);
        connect(defWatcher, &QDBusPendingCallWatcher::finished, this, [this](QDBusPendingCallWatcher* w) {
            w->deleteLater();
            const QDBusPendingReply<> r = *w;
            if (r.isError())
                qCWarning(lcBtAgent) << "RequestDefaultAgent failed:" << r.error().message();
            else
                qCInfo(lcBtAgent) << "Registered as the default Bluetooth agent";
            setRegistered(true);
        });
    });
}

void BluetoothAgent::setRegistered(bool registered) {
    if (m_registered == registered)
        return;
    m_registered = registered;
    emit registeredChanged();
}

bool BluetoothAgent::registered() const {
    return m_registered;
}

BluetoothAgent::RequestType BluetoothAgent::type() const {
    return m_type;
}

bool BluetoothAgent::needsReply() const {
    return m_hasPending;
}

QString BluetoothAgent::devicePath() const {
    return m_devicePath;
}

QString BluetoothAgent::deviceName() const {
    return m_deviceName;
}

QString BluetoothAgent::deviceAddress() const {
    return m_deviceAddress;
}

QString BluetoothAgent::deviceIcon() const {
    return m_deviceIcon;
}

QString BluetoothAgent::code() const {
    return m_code;
}

int BluetoothAgent::entered() const {
    return m_entered;
}

QString BluetoothAgent::service() const {
    return m_service;
}

void BluetoothAgent::beginRequest(RequestType type, const QDBusObjectPath& device, const QDBusMessage& msg,
    const QString& code, const QString& service) {
    msg.setDelayedReply(true);

    if (m_hasPending) {
        replyError(k_errCanceled, u"Superseded by a newer request"_s);
        clear();
    }

    const auto path = device.path();

    if (type == RequestType::AuthorizeService) {
        if (isPaired(path)) {
            m_bus.send(msg.createReply());
            return;
        }
    }

    m_type = type;
    m_pending = msg;
    m_hasPending = true;
    m_devicePath = path;
    m_code = code;
    m_service = service;
    m_entered = 0;
    m_accepted = false;
    m_timeout.start();

    loadDeviceInfo(path);
    watchDevice(path);

    qCInfo(lcBtAgent) << "Request" << type << "for" << path;
    emit requestChanged();
    emit enteredChanged();
}

void BluetoothAgent::showCode(RequestType type, const QDBusObjectPath& device, const QString& code, int entered) {
    const auto path = device.path();

    if (m_type == type && m_devicePath == path && m_code == code) {
        if (m_entered != entered) {
            m_entered = entered;
            emit enteredChanged();
        }
        m_timeout.start();
        return;
    }

    if (m_hasPending) {
        replyError(k_errCanceled, u"Superseded by a newer request"_s);
        clear();
    }

    m_type = type;
    m_devicePath = path;
    m_code = code;
    m_service.clear();
    m_entered = entered;
    m_accepted = true;
    m_timeout.start();

    loadDeviceInfo(path);
    watchDevice(path);

    emit requestChanged();
    emit enteredChanged();
}

void BluetoothAgent::cancelFromBluez() {
    if (m_type == RequestType::None)
        return;

    qCInfo(lcBtAgent) << "bluetoothd cancelled the request for" << m_devicePath;
    const auto name = m_deviceName;
    m_hasPending = false;
    clear();
    emit failed(name, FailReason::Cancelled);
}

void BluetoothAgent::releasedByBluez() {
    qCInfo(lcBtAgent) << "bluetoothd released the agent";
    m_hasPending = false;
    clear();
    setRegistered(false);
}

void BluetoothAgent::accept() {
    if (!m_hasPending)
        return;

    switch (m_type) {
    case RequestType::Confirm:
    case RequestType::Authorize:
    case RequestType::AuthorizeService:
        m_bus.send(m_pending.createReply());
        m_hasPending = false;
        m_accepted = true;
        finishReplied();
        break;
    default:
        qCWarning(lcBtAgent) << "accept() called for a request that needs input, use submit()";
        break;
    }
}

void BluetoothAgent::reject() {
    if (m_type == RequestType::None)
        return;

    if (m_hasPending)
        replyError(k_errRejected, u"Rejected by the user"_s);
    m_accepted = false;
    clear();
}

bool BluetoothAgent::submit(const QString& input) {
    if (!m_hasPending)
        return false;

    const auto text = input.trimmed();

    if (m_type == RequestType::Passkey) {
        static const QRegularExpression re(u"^\\d{1,6}$"_s);
        if (!re.match(text).hasMatch())
            return false;
        m_bus.send(m_pending.createReply(QVariant::fromValue(static_cast<uint>(text.toUInt()))));
    } else if (m_type == RequestType::PinCode) {
        if (text.isEmpty() || text.size() > 16)
            return false;
        m_bus.send(m_pending.createReply(text));
    } else {
        qCWarning(lcBtAgent) << "submit() called for a request that doesn't take input";
        return false;
    }

    m_hasPending = false;
    m_accepted = true;
    finishReplied();
    return true;
}

void BluetoothAgent::finishReplied() {
    m_timeout.stop();
    m_type = RequestType::None;
    m_code.clear();
    m_service.clear();
    m_entered = 0;
    emit requestChanged();
    emit enteredChanged();
}

void BluetoothAgent::replyError(const QString& name, const QString& message) {
    if (!m_hasPending)
        return;
    m_bus.send(m_pending.createErrorReply(name, message));
    m_hasPending = false;
}

void BluetoothAgent::clear() {
    m_timeout.stop();
    m_pending = {};
    m_hasPending = false;
    if (!m_accepted)
        unwatchDevice();

    const bool hadRequest = m_type != RequestType::None;
    m_type = RequestType::None;
    m_devicePath.clear();
    m_code.clear();
    m_service.clear();
    m_entered = 0;
    if (hadRequest) {
        emit requestChanged();
        emit enteredChanged();
    }
}

void BluetoothAgent::watchDevice(const QString& path) {
    if (m_watchedPath == path)
        return;
    unwatchDevice();
    if (m_bus.connect(k_service, path, k_propsIface, u"PropertiesChanged"_s, this,
            SLOT(handleDeviceProperties(QString, QVariantMap, QStringList))))
        m_watchedPath = path;
}

void BluetoothAgent::unwatchDevice() {
    if (m_watchedPath.isEmpty())
        return;
    m_bus.disconnect(k_service, m_watchedPath, k_propsIface, u"PropertiesChanged"_s, this,
        SLOT(handleDeviceProperties(QString, QVariantMap, QStringList)));
    m_watchedPath.clear();
}

void BluetoothAgent::handleDeviceProperties(
    const QString& iface, const QVariantMap& changed, const QStringList& invalidated) {
    Q_UNUSED(invalidated)
    if (iface != k_deviceIface)
        return;

    if (changed.contains(u"Alias"_s)) {
        m_deviceName = changed.value(u"Alias"_s).toString();
        emit deviceInfoChanged();
    }

    if (!changed.value(u"Paired"_s).toBool())
        return;

    const auto path = m_watchedPath;
    qCInfo(lcBtAgent) << "Paired with" << path;

    if (m_type == RequestType::DisplayPasskey || m_type == RequestType::DisplayPinCode)
        clear();

    if (m_accepted) {
        auto msg = QDBusMessage::createMethodCall(k_service, path, k_propsIface, u"Set"_s);
        msg.setArguments({ k_deviceIface, u"Trusted"_s, QVariant::fromValue(QDBusVariant(true)) });
        m_bus.asyncCall(msg);
        m_accepted = false;
    }

    if (m_type == RequestType::None)
        unwatchDevice();
}

void BluetoothAgent::loadDeviceInfo(const QString& path) {
    if (path != m_infoPath) {
        m_infoPath = path;
        m_deviceAddress = addressFromPath(path);
        m_deviceName = m_deviceAddress;
        m_deviceIcon.clear();
        emit deviceInfoChanged();
    }

    auto msg = QDBusMessage::createMethodCall(k_service, path, k_propsIface, u"GetAll"_s);
    msg.setArguments({ k_deviceIface });
    auto* watcher = new QDBusPendingCallWatcher(m_bus.asyncCall(msg), this);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, path](QDBusPendingCallWatcher* self) {
        self->deleteLater();
        const QDBusPendingReply<QVariantMap> reply = *self;
        if (reply.isError() || path != m_devicePath)
            return;
        const auto props = reply.value();
        const auto alias = props.value(u"Alias"_s).toString();
        if (!alias.isEmpty())
            m_deviceName = alias;
        const auto address = props.value(u"Address"_s).toString();
        if (!address.isEmpty())
            m_deviceAddress = address;
        m_deviceIcon = props.value(u"Icon"_s).toString();
        emit deviceInfoChanged();
    });
}

bool BluetoothAgent::isPaired(const QString& path) const {
    auto msg = QDBusMessage::createMethodCall(k_service, path, k_propsIface, u"Get"_s);
    msg.setArguments({ k_deviceIface, u"Paired"_s });
    const QDBusReply<QDBusVariant> reply = m_bus.call(msg, QDBus::Block, 500);
    return reply.isValid() && reply.value().variant().toBool();
}

} // namespace caelestia::services
