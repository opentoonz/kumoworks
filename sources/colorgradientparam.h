#pragma once
#ifndef COLORGRADIENTPARAM_H
#define COLORGRADIENTPARAM_H

#include <QLinearGradient>
#include <QVector3D>

class ColorGradient : public QLinearGradient {
public:
  ColorGradient() : QLinearGradient() {}
  
  QColor getColorAt(qreal pos) {
    if (pos <= stops().first().first) return stops().first().second;
    else if (pos >= stops().last().first) return stops().last().second;

    for (int k = 1; k < stops().count(); k++) {
      if (stops().at(k).first < pos) continue;
      qreal segmentLength = stops().at(k).first - stops().at(k - 1).first;
      qreal pdist = pos - stops().at(k - 1).first;
      qreal ratio = pdist / segmentLength;
      QColor start = stops().at(k - 1).second;
      QColor end = stops().at(k).second;
      qreal red = (1.0 - ratio)*start.redF() + ratio*end.redF();
      qreal green = (1.0 - ratio)*start.greenF() + ratio*end.greenF();
      qreal blue = (1.0 - ratio)*start.blueF() + ratio*end.blueF();
      return QColor::fromRgbF(red, green, blue);
    }
    return QColor();
  }

  QVector3D getColorVecAt(qreal pos) {
    QColor col = getColorAt(pos);
    return QVector3D(col.redF(), col.greenF(), col.blueF());
  }

  

  int getKeyIndexAt(qreal pos, int notFoundValue = -1) {
    for (int k = 0; k < stops().count(); k++) {
      if (stops().at(k).first == pos) return k;
    }
    return notFoundValue;
  }

  // save and load
  friend QDataStream & operator << (QDataStream &arch, const ColorGradient & object)
  {
    QGradientStops stops = object.stops();
    arch << stops.count();
    for (QGradientStop & stop : stops) {
      arch << stop.first;
      arch << stop.second;
    }
    return arch;
  }

  friend QDataStream & operator >> (QDataStream &arch, ColorGradient & object)
  {
    int stopCount;
    arch >> stopCount;
    QGradientStops stops;
    for (int i = 0; i < stopCount; i++) {
      QGradientStop stop;
      arch >> stop.first;
      arch >> stop.second;
      stops.append(stop);
    }
    object.setStops(stops);
    return arch;
  }
};

Q_DECLARE_METATYPE(ColorGradient)

#endif