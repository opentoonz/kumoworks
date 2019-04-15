#pragma once

#ifndef MYPARAMS_H
#define MYPARAMS_H

#define _USE_MATH_DEFINES
#include <cmath>

#include <QColor>
#include <QObject>
#include <QVariant>
#include <QMap>
#include <QSize>
#include <QApplication>

#include "colorgradientparam.h"
#include "myflowmanager.h"

class Cloud;
class QMainWindow;

enum ParamCategory {
  Background = 0,
  Camera,
  Sun,
  SkyRender,
  Shape1,
  Shape2,
  Noise,
  Render,
  // parameters which are not included in the Control.
  // Controlに入れないパラメータ
  Others,
  CategoryCount
};

enum ParamId {
  LayoutImgPath = 0,
  LayoutImgOpacity,
  BackgroundImgPath,
  BackgroundImgOpacity,
  CameraFov,
  CameraFixEyeLevel,
  CameraElevation,

  SunUseUniqueAngles,
  SunUniqueElevation,
  SunUniqueAzimuth,
  SunElevation,
  SunAzimuth,

  SkyTurbidity,
  SkyRenderSkip,
  SkyRenderGrid,

  ShapeDepth,
  ShapeDepthAuto,
  ShapeFollowingAccuracy,
  ShapeRandomSeed,
  ShapeBottomCutOff,
  ShapeBottomCutOffBlur,

  ShapeRadiusBase,
  ShapeRadiusRandom,
  ShapeOblateness,
  ShapeRadiusRatio,
  ShapeIntensityRatio,
  ShapeBBoxMargin,

  WorleyNoiseSizeRatio,
  WorleyNoiseRate,
  WorleyNoiseRandomSeed,
  SimplexNoiseSizeBaseRatio,
  SimplexNoiseRate,
  NoisePrevalence,

  LayerVisibility,

  RenderRayStep,
  RenderCloudDensity,
  RenderAerialRatio,
  RenderToneMap,
  RenderReflectionColor,
  RenderReflectionAlpha,

  ToolbarVoxelSkip,
  ParamCount
};

const ParamId InvalidParam = ParamCount;

Q_DECLARE_METATYPE(ParamId)

//---- Render Options ----
enum RO_Target { CurrentCloudOnly = 0, EachCloud, CombineAllCloud };

enum RO_Region { WholeSceneSize = 0, CloudBoundingBox };

enum RO_Background { RenderCloudOnly = 0, CompositeSky, RenderSkySeparately };

struct RenderOptions {
  RO_Target target         = EachCloud;
  RO_Region region         = WholeSceneSize;
  RO_Background background = RenderCloudOnly;
  int voxelSkip            = 2;
  QString dirPath;
  QString fileName;
  bool renderHiddenLayers = false;
};

struct Param {
  QString idString;  // id used for saving / loading. ファイルに保存するときのID
  QString name;      // string to be displayed on UI. UIに表示する文字列
  ParamCategory category;
  QMetaType::Type type;  // QVariant::Type is obsolete
  QList<FlowId> flowIds;
  QVariant value;
  QVariant defaultValue;
  QVariant min = 0;
  QVariant max = -1;

  // set to true if the value should be shrinked according to m_voxelSkip value.
  // に合わせて縮小されるべき値ならtrue
  bool isScalable = false;

  int decimal = 2;

  Param(QString _idString, QString _name, ParamCategory _category,
        QMetaType::Type _type, FlowId _flowId, QVariant _value,
        QVariant _min = 0, QVariant _max = -1, bool _isScalable = false,
        int _decimal = 2)
      : idString(_idString)
      , name(_name)
      , category(_category)
      , type(_type)
      , value(_value)
      , defaultValue(_value)
      , min(_min)
      , max(_max)
      , isScalable(_isScalable)
      , decimal(_decimal) {
    if (_flowId != Flow_None) flowIds.append(_flowId);
  }

  Param(QString _idString, QString _name, ParamCategory _category,
        QMetaType::Type _type, FlowId _flowId, const ColorGradient& _value)
      : idString(_idString), name(_name), category(_category), type(_type) {
    value        = QVariant::fromValue(_value);
    defaultValue = value;
    if (_flowId != Flow_None) flowIds.append(_flowId);
  }

  Param() {}

  void addFlow(FlowId fid) { flowIds.append(fid); }
};

enum Mode { DrawMode = 0, PreviewMode };

class MyParams : public QObject {  // singleton
  Q_OBJECT

  QMap<ParamId, Param> m_params;

  QList<Cloud*> m_clouds;
  int m_currentCloudIndex = 0;

  QSize m_sceneSize;

  FlowManager m_flowManager;

  Mode m_mode = DrawMode;

  QMainWindow* m_mainWindow = nullptr;

  QString m_sceneFilePath = QString();

  RenderOptions m_renderOptions;

  // Total amount of cloud layers created in this scene. 通算雲レイヤー数
  int m_totalCloudCount = 1;

  bool m_autoPreview = false;

  MyParams();

public:
  static MyParams* instance();

  Param& getParam(ParamId id) { return m_params[id]; }
  void notifyParamChanged(ParamId paramId, bool isDragging = false);

  void addCloud(Cloud* c);
  void removeCloud();
  Cloud* getCurrentCloud() { return getCloud(m_currentCloudIndex); }
  int getCloudCount() { return m_clouds.size(); }
  Cloud* getCloud(int index) {
    return (m_clouds.isEmpty() || index < 0 || index >= m_clouds.size())
               ? nullptr
               : m_clouds[index];
  }
  QList<Cloud*>& getClouds() { return m_clouds; }
  void setClouds(QList<Cloud*> clouds) { m_clouds = clouds; }

  void setSceneSize(QSize size);
  QSize getSceneSize() { return m_sceneSize; }

  void setMainWindow(QMainWindow* mw) { m_mainWindow = mw; }

  ~MyParams();

  double getDouble(ParamId id) {
    if (m_params[id].type != QMetaType::Double) return 0.0;

    if (m_params[id].isScalable)
      return m_params[id].value.toDouble() / (double)voxelSkip();
    else
      return m_params[id].value.toDouble();
  }

  int getInt(ParamId id) {
    if (m_params[id].type != QMetaType::Int) return 0;
    if (m_params[id].isScalable)
      return (int)std::round((double)m_params[id].value.toInt() /
                             (double)voxelSkip());
    else
      return m_params[id].value.toInt();
  }

  QColor getColor(ParamId id) {
    if (m_params[id].type != QMetaType::QColor) return QColor();
    return m_params[id].value.value<QColor>();
  }

  bool getBool(ParamId id) {
    if (m_params[id].type != QMetaType::Bool) return false;
    return m_params[id].value.toBool();
  }

  QString getString(ParamId id) {
    if (m_params[id].type != QMetaType::QString) return QString();
    return m_params[id].value.toString();
  }

  ColorGradient getColorGradient(ParamId id) {
    if (m_params[id].type != QMetaType::User) return ColorGradient();
    return m_params[id].value.value<ColorGradient>();
  }

  // flow management
  bool isFlowDone(FlowId fid) { return m_flowManager.isDone(fid); }
  void setFlowDone(FlowId fid) {
    m_flowManager.setDone(fid);
    emit flowStateChanged();
  }
  void setFlowUndone(FlowId fid) {
    m_flowManager.setUndone(fid);
    emit flowStateChanged();

    if (fid == Flow_Silhouette2Voxel) emit setAutoDepthTempValue(-1.0);
  }

  // mode
  void setMode(Mode mode) {
    if (mode == m_mode) return;
    m_mode = mode;
    emit modeChanged((int)mode);
  }
  Mode getMode() { return m_mode; }

  bool isCloudParam(ParamId pId);
  bool isGlobalParam(ParamId pId) { return !isCloudParam(pId); }

  ParamId getParamIdFromIdString(QString idString);

  void setCurrentCloudIndex(int index, bool emitSignal = true);
  int getCurrentCloudIndex() { return m_currentCloudIndex; }

  void notifyCloudImageRendered() { emit cloudImageRendered(); }
  void notifyAutoDepthTempValue(double val) { emit setAutoDepthTempValue(val); }

  // I/O
  void save();
  void load();

  // render
  int estimateRenderCloudCount(RO_Target target, bool renderHiddenLayers);

  RenderOptions& getRenderOptions() { return m_renderOptions; }

  QString sceneName();

  void checkImgSizeAndChangeSceneSizeIfNeeded(const ParamId id);

  int voxelSkip() { return getInt(ToolbarVoxelSkip); }

  QString getNextCloudName() {
    return QString("Cloud %1").arg(m_totalCloudCount++);
  }

  bool autoPreview() { return m_autoPreview; }
  void setAutoPreview(bool on) { m_autoPreview = on; }

  void saveDefaultParameters();
  void newScene();
signals:
  void paramChanged(ParamId);
  void modeChanged(int);
  void cloudAdded(Cloud*);
  void cloudsCleared();
  void cloudImageRendered();
  void sceneSizeChanged();
  void sceneFilePathChanged();
  void fitZoom();
  void flowStateChanged();

  void setAutoDepthTempValue(double);
};

#endif
