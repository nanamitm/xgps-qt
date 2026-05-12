#ifndef QGPSSKYVIEW_H
#define QGPSSKYVIEW_H

#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QHash>
#include <QList>

#include "qgpssatitem.h"

class QGpsSkyView : public QGraphicsView
{
    Q_OBJECT

public:
    QGpsSkyView(QWidget *parent = nullptr);

    void setSatellites(const QList<QGpsSatItem::Data> &satellites);
    void setElRadiusMode(bool el_mode);
    void setRotation(double rotation);
    void setGridStep(int degrees);
    void setLegendVisible(bool visible);
    void setMarkerLabelsVisible(bool visible);
    void resetView();

    bool isLegendVisible() const { return m_legend_visible; }
    bool markerLabelsVisible() const { return m_marker_labels_visible; }
    double rotation() const { return m_rotation; }
    double zoom() const { return m_zoom; }

signals:
    void rotationChanged(double rotation);
    void legendVisibleChanged(bool visible);
    void markerLabelsVisibleChanged(bool visible);
    void viewReset();

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;

    QPointF pol2cart(double az, double el) const;

private:
    void rebuildGrid();
    void rebuildSatellites();
    double pointerAngle(const QPoint &pos) const;
    QString satelliteKey(const QGpsSatItem::Data &satellite) const;
    QGpsSatItem::Data displaySatellite(const QGpsSatItem::Data &satellite, qint64 nowMs);
    int signalBand(float ss, int previousBand) const;
    float representativeSignal(int band, float fallback) const;

    qreal m_padding;
    qreal m_radius;
    double m_zoom = 1.0;
    QPointF m_center;

    bool m_el_radius = false;
    bool m_legend_visible = true;
    bool m_marker_labels_visible = true;
    bool m_rotating = false;
    int m_grid_step = 30;
    double m_rotation = 0.0;
    double m_drag_start_angle = 0.0;
    double m_drag_start_rotation = 0.0;

    QGraphicsScene *m_scene;

    QGraphicsSimpleTextItem m_direction[4];
    QList<QGraphicsItem *> m_grid_items;

    QList<QGpsSatItem::Data> m_satellites;
    QList<QGpsSatItem *> m_sat_list;
    QHash<QString, qint64> m_last_used_ms;
    QHash<QString, int> m_signal_bands;
};

#endif // QGPSSKYVIEW_H
