#pragma once

#ifndef SKY_H
#define SKY_H

#include <QColor>
#include <QImage>
#include <QPixmap>

class Sky {  // singleton

  QImage m_skyImage;
  QRectF m_sunRectOnViewer, m_uniqueSunRectOnViewer;
  float m_eyeLevelPosOnViewer;

  QPixmap m_layoutPixmap;
  QPixmap m_backgroundPixmap;
  QImage m_combinedBackground;

  Sky() {}

public:
  static Sky* instance();

  static QVector3D posToVec(QPointF pos, QSize size);
  static QPointF posToAngle(QPointF pos, QSize size);
  static QPointF angleToPos(QPointF angle, QSize size);
  static QPointF angleToPos(QPointF angle, QSize size, bool& inverted);

  QImage& getSkyImage();

  QPixmap& getBackgroundImage();
  QImage& getCombinedBackground();

  QRectF getSunRectOnViewer(bool unique = false);
  float getEyeLevelPosOnViewer();
  float getEyeElevationWithFixedEyeLevel();

  QImage createSkyImage(
      int width, int height, float turbidity_in, QColor albedo_in,
      float solarElevation_in,
      float solarAzimuth_in,  // sun direction relative to the camera
      QColor radiance_in,
      float camFov_in,         // in radian
      float camElevation_in);  // in radian

  void setEyeLevelPosOnViewer(float eyeLevel);
  void moveSunPosOnViewer(QPointF offset, bool unique = false);

  QPixmap& getLayoutImage();
};

#endif
