#include "qgpsdclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

QGpsdClient::QGpsdClient(QObject *parent)
    : QObject(parent)
    , m_port(2947)
{
    connect(&m_socket, &QTcpSocket::connected, this, &QGpsdClient::onConnected);
    connect(&m_socket, &QTcpSocket::disconnected, this, &QGpsdClient::onDisconnected);
    connect(&m_socket, &QTcpSocket::readyRead, this, &QGpsdClient::onReadyRead);
    connect(&m_socket, &QTcpSocket::errorOccurred, this, &QGpsdClient::onErrorOccurred);
}

void QGpsdClient::connectToServer(const QString &host, quint16 port)
{
    m_host = host;
    m_port = port;
    m_buffer.clear();

    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.abort();
    }

    emit statusMessage(QStringLiteral("Connecting to gpsd %1:%2").arg(m_host).arg(m_port));
    m_socket.connectToHost(m_host, m_port);
}

void QGpsdClient::disconnectFromServer()
{
    if (m_socket.state() == QAbstractSocket::UnconnectedState) {
        emit statusMessage(QStringLiteral("Disconnected from gpsd"));
        emit connectionStateChanged(false);
        return;
    }

    m_socket.disconnectFromHost();
}

bool QGpsdClient::isConnected() const
{
    return m_socket.state() == QAbstractSocket::ConnectedState;
}

bool QGpsdClient::isConnecting() const
{
    return m_socket.state() == QAbstractSocket::ConnectingState
           || m_socket.state() == QAbstractSocket::HostLookupState;
}

void QGpsdClient::onConnected()
{
    static const QByteArray watchCommand = "?WATCH={\"enable\":true,\"json\":true,\"scaled\":true};\n";
    m_socket.write(watchCommand);
    emit statusMessage(QStringLiteral("Connected to gpsd %1:%2").arg(m_host).arg(m_port));
    emit connectionStateChanged(true);
}

void QGpsdClient::onDisconnected()
{
    emit statusMessage(QStringLiteral("Disconnected from gpsd"));
    emit connectionStateChanged(false);
}

void QGpsdClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error);
    emit statusMessage(QStringLiteral("gpsd error: %1").arg(m_socket.errorString()));
    emit connectionStateChanged(false);
}

void QGpsdClient::onReadyRead()
{
    m_buffer.append(m_socket.readAll());

    int newline = m_buffer.indexOf('\n');
    while (newline >= 0) {
        const QByteArray line = m_buffer.left(newline).trimmed();
        m_buffer.remove(0, newline + 1);

        if (!line.isEmpty()) {
            emit rawJsonReceived(QString::fromUtf8(line));
            processLine(line);
        }

        newline = m_buffer.indexOf('\n');
    }
}

void QGpsdClient::processLine(const QByteArray &line)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit statusMessage(QStringLiteral("Ignored invalid gpsd JSON"));
        return;
    }

    const QJsonObject object = document.object();
    const QString packetClass = object.value(QStringLiteral("class")).toString();

    if (packetClass == QStringLiteral("SKY")) {
        processSky(object);
    } else if (packetClass == QStringLiteral("TPV")) {
        processTpv(object);
    }
}

void QGpsdClient::processSky(const QJsonObject &sky)
{
    if (!sky.contains(QStringLiteral("satellites"))
        || !sky.value(QStringLiteral("satellites")).isArray()) {
        return;
    }

    const QJsonArray satelliteArray = sky.value(QStringLiteral("satellites")).toArray();
    if (satelliteArray.isEmpty()) {
        return;
    }

    QList<QGpsSatItem::Data> satellites;
    satellites.reserve(satelliteArray.size());

    for (const QJsonValue &value : satelliteArray) {
        if (!value.isObject()) {
            continue;
        }
        satellites.append(satelliteFromJson(value.toObject()));
    }

    emit satellitesUpdated(satellites);
    emit statusMessage(QStringLiteral("SKY: %1 satellites").arg(satellites.size()));
}

void QGpsdClient::processTpv(const QJsonObject &tpv)
{
    QGpsTpvData data;
    data.time = tpv.value(QStringLiteral("time")).toString();
    data.mode = tpv.value(QStringLiteral("mode")).toInt(0);

    data.hasLat = tpv.contains(QStringLiteral("lat"));
    data.hasLon = tpv.contains(QStringLiteral("lon"));
    data.hasAlt = tpv.contains(QStringLiteral("altHAE"))
                  || tpv.contains(QStringLiteral("altMSL"))
                  || tpv.contains(QStringLiteral("alt"));
    data.hasSpeed = tpv.contains(QStringLiteral("speed"));
    data.hasTrack = tpv.contains(QStringLiteral("track"));
    data.hasClimb = tpv.contains(QStringLiteral("climb"));

    data.lat = tpv.value(QStringLiteral("lat")).toDouble();
    data.lon = tpv.value(QStringLiteral("lon")).toDouble();
    if (tpv.contains(QStringLiteral("altHAE"))) {
        data.alt = tpv.value(QStringLiteral("altHAE")).toDouble();
    } else if (tpv.contains(QStringLiteral("altMSL"))) {
        data.alt = tpv.value(QStringLiteral("altMSL")).toDouble();
    } else {
        data.alt = tpv.value(QStringLiteral("alt")).toDouble();
    }
    data.speed = tpv.value(QStringLiteral("speed")).toDouble();
    data.track = tpv.value(QStringLiteral("track")).toDouble();
    data.climb = tpv.value(QStringLiteral("climb")).toDouble();

    emit tpvUpdated(data);
}

QGpsSatItem::Data QGpsdClient::satelliteFromJson(const QJsonObject &object) const
{
    QGpsSatItem::Data satellite;
    satellite.type = gnssTypeFromJson(object);
    satellite.svid = object.value(QStringLiteral("svid")).toInt(0);
    satellite.PRN = object.value(QStringLiteral("PRN")).toInt(satellite.svid);
    satellite.sigid = object.value(QStringLiteral("sigid")).toInt(0);
    satellite.Elev = static_cast<float>(object.value(QStringLiteral("el")).toDouble(0.0));
    satellite.az = static_cast<float>(object.value(QStringLiteral("az")).toDouble(0.0));
    satellite.ss = static_cast<float>(object.value(QStringLiteral("ss")).toDouble(0.0));
    satellite.qual = static_cast<float>(object.value(QStringLiteral("qual")).toDouble(0.0));
    satellite.qrRes = static_cast<float>(object.value(QStringLiteral("prRes")).toDouble(0.0));
    satellite.used = object.value(QStringLiteral("used")).toBool(false);
    satellite.health = object.value(QStringLiteral("health")).toInt(0);
    satellite.hasSigid = object.contains(QStringLiteral("sigid"));
    satellite.hasQual = object.contains(QStringLiteral("qual"));
    satellite.hasPrRes = object.contains(QStringLiteral("prRes"));
    satellite.hasHealth = object.contains(QStringLiteral("health"));
    return satellite;
}

QGpsSatItem::QGnnsType QGpsdClient::gnssTypeFromJson(const QJsonObject &object) const
{
    if (object.contains(QStringLiteral("gnssid"))) {
        switch (object.value(QStringLiteral("gnssid")).toInt()) {
        case 0:
            return QGpsSatItem::GPS;
        case 1:
            return QGpsSatItem::SBAS;
        case 2:
            return QGpsSatItem::Galileo;
        case 3:
            return QGpsSatItem::BeiDou;
        case 4:
            return QGpsSatItem::IMES;
        case 5:
            return QGpsSatItem::QZSS;
        case 6:
            return QGpsSatItem::GLONASS;
        case 7:
            return QGpsSatItem::IRNSS;
        default:
            return QGpsSatItem::GPS;
        }
    }

    const int prn = object.value(QStringLiteral("PRN")).toInt();
    if (prn >= 65 && prn <= 96) {
        return QGpsSatItem::GLONASS;
    }
    if (prn >= 120 && prn <= 158) {
        return QGpsSatItem::SBAS;
    }
    if (prn >= 193 && prn <= 199) {
        return QGpsSatItem::QZSS;
    }
    if (prn >= 301 && prn <= 336) {
        return QGpsSatItem::Galileo;
    }
    if (prn >= 401 && prn <= 437) {
        return QGpsSatItem::BeiDou;
    }

    return QGpsSatItem::GPS;
}
