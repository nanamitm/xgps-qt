#ifndef QGPSSATITEM_H
#define QGPSSATITEM_H

#include <QGraphicsPolygonItem>
#include <QGraphicsTextItem>
#include <QWidget>
#include <QPainter>
#include <QPainterPath>
#include <QPolygon>
#include <QColor>

class QGpsSatItem : public QGraphicsPolygonItem
{
public:
    enum { Type = UserType + 20 };
    typedef enum { GPS, SBAS, Galileo, BeiDou, IMES, QZSS, GLONASS, IRNSS } QGnnsType;

    QGpsSatItem( QGnnsType gnns_type=GPS, QGraphicsItem *parent = nullptr);

    int type() const override { return Type; }
    QPainterPath shape() const override { return m_path;}

    class Data
    {
    public:
        QGnnsType type;
        int svid;
        int PRN;
        int sigid;
        float Elev;
        float az;
        float ss;
        float qual;
        float qrRes;
        bool used;
        int health;
        bool hasSigid;
        bool hasQual;
        bool hasPrRes;
        bool hasHealth;
    } sat;

    void setData(const Data &data);
    void setLabelVisible(bool visible);

protected:
    void paint(QPainter *painter, const QStyleOptionGraphicsItem *option,
               QWidget *widget = nullptr) override;

    void set_gnns_type(QGnnsType gnns_type);

    QColor get_signal_color(int ss);

private:
    QString gnssName() const;
    QString healthText() const;

    QPainterPath m_path;
    QGraphicsTextItem *m_text;
    QColor m_color;
};

#endif // QGPSSATITEM_H
