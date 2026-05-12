#include "qgpssatitem.h"

#include <QFont>
#include <QString>

QGpsSatItem::QGpsSatItem( QGnnsType gnns_type,QGraphicsItem *parent)
   : QGraphicsPolygonItem(parent)
   , m_color(Qt::red)
{
    set_gnns_type(gnns_type);

    setFlag(QGraphicsItem::ItemIsSelectable, true);

    m_text = new QGraphicsTextItem(this);
    m_text->setDefaultTextColor(Qt::white);
    QFont labelFont = m_text->font();
    labelFont.setPointSize(7);
    m_text->setFont(labelFont);
    m_text->setPos(6, -11);

    sat.type = gnns_type;
    sat.svid = 0;
    sat.PRN = 0;
    sat.sigid = 0;
    sat.Elev = 0;
    sat.az = 0;
    sat.ss = 0;
    sat.qual = 0;
    sat.qrRes = 0;
    sat.used = false;
    sat.health = 0;
    sat.hasSigid = false;
    sat.hasQual = false;
    sat.hasPrRes = false;
    sat.hasHealth = false;

    setData(sat);
}

void QGpsSatItem::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_UNUSED(widget);

    painter->setPen(QPen(Qt::blue, 2
                         , isSelected() ? Qt::DotLine : Qt::SolidLine
                         , Qt::RoundCap, Qt::RoundJoin));
    painter->setBrush(m_color);

    if (!sat.used) {
        painter->setBrush(Qt::NoBrush);
    }

    painter->drawPolygon(polygon());

    Q_UNUSED(option);
}

void QGpsSatItem::setData(const Data &data)
{
    sat = data;
    set_gnns_type(sat.type);
    m_color = get_signal_color(static_cast<int>(sat.ss));
    m_text->setPlainText(sat.PRN > 0 ? QString::number(sat.PRN) : QStringLiteral("--"));
    m_text->setDefaultTextColor(Qt::white);

    setToolTip(QStringLiteral(
                   "<b>%1</b><br/>"
                   "SVID: %2<br/>"
                   "PRN: %3<br/>"
                   "Elevation: %4<br/>"
                   "Azimuth: %5<br/>"
                   "SNR: %6<br/>"
                   "sigId: %7<br/>"
                   "Quality: %8<br/>"
                   "prRes: %9<br/>"
                   "Used: %10<br/>"
                   "Health: %11")
                   .arg(gnssName())
                   .arg(sat.svid)
                   .arg(sat.PRN)
                   .arg(sat.Elev, 0, 'f', 1)
                   .arg(sat.az, 0, 'f', 1)
                   .arg(sat.ss, 0, 'f', 1)
                   .arg(sat.hasSigid ? QString::number(sat.sigid) : QStringLiteral("--"))
                   .arg(sat.hasQual ? QString::number(sat.qual, 'f', 0) : QStringLiteral("--"))
                   .arg(sat.hasPrRes ? QString::number(sat.qrRes, 'f', 1) : QStringLiteral("--"))
                   .arg(sat.used ? QStringLiteral("Yes") : QStringLiteral("No"))
                   .arg(healthText()));

    update();
}

void QGpsSatItem::setLabelVisible(bool visible)
{
    m_text->setVisible(visible);
}


void QGpsSatItem::set_gnns_type(QGnnsType gnns_type)
{
    constexpr qreal size = 7.0;

    m_path = QPainterPath();
    QPolygonF p;

    switch( gnns_type)
    {
    case GPS:
        m_path.addEllipse(QRectF(-size, -size, size * 2.0, size * 2.0));
        setPolygon( m_path.toFillPolygon());
        break;
    case SBAS:
        m_path.addRect(QRectF(-size, -size, size * 2.0, size * 2.0));
        setPolygon( m_path.toFillPolygon());
        break;
    case Galileo:
        p << QPointF(-size, -size) << QPointF(size, -size)
          << QPointF(0, size) << QPointF(-size, -size);
        m_path.addPolygon(p);
        setPolygon(p);
        break;
    case BeiDou:
        p << QPointF(0, -size) << QPointF(size, size)
          << QPointF(-size, size) << QPointF(0, -size);
        m_path.addPolygon(p);
        setPolygon(p);
        break;
    case IMES:
        p << QPointF(-size, 0) << QPointF(size, -size)
          << QPointF(size, size) << QPointF(-size, 0);
        m_path.addPolygon(p);
        setPolygon(p);
        break;
    case QZSS:
        p << QPointF(-size, -size) << QPointF(size, 0)
          << QPointF(-size, size) << QPointF(-size, -size);
        m_path.addPolygon(p);
        setPolygon(p);
        break;
    case GLONASS:
        p << QPointF(0, -size) << QPointF(size, 0)
          << QPointF(0, size) << QPointF(-size, 0) << QPointF(0, -size);
        m_path.addPolygon(p);
        setPolygon(p);
        break;
    case IRNSS:
        p << QPointF(-size, 0) << QPointF(size, -size)
          << QPointF(size, size) << QPointF(-size, 0);
        m_path.addPolygon(p);
        setPolygon(p);
        break;
    default:
        break;
    }
}


QColor QGpsSatItem::get_signal_color(int ss)
{
    if( 12 > ss) return QColor(190, 190, 190);  // gray
    if( 30 > ss ) return QColor(255, 0, 0);     // red
    if( 36 > ss ) return QColor(255, 255, 0);   // yellow
    if( 42 > ss ) return QColor(0, 205, 0);     // green

    return QColor(0, 255, 180); // green and some blue
};

QString QGpsSatItem::gnssName() const
{
    switch (sat.type) {
    case GPS:
        return QStringLiteral("GPS");
    case SBAS:
        return QStringLiteral("SBAS");
    case Galileo:
        return QStringLiteral("Galileo");
    case BeiDou:
        return QStringLiteral("BeiDou");
    case IMES:
        return QStringLiteral("IMES");
    case QZSS:
        return QStringLiteral("QZSS");
    case GLONASS:
        return QStringLiteral("GLONASS");
    case IRNSS:
        return QStringLiteral("IRNSS");
    }

    return QStringLiteral("GNSS");
}

QString QGpsSatItem::healthText() const
{
    if (!sat.hasHealth) {
        return QStringLiteral("--");
    }

    switch (sat.health) {
    case 1:
        return QStringLiteral("OK");
    case 2:
        return QStringLiteral("Bad");
    default:
        return QStringLiteral("Unknown");
    }
}




