#ifndef QGPSDCLIENT_H
#define QGPSDCLIENT_H

#include "qgpssatitem.h"

#include <QByteArray>
#include <QHostAddress>
#include <QList>
#include <QObject>
#include <QString>
#include <QTcpSocket>

class QJsonObject;

struct QGpsTpvData
{
    QString time;
    int mode = 0;
    double lat = 0.0;
    double lon = 0.0;
    double alt = 0.0;
    double speed = 0.0;
    double track = 0.0;
    double climb = 0.0;
    bool hasLat = false;
    bool hasLon = false;
    bool hasAlt = false;
    bool hasSpeed = false;
    bool hasTrack = false;
    bool hasClimb = false;
};

class QGpsdClient : public QObject
{
    Q_OBJECT

public:
    explicit QGpsdClient(QObject *parent = nullptr);

    void connectToServer(const QString &host, quint16 port = 2947);
    void disconnectFromServer();
    bool isConnected() const;
    bool isConnecting() const;

signals:
    void satellitesUpdated(const QList<QGpsSatItem::Data> &satellites);
    void tpvUpdated(const QGpsTpvData &tpv);
    void rawJsonReceived(const QString &line);
    void statusMessage(const QString &message);
    void connectionStateChanged(bool connected);

private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onReadyRead();

private:
    void processLine(const QByteArray &line);
    void processSky(const QJsonObject &sky);
    void processTpv(const QJsonObject &tpv);
    QGpsSatItem::Data satelliteFromJson(const QJsonObject &object) const;
    QGpsSatItem::QGnnsType gnssTypeFromJson(const QJsonObject &object) const;

    QTcpSocket m_socket;
    QByteArray m_buffer;
    QString m_host;
    quint16 m_port;
};

#endif // QGPSDCLIENT_H
