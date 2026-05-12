#include "qgpsskyview.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QGraphicsLineItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QMenu>
#include <QMouseEvent>
#include <QDateTime>
#include <QResizeEvent>
#include <QWheelEvent>
#include <QtMath>
#include <algorithm>
#include <cmath>
#include <iterator>

QGpsSkyView::QGpsSkyView(QWidget *parent)
    : QGraphicsView(parent)
    , m_padding(36.0)
    , m_radius(0.0)
    , m_scene(new QGraphicsScene(this))
{
    setScene(m_scene);
    setToolTip("GPS Sky View");
    setRenderHint(QPainter::Antialiasing, true);
    setFrameShape(QFrame::NoFrame);
    setBackgroundBrush(QBrush(Qt::black));

    rebuildGrid();
}

void QGpsSkyView::setSatellites(const QList<QGpsSatItem::Data> &satellites)
{
    m_satellites = satellites;
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const QGpsSatItem::Data &satellite : std::as_const(m_satellites)) {
        if (satellite.used) {
            m_last_used_ms.insert(satelliteKey(satellite), nowMs);
        }
    }
    rebuildSatellites();
}

void QGpsSkyView::setElRadiusMode(bool el_mode)
{
    if (m_el_radius == el_mode) {
        return;
    }

    m_el_radius = el_mode;
    rebuildGrid();
    rebuildSatellites();
}

void QGpsSkyView::setRotation(double rotation)
{
    const double normalizedRotation = [&rotation]() {
        double normalized = std::fmod(rotation, 360.0);
        if (normalized < 0) {
            normalized += 360.0;
        }
        return normalized;
    }();

    if (qFuzzyCompare(m_rotation + 1.0, normalizedRotation + 1.0)) {
        return;
    }

    m_rotation = normalizedRotation;
    rebuildGrid();
    rebuildSatellites();
    emit rotationChanged(m_rotation);
}

void QGpsSkyView::setGridStep(int degrees)
{
    const int normalized = degrees == 45 ? 45 : 30;
    if (m_grid_step == normalized) {
        return;
    }

    m_grid_step = normalized;
    rebuildGrid();
}

void QGpsSkyView::setLegendVisible(bool visible)
{
    if (m_legend_visible == visible) {
        return;
    }

    m_legend_visible = visible;
    rebuildGrid();
    emit legendVisibleChanged(m_legend_visible);
}

void QGpsSkyView::setMarkerLabelsVisible(bool visible)
{
    if (m_marker_labels_visible == visible) {
        return;
    }

    m_marker_labels_visible = visible;
    for (QGpsSatItem *item : std::as_const(m_sat_list)) {
        item->setLabelVisible(m_marker_labels_visible);
    }
    emit markerLabelsVisibleChanged(m_marker_labels_visible);
}

void QGpsSkyView::resetView()
{
    m_zoom = 1.0;
    setRotation(0.0);
    rebuildGrid();
    rebuildSatellites();
    emit viewReset();
}

void QGpsSkyView::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *showPrnAction = menu.addAction(QStringLiteral("Show PRN labels"));
    showPrnAction->setCheckable(true);
    showPrnAction->setChecked(m_marker_labels_visible);
    QAction *showLegendAction = menu.addAction(QStringLiteral("Show Legend"));
    showLegendAction->setCheckable(true);
    showLegendAction->setChecked(m_legend_visible);
    menu.addSeparator();
    QAction *resetViewAction = menu.addAction(QStringLiteral("Reset View"));

    QAction *selectedAction = menu.exec(event->globalPos());
    if (selectedAction == showPrnAction) {
        setMarkerLabelsVisible(showPrnAction->isChecked());
    } else if (selectedAction == showLegendAction) {
        setLegendVisible(showLegendAction->isChecked());
    } else if (selectedAction == resetViewAction) {
        resetView();
    }
}

void QGpsSkyView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_rotating = true;
        m_drag_start_angle = pointerAngle(event->pos());
        m_drag_start_rotation = m_rotation;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void QGpsSkyView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_rotating) {
        const double currentAngle = pointerAngle(event->pos());
        setRotation(m_drag_start_rotation - (currentAngle - m_drag_start_angle));
        event->accept();
        return;
    }

    QGraphicsView::mouseMoveEvent(event);
}

void QGpsSkyView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_rotating) {
        m_rotating = false;
        unsetCursor();
        event->accept();
        return;
    }

    QGraphicsView::mouseReleaseEvent(event);
}

void QGpsSkyView::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    rebuildGrid();
    rebuildSatellites();
}

void QGpsSkyView::wheelEvent(QWheelEvent *event)
{
    const int delta = event->angleDelta().y();
    if (delta == 0) {
        QGraphicsView::wheelEvent(event);
        return;
    }

    const double factor = delta > 0 ? 1.1 : 1.0 / 1.1;
    m_zoom = std::clamp(m_zoom * factor, 0.55, 2.5);
    rebuildGrid();
    rebuildSatellites();
    event->accept();
}

QPointF QGpsSkyView::pol2cart(double az, double el) const
{
    az = std::fmod(az - m_rotation, 360.0);
    if (az < 0) {
        az += 360.0;
    }

    const double radians = qDegreesToRadians(az);
    const double elevation = std::clamp(el, -10.0, 90.0);
    const double radiusScale = m_el_radius
                                   ? qSin(qDegreesToRadians(90.0 - elevation))
                                   : ((90.0 - elevation) / 90.0);

    return QPointF(m_center.x() + qSin(radians) * radiusScale * m_radius,
                   m_center.y() - qCos(radians) * radiusScale * m_radius);
}

double QGpsSkyView::pointerAngle(const QPoint &pos) const
{
    const QPointF vector = QPointF(pos) - m_center;
    double angle = qRadiansToDegrees(std::atan2(vector.x(), -vector.y()));
    if (angle < 0) {
        angle += 360.0;
    }
    return angle;
}

QString QGpsSkyView::satelliteKey(const QGpsSatItem::Data &satellite) const
{
    return QStringLiteral("%1:%2:%3:%4")
        .arg(static_cast<int>(satellite.type))
        .arg(satellite.svid)
        .arg(satellite.PRN)
        .arg(satellite.hasSigid ? satellite.sigid : -1);
}

QGpsSatItem::Data QGpsSkyView::displaySatellite(const QGpsSatItem::Data &satellite, qint64 nowMs)
{
    constexpr qint64 usedHoldMs = 3000;
    QGpsSatItem::Data display = satellite;
    const QString key = satelliteKey(satellite);

    if (satellite.used) {
        m_last_used_ms.insert(key, nowMs);
    } else {
        const qint64 lastUsedMs = m_last_used_ms.value(key, 0);
        if (lastUsedMs > 0 && nowMs - lastUsedMs <= usedHoldMs) {
            display.used = true;
        }
    }

    const int previousBand = m_signal_bands.value(key, -1);
    const int band = signalBand(satellite.ss, previousBand);
    m_signal_bands.insert(key, band);
    display.ss = representativeSignal(band, satellite.ss);

    return display;
}

int QGpsSkyView::signalBand(float ss, int previousBand) const
{
    constexpr float hysteresis = 1.5f;
    constexpr float thresholds[] = {12.0f, 30.0f, 36.0f, 42.0f};

    if (previousBand < 0) {
        if (ss < thresholds[0]) return 0;
        if (ss < thresholds[1]) return 1;
        if (ss < thresholds[2]) return 2;
        if (ss < thresholds[3]) return 3;
        return 4;
    }

    int band = previousBand;
    while (band < 4 && ss >= thresholds[band] + hysteresis) {
        ++band;
    }
    while (band > 0 && ss < thresholds[band - 1] - hysteresis) {
        --band;
    }
    return band;
}

float QGpsSkyView::representativeSignal(int band, float fallback) const
{
    switch (band) {
    case 0:
        return 0.0f;
    case 1:
        return 20.0f;
    case 2:
        return 33.0f;
    case 3:
        return 39.0f;
    case 4:
        return 45.0f;
    default:
        return fallback;
    }
}

void QGpsSkyView::rebuildGrid()
{
    for (QGraphicsItem *item : std::as_const(m_grid_items)) {
        m_scene->removeItem(item);
        delete item;
    }
    m_grid_items.clear();

    const QSize viewportSize = viewport()->size();
    const qreal side = std::max<qreal>(80.0, std::min(viewportSize.width(), viewportSize.height()));
    m_radius = std::max<qreal>(10.0, ((side - (m_padding * 2.0)) / 2.0) * m_zoom);
    m_center = QPointF(viewportSize.width() / 2.0, viewportSize.height() / 2.0);

    const QRectF sceneRect(QPointF(0, 0), QSizeF(viewportSize));
    m_scene->setSceneRect(sceneRect);

    const QPen gridPen(QColor(230, 230, 230), 1);
    const QPen axisPen(QColor(255, 255, 255), 1);
    const QPen zenithPen(QColor(255, 255, 255), 2);

    auto addGridItem = [this](QGraphicsItem *item) {
        m_grid_items.append(item);
        return item;
    };

    for (int elevation = 0; elevation <= 90; elevation += m_grid_step) {
        const double radiusScale = m_el_radius
                                       ? qSin(qDegreesToRadians(90.0 - elevation))
                                       : ((90.0 - elevation) / 90.0);
        const qreal r = std::max<qreal>(2.0, m_radius * radiusScale);
        addGridItem(m_scene->addEllipse(QRectF(m_center.x() - r,
                                               m_center.y() - r,
                                               r * 2.0,
                                               r * 2.0),
                                        elevation == 90 ? zenithPen : gridPen));
    }

    const QPointF north = pol2cart(0, 0);
    const QPointF east = pol2cart(90, 0);
    const QPointF south = pol2cart(180, 0);
    const QPointF west = pol2cart(270, 0);

    addGridItem(m_scene->addLine(QLineF(north, south), axisPen));
    addGridItem(m_scene->addLine(QLineF(east, west), axisPen));

    struct Direction {
        QString name;
        double az;
    } directions[] = {
        {QStringLiteral("N"), 0.0},
        {QStringLiteral("E"), 90.0},
        {QStringLiteral("S"), 180.0},
        {QStringLiteral("W"), 270.0},
    };

    for (const Direction &direction : directions) {
        QGraphicsSimpleTextItem *label = m_scene->addSimpleText(direction.name);
        label->setBrush(QBrush(Qt::white));
        const QPointF pos = pol2cart(direction.az, -6.0);
        const QRectF bounds = label->boundingRect();
        label->setPos(pos.x() - bounds.width() / 2.0, pos.y() - bounds.height() / 2.0);
        addGridItem(label);
    }

    if (!m_legend_visible) {
        return;
    }

    struct LegendEntry {
        QGpsSatItem::QGnnsType type;
        QString name;
    } legendEntries[] = {
        {QGpsSatItem::GPS, QStringLiteral("GPS")},
        {QGpsSatItem::SBAS, QStringLiteral("SBAS")},
        {QGpsSatItem::Galileo, QStringLiteral("Galileo")},
        {QGpsSatItem::BeiDou, QStringLiteral("BeiDou")},
        {QGpsSatItem::QZSS, QStringLiteral("QZSS")},
        {QGpsSatItem::GLONASS, QStringLiteral("GLONASS")},
        {QGpsSatItem::IRNSS, QStringLiteral("IRNSS")},
    };

    const qreal legendWidth = 118.0;
    const qreal legendRowHeight = 19.0;
    const qreal legendHeight = 12.0 + (std::size(legendEntries) * legendRowHeight);
    const QPointF legendTopLeft(sceneRect.right() - legendWidth - 10.0, sceneRect.top() + 10.0);

    QGraphicsRectItem *legendBackground = m_scene->addRect(QRectF(legendTopLeft, QSizeF(legendWidth, legendHeight)),
                                                           QPen(QColor(90, 90, 90)),
                                                           QBrush(QColor(0, 0, 0, 180)));
    legendBackground->setZValue(10);
    addGridItem(legendBackground);

    for (int i = 0; i < static_cast<int>(std::size(legendEntries)); ++i) {
        const qreal y = legendTopLeft.y() + 13.0 + (i * legendRowHeight);
        QGpsSatItem::Data data;
        data.type = legendEntries[i].type;
        data.svid = 0;
        data.PRN = 1;
        data.sigid = 0;
        data.Elev = 45.0f;
        data.az = 0.0f;
        data.ss = 45.0f;
        data.qual = 0.0f;
        data.qrRes = 0.0f;
        data.used = true;
        data.health = 1;
        data.hasSigid = false;
        data.hasQual = false;
        data.hasPrRes = false;
        data.hasHealth = true;

        QGpsSatItem *marker = new QGpsSatItem(legendEntries[i].type);
        marker->setData(data);
        marker->setLabelVisible(false);
        marker->setPos(legendTopLeft.x() + 14.0, y);
        marker->setZValue(11);
        m_scene->addItem(marker);
        addGridItem(marker);

        QGraphicsSimpleTextItem *name = m_scene->addSimpleText(legendEntries[i].name);
        name->setBrush(QBrush(Qt::white));
        name->setPos(legendTopLeft.x() + 30.0, y - 8.0);
        name->setZValue(11);
        addGridItem(name);
    }
}

void QGpsSkyView::rebuildSatellites()
{
    for (QGpsSatItem *item : std::as_const(m_sat_list)) {
        m_scene->removeItem(item);
        delete item;
    }
    m_sat_list.clear();

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (const QGpsSatItem::Data &rawSatellite : std::as_const(m_satellites)) {
        const QGpsSatItem::Data satellite = displaySatellite(rawSatellite, nowMs);
        if (satellite.PRN < 1 || satellite.PRN > 437) {
            continue;
        }
        if (satellite.az < 0 || satellite.az > 359) {
            continue;
        }
        if (satellite.Elev < -10 || satellite.Elev > 90) {
            continue;
        }
        if (qFuzzyIsNull(satellite.az) && qFuzzyIsNull(satellite.Elev)) {
            continue;
        }

        QGpsSatItem *item = new QGpsSatItem(satellite.type);
        item->setData(satellite);
        item->setLabelVisible(m_marker_labels_visible);
        item->setPos(pol2cart(satellite.az, satellite.Elev));
        m_scene->addItem(item);
        m_sat_list.append(item);
    }
}
