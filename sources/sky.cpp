
#define _USE_MATH_DEFINES
#include "sky.h"

#include "ArHosekSkyModel.h"

#include "myparams.h"

#include <iostream>
#include <QVector3D>
#include <QQuaternion>
#include <QPainter>

namespace {
inline double rad2Deg(double rad) { return 180.0 * rad / M_PI; }
}

Sky* Sky::instance() {
  static Sky _instance;
  return &_instance;
}

QVector3D Sky::posToVec(QPointF pos, QSize size) {
  auto deg2Rad  = [](double degree) -> double { return M_PI * degree / 180.0; };
  MyParams* p   = MyParams::instance();
  double camFov = deg2Rad(p->getDouble(CameraFov));
  double camElev = deg2Rad(p->getDouble(CameraElevation));
  QVector3D camVec(0, cosf(camElev), sinf(camElev));
  QVector3D camUpper(0, -sinf(camElev), cosf(camElev));
  double k = 2.0 * tan(camFov / 2.0) / (double)size.height();

  double dy = (pos.y() - (double)size.height() / 2.0) * k;
  double dx = (pos.x() - (double)size.width() / 2.0) * k;

  QQuaternion q1 = QQuaternion::fromAxisAndAngle(
      -camUpper, rad2Deg(atan(sqrt(dx * dx + dy * dy))));
  QQuaternion q2 =
      QQuaternion::fromAxisAndAngle(camVec, rad2Deg(atan2(dy, dx)));
  QQuaternion totalRotation = q2 * q1;

  return totalRotation * camVec;
}

QPointF Sky::posToAngle(QPointF pos, QSize size) {
  QVector3D vec    = posToVec(pos, size);
  double theta_e   = asin(vec.z());
  double azimuth_e = atan2(vec.x(), vec.y());
  return QPointF(azimuth_e, theta_e);
}

QPointF Sky::angleToPos(QPointF angle, QSize size) {
  bool dummy;
  return angleToPos(angle, size, dummy);
}

QPointF Sky::angleToPos(QPointF angle, QSize size, bool& inverted) {
  auto deg2Rad  = [](double degree) -> double { return M_PI * degree / 180.0; };
  MyParams* p   = MyParams::instance();
  double camFov = deg2Rad(p->getDouble(CameraFov));
  double camElev  = deg2Rad(p->getDouble(CameraElevation));
  double pAzimuth = angle.x();
  double pElev    = angle.y();

  // length on the unit sphere, corresponding to 1 pixel on the screen
  // 単位球上の投影面の1ピクセルあたりの距離
  double A = 2.0 * tan(camFov * 0.5) / (double)size.height();

  double sin_camE = sin(camElev);
  double cos_camE = cos(camElev);
  double sin_pE   = sin(pElev);
  double cos_pE   = cos(pElev);
  double sin_pA   = sin(pAzimuth);
  double cos_pA   = cos(pAzimuth);

  double t = (-sin_camE * cos_pE * cos_pA + cos_camE * sin_pE) /
             (A * (cos_camE * cos_pE * cos_pA + sin_camE * sin_pE));
  double k = (A * cos_camE * t + sin_camE) / sin_pE;
  inverted = (k < 0.0);
  double s = k * cos_pE * sin_pA / A;

  return QPointF(s + ((double)size.width() / 2.0),
                 -t + ((double)size.height() / 2.0));
}

QImage& Sky::getSkyImage() {
  auto deg2Rad = [](double degree) -> double { return M_PI * degree / 180.0; };
  MyParams* p  = MyParams::instance();
  if (p->isFlowDone(Flow_GenerateSky)) return m_skyImage;

  double camFov  = deg2Rad(p->getDouble(CameraFov));
  double camElev = deg2Rad(p->getDouble(CameraElevation));

  double sunElev     = std::max(0.0, deg2Rad(p->getDouble(SunElevation)));
  double sunAzimuth  = deg2Rad(p->getDouble(SunAzimuth));
  QColor sunRadiance = QColor(50, 50, 50);

  double airTurb   = p->getDouble(SkyTurbidity);
  QColor airAlbedo = QColor(12, 12, 12);

  int skySkip = p->getInt(SkyRenderSkip);

  QSize sceneSize = p->getSceneSize();

  m_skyImage = createSkyImage(
      sceneSize.width() / skySkip, sceneSize.height() / skySkip, airTurb,
      airAlbedo, sunElev, sunAzimuth, sunRadiance, camFov, camElev);

  p->setFlowDone(Flow_GenerateSky);
  return m_skyImage;
}

QImage& Sky::getCombinedBackground() {
  MyParams* p = MyParams::instance();

  if (p->isFlowDone(Flow_CombineBackground)) return m_combinedBackground;

  double opacity = p->getDouble(BackgroundImgOpacity);

  QSize sceneSize = p->getSceneSize();
  QRect sceneRect(QPoint(0, 0), sceneSize);

  QImage img(sceneSize, QImage::Format_ARGB32);

  QPainter painter(&img);

  if (opacity < 1.0 || getBackgroundImage().isNull())
    painter.drawImage(sceneRect, getSkyImage());

  if (opacity > 0.0 && !getBackgroundImage().isNull()) {
    QSize bgSize = Sky::instance()->getBackgroundImage().size();
    QRect bgRectOnViewer(0, 0,
                         sceneRect.height() * bgSize.width() / bgSize.height(),
                         sceneRect.height());
    bgRectOnViewer.moveCenter(sceneRect.center());

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(opacity);
    painter.drawPixmap(bgRectOnViewer, getBackgroundImage());
    painter.setOpacity(1.0);
  }

  m_combinedBackground = img;

  p->setFlowDone(Flow_CombineBackground);
  return m_combinedBackground;
}

QRectF Sky::getSunRectOnViewer(bool unique) {
  auto deg2Rad = [](double degree) -> double { return M_PI * degree / 180.0; };
  MyParams* p  = MyParams::instance();

  FlowId flowId =
      (unique) ? Flow_GetUniqueSunRectOnViewer : Flow_GetSunRectOnViewer;
  QRectF& sunRect   = (unique) ? m_uniqueSunRectOnViewer : m_sunRectOnViewer;
  ParamId sunElevId = (unique) ? SunUniqueElevation : SunElevation;
  ParamId sunAzimId = (unique) ? SunUniqueAzimuth : SunAzimuth;

  if (p->isFlowDone(flowId)) return sunRect;

  double sunElev    = std::max(0.0, deg2Rad(p->getDouble(sunElevId)));
  double sunAzimuth = deg2Rad(p->getDouble(sunAzimId));
  double camFov     = deg2Rad(p->getDouble(CameraFov));
  QSize sceneSize   = p->getSceneSize();

  bool inverted;
  QPointF sunPos =
      angleToPos(QPointF(sunAzimuth, sunElev), sceneSize, inverted);

  if (inverted)
    sunRect = QRectF();
  else {
    //太陽の見かけの半径は0.265度...だが、0.5度くらいにする
    float sunRadius = (double)sceneSize.height() * deg2Rad(0.5) / camFov;
    sunRect         = QRectF(sunPos - QPointF(sunRadius, sunRadius),
                     QSizeF(sunRadius * 2.0, sunRadius * 2.0));
  }

  p->setFlowDone(flowId);
  return sunRect;
}

float Sky::getEyeLevelPosOnViewer() {
  auto deg2Rad = [](double degree) -> double { return M_PI * degree / 180.0; };
  MyParams* p  = MyParams::instance();
  if (p->isFlowDone(Flow_GetEyeLevelPosOnViewer)) return m_eyeLevelPosOnViewer;

  double camFov   = deg2Rad(p->getDouble(CameraFov));
  double camElev  = deg2Rad(p->getDouble(CameraElevation));
  QSize sceneSize = p->getSceneSize();

  // eye level position (length from the top of the viewer)
  // 地平線のピクセル位置（ビューア上から）
  m_eyeLevelPosOnViewer = (float)sceneSize.height() *
                          (tan(camFov / 2.0f) + tan(camElev)) /
                          (2 * tan(camFov / 2.0f));

  p->setFlowDone(Flow_GetEyeLevelPosOnViewer);
  return m_eyeLevelPosOnViewer;
}

float Sky::getEyeElevationWithFixedEyeLevel() {
  auto deg2Rad  = [](double degree) -> double { return M_PI * degree / 180.0; };
  auto rad2Deg  = [](double radian) -> double { return 180.0 * radian / M_PI; };
  MyParams* p   = MyParams::instance();
  double camFov = deg2Rad(p->getDouble(CameraFov));
  QSize sceneSize = p->getSceneSize();

  return rad2Deg(std::atan(2.0 * tan(camFov / 2.0f) * m_eyeLevelPosOnViewer /
                               (float)sceneSize.height() -
                           tan(camFov / 2.0f)));
}

QImage Sky::createSkyImage(
    int width, int height,    // ピクセルサイズ
    float turbidity_in,       // 空気の濁り具合
    QColor albedo_in,         // 地表の色
    float solarElevation_in,  // 太陽の高さ（時間帯）,
    float solarAzimuth_in,    //太陽の方向（カメラの向きとの変位）
    QColor radiance_in,       // 入射光
    float camFov_in,          //カメラ画角（縦方向、radian）
    float camElevation_in)    // カメラ仰角（radian）
{
  /////////////////
  // alloc pixels
  /////////////////
  QImage img(width, height, QImage::Format_ARGB32);
  img.fill(Qt::black);

  /////////////////////////////
  // init Ar Hosek RGB  Model
  /////////////////////////////
  const int num_channels = 3;
  ArHosekSkyModelState* skymodel_state[num_channels];
  double albedo[num_channels];
  albedo_in.getRgbF(&albedo[0], &albedo[1], &albedo[2]);

  for (unsigned int i = 0; i < num_channels; i++) {
    skymodel_state[i] = arhosek_rgb_skymodelstate_alloc_init(
        turbidity_in, albedo[i], solarElevation_in);
  }

  //////////////////
  // UV map result
  //////////////////

  QVector3D camVec(0, cosf(camElevation_in), sinf(camElevation_in));
  QVector3D camUpper(0, -sinf(camElevation_in), cosf(camElevation_in));
  double k = 2.0 * tan(camFov_in / 2.0) / (double)height;

  double theta_s   = M_PI / 2.0 - solarElevation_in;
  double azimuth_s = solarAzimuth_in;  // sun azimuth 太陽の方位角

  for (int y = 0; y < height; y++) {
    double dy = ((double)y - (double)height / 2.0) * k;

    for (int x = 0; x < width; x++) {
      double dx = ((double)x - (double)width / 2.0) * k;

      QQuaternion q1 = QQuaternion::fromAxisAndAngle(
          -camUpper, rad2Deg(atan(sqrt(dx * dx + dy * dy))));
      QQuaternion q2 =
          QQuaternion::fromAxisAndAngle(camVec, rad2Deg(atan2(dy, dx)));
      QQuaternion totalRotation = q2 * q1;

      QVector3D vec = totalRotation * camVec;

      double theta_e = acos(vec.z());
      if (theta_e > M_PI / 2.0) {
        img.setPixelColor(x, y, QColor(64, 64, 64));
        continue;
      }

      double azimuth_e = atan2(vec.x(), vec.y());

      QVector3D pe(sin(theta_e) * cos(azimuth_e), sin(theta_e) * sin(azimuth_e),
                   cos(theta_e));
      QVector3D ps(sin(theta_s) * cos(azimuth_s), sin(theta_s) * sin(azimuth_s),
                   cos(theta_s));
      double gamma = acos(std::min<double>(
          1.0, std::max<double>(-1.0, QVector3D::dotProduct(pe, ps))));
      double skydome_result[num_channels];
      for (unsigned int i = 0; i < num_channels; i++) {
        skydome_result[i] = arhosek_tristim_skymodel_radiance(
            skymodel_state[i], theta_e, gamma, i);
      }

      QColor c(std::min(255, (int)((double)radiance_in.red() *
                                   pow(skydome_result[0], 1.0 / 2.2))),
               std::min(255, (int)((double)radiance_in.green() *
                                   pow(skydome_result[1], 1.0 / 2.2))),
               std::min(255, (int)((double)radiance_in.blue() *
                                   pow(skydome_result[2], 1.0 / 2.2))));
      img.setPixelColor(x, y, c);
    }
  }

  // clean up
  for (unsigned int i = 0; i < num_channels; i++) {
    arhosekskymodelstate_free(skymodel_state[i]);
  }

  return img;
}

void Sky::setEyeLevelPosOnViewer(float eyeLevel) {
  auto deg2Rad = [](double degree) -> double { return M_PI * degree / 180.0; };
  auto rad2Deg = [](double radian) -> double { return 180.0 * radian / M_PI; };

  MyParams* p = MyParams::instance();

  double camFov   = deg2Rad(p->getDouble(CameraFov));
  QSize sceneSize = p->getSceneSize();

  double newCamElevRad = atan((2.0 * eyeLevel - (float)sceneSize.height()) *
                              tan(camFov / 2.0) / (float)sceneSize.height());
  p->getParam(CameraElevation).value = rad2Deg(newCamElevRad);
  m_eyeLevelPosOnViewer              = eyeLevel;

  p->notifyParamChanged(CameraElevation, false);
}

void Sky::moveSunPosOnViewer(QPointF offset, bool unique) {
  auto rad2Deg = [](double radian) -> double { return 180.0 * radian / M_PI; };
  MyParams* p  = MyParams::instance();
  QSize sceneSize = p->getSceneSize();

  QRectF& sunRect     = (unique) ? m_uniqueSunRectOnViewer : m_sunRectOnViewer;
  ParamId azimuthId   = (unique) ? SunUniqueAzimuth : SunAzimuth;
  ParamId elevationId = (unique) ? SunUniqueElevation : SunElevation;

  QPointF newSunPos = sunRect.center() + offset;
  if (newSunPos.y() > m_eyeLevelPosOnViewer - 0.1)
    newSunPos.setY(m_eyeLevelPosOnViewer - 0.1);
  QPointF sunAngles = posToAngle(newSunPos, sceneSize);

  p->getParam(azimuthId).value   = rad2Deg(sunAngles.x());
  p->getParam(elevationId).value = rad2Deg(sunAngles.y());
  p->notifyParamChanged(azimuthId, false);
  p->notifyParamChanged(elevationId, false);

  sunRect.moveCenter(newSunPos);
}

QPixmap& Sky::getLayoutImage() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_GetLayoutImage)) return m_layoutPixmap;

  QString path = p->getString(LayoutImgPath);

  m_layoutPixmap = QPixmap(path);  // pixmap can be null

  p->setFlowDone(Flow_GetLayoutImage);
  return m_layoutPixmap;
}

QPixmap& Sky::getBackgroundImage() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_GetBackgroundImage)) return m_backgroundPixmap;

  QString path = p->getString(BackgroundImgPath);

  m_backgroundPixmap = QPixmap(path);  // pixmap can be null

  p->setFlowDone(Flow_GetBackgroundImage);
  return m_backgroundPixmap;
}