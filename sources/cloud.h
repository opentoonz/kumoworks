#pragma once
#define _USE_MATH_DEFINES

#include <QPixmap>
#include <QList>
#include <QVector3D>
#include <QRunnable>
#include "volume.h"

#include "myparams.h"

class QSettings;

// Used in ArrangeMetaball
struct MetaBall {
  int radius;
  int filterSize;
  Volume volume;
};

class ArrangeMetaballTask : public QRunnable {
  int m_from, m_to;
  QRect m_bbox;
  Volume* m_possibleArea;
  Volume* m_resultVolume;
  const QList<MetaBall> m_metaballs;
  const Real m_ballSize;
  const int m_randomSeed;
  const QPoint m_posOffset;

  // Do linear interpolation the volume when in rough quality mode
  // in order to reproduce the ball arrangement as much as possible.
  // 低画質モードのときは濃度値を線形補間する
  const bool m_doLerp;

  const Real m_oblateness;  // 扁平度
  const Real m_elevation;

  void run();

public:
  ArrangeMetaballTask(int from, int to, QRect bbox, Volume& possibleAreea,
                      Volume& resultVolume, Real ballSize,
                      QList<MetaBall>& metaballs, int randomSeed,
                      QPoint posOffset, bool doLerp, Real oblateness,
                      Real elevation)
      : m_from(from)
      , m_to(to)
      , m_bbox(bbox)
      , m_possibleArea(&possibleAreea)
      , m_resultVolume(&resultVolume)
      , m_ballSize(ballSize)
      , m_metaballs(metaballs)
      , m_randomSeed(randomSeed)
      , m_posOffset(posOffset)
      , m_doLerp(doLerp)
      , m_oblateness(oblateness)
      , m_elevation(elevation) {}
};

class SimplexNoiseTask : public QRunnable {
  int m_fromZ, m_toZ;
  Volume* m_noise;
  Real m_noiseSize;
  Real m_intensity;
  const QVector3D m_oblateNoiseScale;
  void run();

public:
  SimplexNoiseTask(int from, int to, Volume& noise, Real noiseSize,
                   Real intensity, QVector3D oblateNoiseScale)
      : m_fromZ(from)
      , m_toZ(to)
      , m_noise(&noise)
      , m_noiseSize(noiseSize)
      , m_intensity(intensity)
      , m_oblateNoiseScale(oblateNoiseScale) {}
};

class WorleyNoiseTask_MakeFilter : public QRunnable {
  int m_fromZ, m_toZ;
  Volume* m_filter;
  const QVector3D m_oblateFilterRadius;
  void run();

public:
  WorleyNoiseTask_MakeFilter(int from, int to, Volume& filter,
                             QVector3D oblateFilterRadius)
      : m_fromZ(from)
      , m_toZ(to)
      , m_filter(&filter)
      , m_oblateFilterRadius(oblateFilterRadius) {}
};

class WorleyNoiseTask : public QRunnable {
  int m_fromZ, m_toZ;
  Volume* m_worleyNoise;
  Volume* m_filter;
  int* m_noisePivotCenter;
  const QVector3D m_oblateFilterRadius;

  void run();

public:
  WorleyNoiseTask(int from, int to, Volume& worleyNoise, Volume& filter,
                  int* noisePivotCenter, QVector3D oblateFilterRadius)
      : m_fromZ(from)
      , m_toZ(to)
      , m_worleyNoise(&worleyNoise)
      , m_filter(&filter)
      , m_noisePivotCenter(noisePivotCenter)
      , m_oblateFilterRadius(oblateFilterRadius) {}
};

class RayMarchingRenderTask : public QRunnable {
protected:
  int m_from, m_to;
  Volume* m_cloudVol;
  QList<Real> *m_luminances, *m_opacities;
  QRect m_bbox;
  float* m_threadMaxLuminance;

  virtual void run();

public:
  RayMarchingRenderTask(int from, int to, Volume& cloudVol, QRect bbox,
                        QList<Real>& luminances, QList<Real>& opacities,
                        float* threadMaxLuminance)
      : m_from(from)
      , m_to(to)
      , m_cloudVol(&cloudVol)
      , m_bbox(bbox)
      , m_luminances(&luminances)
      , m_opacities(&opacities)
      , m_threadMaxLuminance(threadMaxLuminance) {}
};

class RenderReflectionTask : public RayMarchingRenderTask {
  void run() override;

public:
  RenderReflectionTask(int from, int to, Volume& cloudVol, QRect bbox,
                       QList<Real>& luminances, QList<Real>& opacities,
                       float* threadMaxLuminance)
      : RayMarchingRenderTask(from, to, cloudVol, bbox, luminances, opacities,
                              threadMaxLuminance) {}
};

class Cloud {
  QString m_name;

  QMap<ParamId, QVariant> m_params;

  QPixmap m_canvasPm;

  QPointF m_topLeft, m_bottomRight;

  float m_maxDepth;

  Volume m_samplingDensity;

  Volume m_metaballVol;

  Volume m_worleyNoise;

  Volume m_simplexNoise;
  Volume m_compNoiseVol;

  Volume m_cloudVol;

  QList<Real> m_luminances;
  QList<Real> m_opacities;

  QList<Real> m_refLuminances;

  QImage m_cloudImage;

  // the boundary of currently-finished m_cloudImage
  // 現在計算が完了しているm_cloudImageのbbox
  QRect m_cloudImageBBox;

  // position offset from the original position.
  // It is needed to retain ball arrangement after dragging the cloud.
  // オリジナル位置からの移動量。メタボール配置を変えないために必要
  QPoint m_posOffset;

  bool m_render = false;

public:
  Cloud(QString name = "");
  Cloud(Cloud*);

  QPixmap& getCanvasPm() { return m_canvasPm; }

  void onResize(QSize&);
  // Updating the bounding box. バウンディングボックスの更新
  void addPos(const QPointF& pos, float radius);
  QRect getBBox();

  // Used in DrawUndo. DrawUndoで使う
  QRectF getDrawingBBox() { return QRectF(m_topLeft, m_bottomRight); }
  void setDrawingBBox(const QRectF rect);

  void onStartMove();
  void move(QPoint d);
  void onEndMove(QPoint total_d);

  void autoFill();
  void removeErasedStroke(QRectF eraseStrokeRect);

  bool generateWorleyNoise();

  bool generateSimplexNoise();

  bool compositeNoises();

  bool silhouette2Voxel();

  bool arrangeMetaballs();

  bool compositeMetaballAndNoise();

  bool rayMarchingRender();

  bool renderReflection();

  QImage& getCloudImage();

  void triggerRender() { m_render = true; }
  void suspendRender() { m_render = false; }
  bool isSuspended() { return m_render == false; }

  bool isCloudDone();
  bool isCloudProcessing();

  QRect getCloudImageBBox() { return m_cloudImageBBox; }
  // Used in MoveCloudUndo. MoveCloudUndoで用いる
  void setCloudImageBBox(QRect bbox) { m_cloudImageBBox = bbox; }

  void setParam(ParamId pId, QVariant val) { m_params[pId] = val; }
  QVariant getParam(ParamId pId) { return m_params[pId]; }

  void saveData(QSettings& settings, bool forPreset = false);
  void loadData(QSettings& settings);

  void setName(QString name) { m_name = name; }
  QString getName() { return m_name; }

  void releaseBuffers();

  void addPosOffset(QPoint d) { m_posOffset += d; }

  bool isVisible() { return getParam(LayerVisibility).toBool(); }
};