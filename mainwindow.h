#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

#include "qgpsdclient.h"
#include "qgpsskyview.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    enum class SpeedUnit {
        MetersPerSecond,
        KilometersPerHour,
        Knots,
        MilesPerHour
    };

    enum class CoordFormat {
        DecimalDegrees,
        DegreesMinutes,
        DegreesMinutesSeconds
    };

    enum class TimeFormat {
        Utc,
        Local
    };

    void connectControls();
    void loadSettings();
    void saveSettings() const;
    void resetSettings();
    void toggleGpsdConnection();
    void reconnectGpsd();
    void disconnectGpsd();
    void scheduleReconnect();
    void setConnectionButtonState(const QString &text, const QString &color = QString());
    void appendRawJson(const QString &line);
    void updateSatelliteTable(const QList<QGpsSatItem::Data> &satellites);
    void updateTpvDisplay(const QGpsTpvData &tpv);
    void configureControlBar();
    void configureSatelliteTable();
    QString gnssName(QGpsSatItem::QGnnsType type) const;
    QString healthText(const QGpsSatItem::Data &satellite) const;
    QString satelliteKey(const QGpsSatItem::Data &satellite) const;
    QString modeText(int mode) const;
    QString timeText(const QString &time) const;
    QString coordinateText(bool hasValue, double degrees, bool latitude) const;
    QString speedText(bool hasValue, double metersPerSecond) const;
    QString climbText(bool hasValue, double metersPerSecond) const;
    QString valueOrDash(bool hasValue, double value, int precision, const QString &suffix = QString()) const;

    Ui::MainWindow *ui;
    QGpsdClient *m_gpsdClient;
    QTimer m_reconnectTimer;
    QGpsTpvData m_lastTpv;
    bool m_hasLastTpv = false;
    bool m_userReconnect = false;
    bool m_manualDisconnect = false;
    int m_reconnectAttempts = 0;
    SpeedUnit m_speedUnit = SpeedUnit::MetersPerSecond;
    CoordFormat m_coordFormat = CoordFormat::DecimalDegrees;
    TimeFormat m_timeFormat = TimeFormat::Utc;
};
#endif // MAINWINDOW_H
