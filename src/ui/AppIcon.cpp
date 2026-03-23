#include "ui/AppIcon.hpp"

#include <array>

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPixmap>
#include <QRectF>

namespace cliphist {

QIcon CreateCliphistIcon() {
  QIcon icon;
  const std::array<int, 8> sizes = {16, 20, 24, 32, 40, 48, 64, 128};

  for (const int size : sizes) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const qreal s = static_cast<qreal>(size);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor("#1d4ed8"));
    painter.drawRoundedRect(QRectF(1.0, 1.0, s - 2.0, s - 2.0), s * 0.22, s * 0.22);

    painter.setBrush(QColor("#ffffff"));
    painter.drawRoundedRect(QRectF(s * 0.24, s * 0.20, s * 0.52, s * 0.62),
                            s * 0.08, s * 0.08);

    painter.setBrush(QColor("#93c5fd"));
    painter.drawRoundedRect(QRectF(s * 0.36, s * 0.11, s * 0.28, s * 0.14),
                            s * 0.05, s * 0.05);

    QPen line_pen(QColor("#1d4ed8"));
    line_pen.setCapStyle(Qt::RoundCap);
    line_pen.setWidthF((s < 28.0) ? 1.6 : s * 0.065);
    painter.setPen(line_pen);
    painter.drawLine(QPointF(s * 0.34, s * 0.39), QPointF(s * 0.66, s * 0.39));
    painter.drawLine(QPointF(s * 0.34, s * 0.52), QPointF(s * 0.66, s * 0.52));
    painter.drawLine(QPointF(s * 0.34, s * 0.65), QPointF(s * 0.55, s * 0.65));

    icon.addPixmap(pixmap);
  }

  return icon;
}

}  // namespace cliphist
