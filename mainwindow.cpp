#include "mainwindow.h"
#include "./ui_mainwindow.h"

#include <QComboBox>
#include <QCheckBox>
#include <QDateTime>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLocale>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSettings>
#include <QSet>
#include <QStatusBar>
#include <QTableWidgetItem>
#include <QTextDocument>
#include <QTimeZone>
#include <QWidget>
#include <cmath>

class SortTableWidgetItem : public QTableWidgetItem
{
public:
    explicit SortTableWidgetItem(const QString &text = QString())
        : QTableWidgetItem(text)
    {
    }

    bool operator<(const QTableWidgetItem &other) const override
    {
        const QVariant left = data(Qt::UserRole);
        const QVariant right = other.data(Qt::UserRole);
        if (left.isValid() && right.isValid()) {
            return left.toDouble() < right.toDouble();
        }
        return QTableWidgetItem::operator<(other);
    }
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_gpsdClient(new QGpsdClient(this))
{
    ui->setupUi(this);
    setWindowTitle("GPS Sky View");
    configureControlBar();
    configureSatelliteTable();
    connectControls();
    loadSettings();
    ui->rawJsonEdit->setVisible(ui->rawJsonCheckBox->isChecked());

    connect(m_gpsdClient, &QGpsdClient::satellitesUpdated,
            ui->graphicsView, &QGpsSkyView::setSatellites);
    connect(m_gpsdClient, &QGpsdClient::satellitesUpdated,
            this, &MainWindow::updateSatelliteTable);
    connect(m_gpsdClient, &QGpsdClient::tpvUpdated,
            this, &MainWindow::updateTpvDisplay);
    connect(m_gpsdClient, &QGpsdClient::rawJsonReceived,
            this, &MainWindow::appendRawJson);
    connect(m_gpsdClient, &QGpsdClient::connectionStateChanged,
            this, [this](bool connected) {
                setConnectionButtonState(connected ? QStringLiteral("Disconnect")
                                                   : QStringLiteral("Connect"),
                                         connected ? QStringLiteral("#b00020") : QString());
            });
    connect(m_gpsdClient, &QGpsdClient::statusMessage,
            this, [this](const QString &message) {
                if (message.startsWith(QStringLiteral("Connecting"))
                    || message.startsWith(QStringLiteral("Connected"))
                    || message.startsWith(QStringLiteral("Disconnected"))
                    || message.startsWith(QStringLiteral("gpsd error"))) {
                    ui->statusbar->showMessage(message);
                }
                if (message.startsWith(QStringLiteral("Connected"))) {
                    m_reconnectAttempts = 0;
                    m_reconnectTimer.stop();
                } else if (!m_userReconnect
                           && !m_manualDisconnect
                           && (message.startsWith(QStringLiteral("Disconnected"))
                               || message.startsWith(QStringLiteral("gpsd error")))) {
                    scheduleReconnect();
                }
                ui->statusbar->showMessage(message);
            });

    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout,
            this, &MainWindow::reconnectGpsd);

    reconnectGpsd();
}

MainWindow::~MainWindow()
{
    saveSettings();
    delete ui;
}

void MainWindow::connectControls()
{
    connect(ui->connectButton, &QPushButton::clicked,
            this, &MainWindow::toggleGpsdConnection);

    connect(ui->graphicsView, &QGpsSkyView::rotationChanged,
            this, [this]() {
                saveSettings();
            });
    connect(ui->graphicsView, &QGpsSkyView::legendVisibleChanged,
            this, [this]() {
                saveSettings();
            });
    connect(ui->graphicsView, &QGpsSkyView::markerLabelsVisibleChanged,
            this, [this]() {
                saveSettings();
            });
    connect(ui->graphicsView, &QGpsSkyView::viewReset,
            this, [this]() {
                saveSettings();
            });

    connect(ui->gridStepComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                ui->graphicsView->setGridStep(index == 1 ? 45 : 30);
                saveSettings();
            });

    connect(ui->projectionComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                ui->graphicsView->setElRadiusMode(index == 1);
                saveSettings();
            });

    connect(ui->speedUnitComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                switch (index) {
                case 1:
                    m_speedUnit = SpeedUnit::KilometersPerHour;
                    break;
                case 2:
                    m_speedUnit = SpeedUnit::Knots;
                    break;
                case 3:
                    m_speedUnit = SpeedUnit::MilesPerHour;
                    break;
                default:
                    m_speedUnit = SpeedUnit::MetersPerSecond;
                    break;
                }

                if (m_hasLastTpv) {
                    updateTpvDisplay(m_lastTpv);
                }
                saveSettings();
            });

    connect(ui->coordFormatComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                switch (index) {
                case 1:
                    m_coordFormat = CoordFormat::DegreesMinutes;
                    break;
                case 2:
                    m_coordFormat = CoordFormat::DegreesMinutesSeconds;
                    break;
                default:
                    m_coordFormat = CoordFormat::DecimalDegrees;
                    break;
                }

                if (m_hasLastTpv) {
                    updateTpvDisplay(m_lastTpv);
                }
                saveSettings();
            });

    connect(ui->timeFormatComboBox, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_timeFormat = index == 1 ? TimeFormat::Local : TimeFormat::Utc;
                if (m_hasLastTpv) {
                    updateTpvDisplay(m_lastTpv);
                }
                saveSettings();
            });

    connect(ui->rawJsonCheckBox, &QCheckBox::toggled,
            this, [this](bool checked) {
                ui->rawJsonEdit->setVisible(checked);
                saveSettings();
            });

    connect(ui->resetSettingsButton, &QPushButton::clicked,
            this, &MainWindow::resetSettings);
}

void MainWindow::toggleGpsdConnection()
{
    if (m_reconnectTimer.isActive()) {
        disconnectGpsd();
    } else if (m_gpsdClient->isConnected() || m_gpsdClient->isConnecting()) {
        disconnectGpsd();
    } else {
        m_manualDisconnect = false;
        m_reconnectAttempts = 0;
        m_userReconnect = true;
        reconnectGpsd();
        m_userReconnect = false;
    }
}

void MainWindow::loadSettings()
{
    QSettings settings(QStringLiteral("GPS_SKY_VIEW"), QStringLiteral("GPS_SKY_VIEW"));

    ui->hostEdit->setText(settings.value(QStringLiteral("gpsd/host"), QStringLiteral("localhost")).toString());
    ui->portSpinBox->setValue(settings.value(QStringLiteral("gpsd/port"), 2947).toInt());
    ui->graphicsView->setRotation(settings.value(QStringLiteral("view/rotation"), 0.0).toDouble());
    ui->gridStepComboBox->setCurrentIndex(settings.value(QStringLiteral("view/gridStep"), 30).toInt() == 45 ? 1 : 0);
    ui->projectionComboBox->setCurrentIndex(settings.value(QStringLiteral("view/projection"), 0).toInt());
    ui->speedUnitComboBox->setCurrentIndex(settings.value(QStringLiteral("display/speedUnit"), 0).toInt());
    ui->coordFormatComboBox->setCurrentIndex(settings.value(QStringLiteral("display/coordFormat"), 0).toInt());
    ui->timeFormatComboBox->setCurrentIndex(settings.value(QStringLiteral("display/timeFormat"), 0).toInt());
    ui->rawJsonCheckBox->setChecked(settings.value(QStringLiteral("debug/showRawJson"), false).toBool());
    ui->graphicsView->setLegendVisible(settings.value(QStringLiteral("view/showLegend"), true).toBool());
    ui->graphicsView->setMarkerLabelsVisible(settings.value(QStringLiteral("view/showPrnLabels"), true).toBool());
}

void MainWindow::saveSettings() const
{
    QSettings settings(QStringLiteral("GPS_SKY_VIEW"), QStringLiteral("GPS_SKY_VIEW"));
    settings.setValue(QStringLiteral("gpsd/host"), ui->hostEdit->text().trimmed());
    settings.setValue(QStringLiteral("gpsd/port"), ui->portSpinBox->value());
    settings.setValue(QStringLiteral("view/rotation"), ui->graphicsView->rotation());
    settings.setValue(QStringLiteral("view/gridStep"), ui->gridStepComboBox->currentIndex() == 1 ? 45 : 30);
    settings.setValue(QStringLiteral("view/projection"), ui->projectionComboBox->currentIndex());
    settings.setValue(QStringLiteral("view/showLegend"), ui->graphicsView->isLegendVisible());
    settings.setValue(QStringLiteral("view/showPrnLabels"), ui->graphicsView->markerLabelsVisible());
    settings.setValue(QStringLiteral("display/speedUnit"), ui->speedUnitComboBox->currentIndex());
    settings.setValue(QStringLiteral("display/coordFormat"), ui->coordFormatComboBox->currentIndex());
    settings.setValue(QStringLiteral("display/timeFormat"), ui->timeFormatComboBox->currentIndex());
    settings.setValue(QStringLiteral("debug/showRawJson"), ui->rawJsonCheckBox->isChecked());
}

void MainWindow::resetSettings()
{
    const QMessageBox::StandardButton answer = QMessageBox::question(
        this,
        QStringLiteral("Reset Settings"),
        QStringLiteral("Reset saved settings to defaults?"));

    if (answer != QMessageBox::Yes) {
        return;
    }

    m_manualDisconnect = true;
    m_reconnectTimer.stop();
    m_gpsdClient->disconnectFromServer();

    QSettings settings(QStringLiteral("GPS_SKY_VIEW"), QStringLiteral("GPS_SKY_VIEW"));
    settings.clear();

    ui->hostEdit->setText(QStringLiteral("localhost"));
    ui->portSpinBox->setValue(2947);
    ui->gridStepComboBox->setCurrentIndex(0);
    ui->projectionComboBox->setCurrentIndex(0);
    ui->speedUnitComboBox->setCurrentIndex(0);
    ui->coordFormatComboBox->setCurrentIndex(0);
    ui->timeFormatComboBox->setCurrentIndex(0);
    ui->rawJsonCheckBox->setChecked(false);
    ui->rawJsonEdit->clear();
    ui->graphicsView->setLegendVisible(true);
    ui->graphicsView->setMarkerLabelsVisible(true);
    ui->graphicsView->resetView();
    ui->graphicsView->setSatellites({});
    updateSatelliteTable({});
    setConnectionButtonState(QStringLiteral("Connect"));
    ui->statusbar->showMessage(QStringLiteral("Settings reset"));
}

void MainWindow::disconnectGpsd()
{
    m_userReconnect = true;
    m_manualDisconnect = true;
    m_reconnectAttempts = 0;
    m_reconnectTimer.stop();
    m_gpsdClient->disconnectFromServer();
    setConnectionButtonState(QStringLiteral("Connect"));
    ui->statusbar->showMessage(QStringLiteral("Disconnecting..."));
}

void MainWindow::reconnectGpsd()
{
    m_manualDisconnect = false;
    saveSettings();
    m_reconnectTimer.stop();
    ui->graphicsView->setSatellites({});
    updateSatelliteTable({});
    setConnectionButtonState(QStringLiteral("Connecting..."), QStringLiteral("#0057b8"));
    ui->statusbar->showMessage(QStringLiteral("Connecting..."));
    m_gpsdClient->connectToServer(ui->hostEdit->text().trimmed(),
                                  static_cast<quint16>(ui->portSpinBox->value()));
}

void MainWindow::scheduleReconnect()
{
    constexpr int maxReconnectAttempts = 5;
    if (m_reconnectTimer.isActive()) {
        return;
    }

    if (m_reconnectAttempts >= maxReconnectAttempts) {
        setConnectionButtonState(QStringLiteral("Connect"));
        ui->statusbar->showMessage(QStringLiteral("Auto reconnect stopped after 5 attempts"));
        return;
    }

    ++m_reconnectAttempts;
    setConnectionButtonState(QStringLiteral("Cancel Reconnect"), QStringLiteral("#8a5a00"));
    ui->statusbar->showMessage(QStringLiteral("Reconnecting in 3 seconds... (%1/%2)")
                                   .arg(m_reconnectAttempts)
                                   .arg(maxReconnectAttempts));
    m_reconnectTimer.start(3000);
}

void MainWindow::setConnectionButtonState(const QString &text, const QString &color)
{
    ui->connectButton->setText(text);
    if (color.isEmpty()) {
        ui->connectButton->setStyleSheet(QStringLiteral("QPushButton { color: palette(button-text); font-weight: 400; }"));
        return;
    }

    ui->connectButton->setStyleSheet(QStringLiteral("QPushButton { color: %1; font-weight: 600; }").arg(color));
}

void MainWindow::appendRawJson(const QString &line)
{
    if (!ui->rawJsonCheckBox->isChecked()) {
        return;
    }

    constexpr int maxRawJsonBlocks = 500;
    ui->rawJsonEdit->appendPlainText(line);
    QTextDocument *document = ui->rawJsonEdit->document();
    if (document) {
        document->setMaximumBlockCount(maxRawJsonBlocks);
    }
}

void MainWindow::updateSatelliteTable(const QList<QGpsSatItem::Data> &satellites)
{
    ui->satelliteTable->setSortingEnabled(false);

    auto setText = [this](int row, int column, const QString &text, const QVariant &sortValue = QVariant()) {
        QTableWidgetItem *item = ui->satelliteTable->item(row, column);
        if (!item) {
            item = new SortTableWidgetItem;
            item->setTextAlignment(Qt::AlignCenter);
            ui->satelliteTable->setItem(row, column, item);
        }
        item->setText(text);
        item->setData(Qt::UserRole, sortValue);
    };

    QSet<QString> activeKeys;

    for (const QGpsSatItem::Data &satellite : satellites) {
        const QString key = satelliteKey(satellite);
        activeKeys.insert(key);

        int row = -1;
        for (int currentRow = 0; currentRow < ui->satelliteTable->rowCount(); ++currentRow) {
            QTableWidgetItem *keyItem = ui->satelliteTable->item(currentRow, 0);
            if (keyItem && keyItem->data(Qt::UserRole + 1).toString() == key) {
                row = currentRow;
                break;
            }
        }

        if (row < 0) {
            row = ui->satelliteTable->rowCount();
            ui->satelliteTable->insertRow(row);
        }

        setText(row, 0, gnssName(satellite.type));
        ui->satelliteTable->item(row, 0)->setData(Qt::UserRole + 1, key);
        setText(row, 1, satellite.svid > 0 ? QString::number(satellite.svid) : QStringLiteral("--"), satellite.svid);
        setText(row, 2, satellite.hasSigid ? QString::number(satellite.sigid) : QStringLiteral("--"), satellite.hasSigid ? QVariant(satellite.sigid) : QVariant());
        setText(row, 3, satellite.PRN > 0 ? QString::number(satellite.PRN) : QStringLiteral("--"), satellite.PRN);
        setText(row, 4, QLocale::c().toString(satellite.Elev, 'f', 1), satellite.Elev);
        setText(row, 5, QLocale::c().toString(satellite.az, 'f', 1), satellite.az);
        setText(row, 6, QLocale::c().toString(satellite.ss, 'f', 1), satellite.ss);
        setText(row, 7, satellite.hasQual ? QLocale::c().toString(satellite.qual, 'f', 0) : QStringLiteral("--"), satellite.hasQual ? QVariant(satellite.qual) : QVariant());
        setText(row, 8, satellite.hasPrRes ? QLocale::c().toString(satellite.qrRes, 'f', 1) : QStringLiteral("--"), satellite.hasPrRes ? QVariant(satellite.qrRes) : QVariant());
        setText(row, 9, healthText(satellite));
        setText(row, 10, satellite.used ? QStringLiteral("Yes") : QStringLiteral("No"));
    }

    for (int row = ui->satelliteTable->rowCount() - 1; row >= 0; --row) {
        QTableWidgetItem *keyItem = ui->satelliteTable->item(row, 0);
        if (!keyItem || !activeKeys.contains(keyItem->data(Qt::UserRole + 1).toString())) {
            ui->satelliteTable->removeRow(row);
        }
    }

    ui->satelliteTable->setSortingEnabled(true);
}

void MainWindow::configureControlBar()
{
    ui->controlLayout->setContentsMargins(0, 0, 0, 0);
    ui->controlLayout->setSpacing(6);
    ui->hostEdit->setMinimumWidth(190);
    ui->hostEdit->setMaximumWidth(260);
    ui->portSpinBox->setMaximumWidth(74);
    ui->connectButton->setFlat(true);
    ui->resetSettingsButton->setFlat(true);
    ui->centralwidget->setStyleSheet(QStringLiteral(
        "QWidget#centralwidget { background: palette(window); }"
        "QLineEdit, QSpinBox, QComboBox { min-height: 22px; }"
        "QCheckBox { min-height: 22px; }"
        "QPushButton { min-height: 22px; padding: 2px 8px; }"
    ));
}

void MainWindow::updateTpvDisplay(const QGpsTpvData &tpv)
{
    m_lastTpv = tpv;
    m_hasLastTpv = true;

    ui->timeValue->setText(timeText(tpv.time));
    ui->modeValue->setText(modeText(tpv.mode));
    ui->latValue->setText(coordinateText(tpv.hasLat, tpv.lat, true));
    ui->lonValue->setText(coordinateText(tpv.hasLon, tpv.lon, false));
    ui->altValue->setText(valueOrDash(tpv.hasAlt, tpv.alt, 1, QStringLiteral(" m")));
    ui->speedValue->setText(speedText(tpv.hasSpeed, tpv.speed));
    ui->trackValue->setText(valueOrDash(tpv.hasTrack, tpv.track, 1, QStringLiteral(" deg")));
    ui->climbValue->setText(climbText(tpv.hasClimb, tpv.climb));
}

void MainWindow::configureSatelliteTable()
{
    ui->satelliteTable->verticalHeader()->setVisible(false);
    ui->satelliteTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->satelliteTable->horizontalHeader()->setStretchLastSection(true);
    ui->satelliteTable->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->satelliteTable->setSortingEnabled(true);
}

QString MainWindow::gnssName(QGpsSatItem::QGnnsType type) const
{
    switch (type) {
    case QGpsSatItem::GPS:
        return QStringLiteral("GPS");
    case QGpsSatItem::SBAS:
        return QStringLiteral("SBAS");
    case QGpsSatItem::Galileo:
        return QStringLiteral("Galileo");
    case QGpsSatItem::BeiDou:
        return QStringLiteral("BeiDou");
    case QGpsSatItem::IMES:
        return QStringLiteral("IMES");
    case QGpsSatItem::QZSS:
        return QStringLiteral("QZSS");
    case QGpsSatItem::GLONASS:
        return QStringLiteral("GLONASS");
    case QGpsSatItem::IRNSS:
        return QStringLiteral("IRNSS");
    }

    return QStringLiteral("GNSS");
}

QString MainWindow::healthText(const QGpsSatItem::Data &satellite) const
{
    if (!satellite.hasHealth) {
        return QStringLiteral("--");
    }

    switch (satellite.health) {
    case 1:
        return QStringLiteral("OK");
    case 2:
        return QStringLiteral("Bad");
    default:
        return QStringLiteral("Unknown");
    }
}

QString MainWindow::satelliteKey(const QGpsSatItem::Data &satellite) const
{
    return QStringLiteral("%1:%2:%3:%4")
        .arg(static_cast<int>(satellite.type))
        .arg(satellite.svid)
        .arg(satellite.PRN)
        .arg(satellite.hasSigid ? satellite.sigid : -1);
}

QString MainWindow::modeText(int mode) const
{
    switch (mode) {
    case 1:
        return QStringLiteral("No fix");
    case 2:
        return QStringLiteral("2D");
    case 3:
        return QStringLiteral("3D");
    default:
        return QStringLiteral("--");
    }
}

QString MainWindow::timeText(const QString &time) const
{
    if (time.isEmpty()) {
        return QStringLiteral("--");
    }

    QDateTime dateTime = QDateTime::fromString(time, Qt::ISODateWithMs);
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(time, Qt::ISODate);
    }
    if (!dateTime.isValid()) {
        return time;
    }

    if (dateTime.timeSpec() == Qt::LocalTime) {
        dateTime = QDateTime(dateTime.date(), dateTime.time(), QTimeZone::UTC);
    }

    if (m_timeFormat == TimeFormat::Local) {
        return dateTime.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss t"));
    }

    return dateTime.toUTC().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"));
}

QString MainWindow::coordinateText(bool hasValue, double degrees, bool latitude) const
{
    if (!hasValue) {
        return QStringLiteral("--");
    }

    const QString hemisphere = latitude
                                   ? (degrees < 0 ? QStringLiteral("S") : QStringLiteral("N"))
                                   : (degrees < 0 ? QStringLiteral("W") : QStringLiteral("E"));
    const double absolute = std::abs(degrees);
    const int wholeDegrees = static_cast<int>(absolute);
    const double minutesTotal = (absolute - wholeDegrees) * 60.0;
    const int wholeMinutes = static_cast<int>(minutesTotal);
    const double seconds = (minutesTotal - wholeMinutes) * 60.0;

    switch (m_coordFormat) {
    case CoordFormat::DegreesMinutes:
        return QStringLiteral("%1 %2 %3'")
            .arg(hemisphere)
            .arg(wholeDegrees)
            .arg(QLocale::c().toString(minutesTotal, 'f', 4));
    case CoordFormat::DegreesMinutesSeconds:
        return QStringLiteral("%1 %2 %3' %4\"")
            .arg(hemisphere)
            .arg(wholeDegrees)
            .arg(wholeMinutes)
            .arg(QLocale::c().toString(seconds, 'f', 2));
    case CoordFormat::DecimalDegrees:
        return QStringLiteral("%1 %2")
            .arg(hemisphere)
            .arg(QLocale::c().toString(absolute, 'f', 7));
    }

    return QLocale::c().toString(degrees, 'f', 7);
}

QString MainWindow::speedText(bool hasValue, double metersPerSecond) const
{
    if (!hasValue) {
        return QStringLiteral("--");
    }

    double value = metersPerSecond;
    QString suffix = QStringLiteral(" m/s");

    switch (m_speedUnit) {
    case SpeedUnit::KilometersPerHour:
        value = metersPerSecond * 3.6;
        suffix = QStringLiteral(" km/h");
        break;
    case SpeedUnit::Knots:
        value = metersPerSecond * 1.9438444924406;
        suffix = QStringLiteral(" kn");
        break;
    case SpeedUnit::MilesPerHour:
        value = metersPerSecond * 2.2369362920544;
        suffix = QStringLiteral(" mph");
        break;
    case SpeedUnit::MetersPerSecond:
        break;
    }

    return QLocale::c().toString(value, 'f', 2) + suffix;
}

QString MainWindow::climbText(bool hasValue, double metersPerSecond) const
{
    return speedText(hasValue, metersPerSecond);
}

QString MainWindow::valueOrDash(bool hasValue, double value, int precision, const QString &suffix) const
{
    if (!hasValue) {
        return QStringLiteral("--");
    }

    return QLocale::c().toString(value, 'f', precision) + suffix;
}
