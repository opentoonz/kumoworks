#include "myparams.h"
#include "cloud.h"
#include "sky.h"
#include "undomanager.h"
#include "pathutils.h"
#include <QFileDialog>
#include <QSettings>
#include <QMainWindow>
#include <QPainter>
#include <QMessageBox>

using namespace PathUtils;

MyParams::MyParams() : m_sceneSize(1280, 720) {
  m_params[LayoutImgPath] =
      Param("LayoutImgPath", tr("Layout Image"), Background, QMetaType::QString,
            Flow_GetLayoutImage, "");
  m_params[LayoutImgOpacity] =
      Param("LayoutImgOpacity", tr("Opacity"), Background, QMetaType::Double,
            Flow_None, 1.0, 0.0, 1.0);
  m_params[BackgroundImgPath] =
      Param("BackgroundImgPath", tr("Background"), Background,
            QMetaType::QString, Flow_GetBackgroundImage, "");
  m_params[BackgroundImgOpacity] =
      Param("BackgroundImgOpacity", tr("Opacity"), Background,
            QMetaType::Double, Flow_CombineBackground, 1.0, 0.0, 1.0);

  m_params[CameraFov] = Param("CameraFov", tr("Fov"), Camera, QMetaType::Double,
                              Flow_GenerateSky, 45.0, 10.0, 120.0);
  m_params[CameraFixEyeLevel] =
      Param("CameraFixEyeLevel", tr("Fix Eye Level"), Camera, QMetaType::Bool,
            Flow_None, false);
  m_params[CameraElevation] =
      Param("CameraElevation", tr("Elevation"), Camera, QMetaType::Double,
            Flow_GenerateSky, 20.0, -30.0, 90.0);

  m_params[SunUseUniqueAngles] =
      Param("SunUseUniqueAngles", tr("Use Unique Angles"), Sun, QMetaType::Bool,
            Flow_GenerateSky, false);
  m_params[SunUniqueElevation] =
      Param("SunUniqueElevation", tr("Elevation"), Sun, QMetaType::Double,
            Flow_RayMarchingRender, 40.0, 0.0, 90.0);
  m_params[SunUniqueAzimuth] =
      Param("SunUniqueAzimuth", tr("Azimuth"), Sun, QMetaType::Double,
            Flow_RayMarchingRender, 0.0, -180.0, 180.0);
  m_params[SunElevation] =
      Param("SunElevation", tr("Elevation"), Sun, QMetaType::Double,
            Flow_GenerateSky, 33.0, 0.0, 90.0);
  m_params[SunAzimuth] =
      Param("SunAzimuth", tr("Azimuth"), Sun, QMetaType::Double,
            Flow_GenerateSky, 40.0, -180.0, 180.0);

  m_params[SkyTurbidity] =
      Param("SkyTurbidity", tr("Turbidity"), SkyRender, QMetaType::Double,
            Flow_GenerateSky, 2.0, 1.1, 10.0);

  m_params[SkyRenderSkip] = Param("SkyRenderSkip", tr("Sky Skip"), SkyRender,
                                  QMetaType::Int, Flow_GenerateSky, 10, 1, 10);
  m_params[SkyRenderGrid] = Param("SkyRenderGrid", tr("Sky Grid"), SkyRender,
                                  QMetaType::Bool, Flow_None, false);

  m_params[CameraFov].addFlow(Flow_RayMarchingRender);
  m_params[CameraFov].addFlow(Flow_GetSunRectOnViewer);
  m_params[CameraFov].addFlow(Flow_GetEyeLevelPosOnViewer);
  m_params[CameraFov].addFlow(Flow_ArrangeMetaballs);  // needed for oblate
                                                       // balls
  m_params[CameraFov].addFlow(
      Flow_GenerateWorlyNoise);  // needed for oblate balls
  m_params[CameraElevation].addFlow(
      Flow_CompositeMetaballAndNoise);  // needed for bottom cut off
  m_params[CameraElevation].addFlow(
      Flow_GetEyeLevelPosOnViewer);  // needed for bottom cut off
  m_params[CameraElevation].addFlow(Flow_GetSunRectOnViewer);
  m_params[CameraElevation].addFlow(
      Flow_ArrangeMetaballs);  // needed for oblate balls
  m_params[CameraElevation].addFlow(
      Flow_GenerateWorlyNoise);  // needed for oblate balls
  m_params[SunElevation].addFlow(Flow_RayMarchingRender);
  m_params[SunElevation].addFlow(Flow_GetSunRectOnViewer);
  m_params[SunAzimuth].addFlow(Flow_RayMarchingRender);
  m_params[SunAzimuth].addFlow(Flow_GetSunRectOnViewer);
  m_params[SunUniqueElevation].addFlow(Flow_GetUniqueSunRectOnViewer);
  m_params[SunUniqueAzimuth].addFlow(Flow_GetUniqueSunRectOnViewer);
  m_params[SunUseUniqueAngles].addFlow(Flow_GetUniqueSunRectOnViewer);
  m_params[SunUseUniqueAngles].addFlow(Flow_RayMarchingRender);

  m_params[ShapeDepth] =
      Param("ShapeDepth", tr("Depth"), Shape1, QMetaType::Double,
            Flow_Silhouette2Voxel, 60.0, 10.0, 1000.0, true);
  m_params[ShapeDepthAuto] =
      Param("ShapeDepthAuto", tr("Auto Depth"), Shape1, QMetaType::Bool,
            Flow_Silhouette2Voxel, true);
  m_params[ShapeFollowingAccuracy] =
      Param("ShapeFollowingAccuracy", tr("Following Accuracy"), Shape1,
            QMetaType::Double, Flow_ArrangeMetaballs, 0.5, 0.0, 1.0);
  m_params[ShapeRandomSeed] =
      Param("ShapeRandomSeed", tr("Random Seed"), Shape1, QMetaType::Int,
            Flow_ArrangeMetaballs, 0, 0, 255);
  m_params[ShapeBottomCutOff] =
      Param("ShapeBottomCutOff", tr("Bottom Cut Off"), Shape1,
            QMetaType::Double, Flow_CompositeMetaballAndNoise, 0.0, 0.0, 1.0);
  m_params[ShapeBottomCutOffBlur] =
      Param("ShapeBottomCutOffBlur", tr("Bottom Cut Off Blur"), Shape1,
            QMetaType::Double, Flow_CompositeMetaballAndNoise, 0.0, 0.0, 0.2);

  m_params[ShapeRadiusBase] =
      Param("ShapeRadiusBase", tr("Radius"), Shape2, QMetaType::Double,
            Flow_ArrangeMetaballs, 50, 5.0, 120.0, true);
  m_params[ShapeRadiusRandom] =
      Param("ShapeRadiusRandom", tr("Radius Random"), Shape2, QMetaType::Double,
            Flow_ArrangeMetaballs, 0.2, 0.0, 0.9);
  m_params[ShapeOblateness] =
      Param("ShapeOblateness", tr("Oblateness"), Shape2, QMetaType::Double,
            Flow_ArrangeMetaballs, 2.5, 1.0, 10.0);
  m_params[ShapeRadiusRatio] =
      Param("ShapeRadiusRatio", tr("Radius Ratio"), Shape2, QMetaType::Double,
            Flow_ArrangeMetaballs, 0.5, 0.1, 0.9);
  m_params[ShapeIntensityRatio] =
      Param("ShapeIntensityRatio", tr("Intensity Ratio"), Shape2,
            QMetaType::Double, Flow_ArrangeMetaballs, 1.0, 0.2, 5.0);
  m_params[ShapeBBoxMargin] =
      Param("ShapeBBoxMargin", tr("Margin"), Shape2, QMetaType::Int,
            Flow_Silhouette2Voxel, 45.0, 20.0, 200.0);

  m_params[ShapeRadiusBase].addFlow(Flow_GenerateWorlyNoise);
  m_params[ShapeRadiusBase].addFlow(Flow_GenerateSimplexNoise);
  m_params[ShapeOblateness].addFlow(Flow_GenerateWorlyNoise);
  m_params[ShapeOblateness].addFlow(Flow_GenerateSimplexNoise);

  m_params[WorleyNoiseSizeRatio] =
      Param("WorleyNoiseSizeRatio", tr("Worley Noise Scale"), Noise,
            QMetaType::Double, Flow_GenerateWorlyNoise, 1.4, 0.1, 5.0);
  m_params[WorleyNoiseRate] =
      Param("WorleyNoiseRate", tr("Worley Noise Rate"), Noise,
            QMetaType::Double, Flow_CompositeNoises, 1.0, 0.0, 8.0);
  m_params[WorleyNoiseRandomSeed] =
      Param("WorleyNoiseRandomSeed", tr("Worley Noise Seed"), Noise,
            QMetaType::Int, Flow_GenerateWorlyNoise, 1, 1, 256);
  m_params[SimplexNoiseSizeBaseRatio] =
      Param("SimplexNoiseSizeBaseRatio", tr("Simplex Noise Scale"), Noise,
            QMetaType::Double, Flow_GenerateSimplexNoise, 1.4, 0.1, 5.0);
  m_params[SimplexNoiseRate] =
      Param("SimplexNoiseRate", tr("Simplex Noise Rate"), Noise,
            QMetaType::Double, Flow_CompositeNoises, 1.0, 0.0, 8.0);
  m_params[NoisePrevalence] =
      Param("NoisePrevalence", tr("Noise Prevalence"), Noise, QMetaType::Double,
            Flow_CompositeMetaballAndNoise, 1.0, 0.1, 1.0);

  m_params[LayerVisibility] = Param("LayerVisibility", tr("Visibility Toggle"),
                                    Others, QMetaType::Bool, Flow_None, true);

  m_params[RenderRayStep] =
      Param("RenderRayStep", tr("Ray Step"), Render, QMetaType::Double,
            Flow_RayMarchingRender, 2.0, 1.0, 15.0);
  m_params[RenderCloudDensity] = Param(
      "RenderCloudDensity", tr("Cloud Density"), Render, QMetaType::Double,
      Flow_RayMarchingRender, 0.05, 0.01, 0.2, true, 3);
  m_params[RenderAerialRatio] =
      Param("RenderAerialRatio", tr("Aerial Perspective"), Render,
            QMetaType::Double, Flow_GetCloudImage, 0.0, 0.0, 0.99);
  m_params[RenderReflectionColor] =
      Param("RenderReflectionColor", tr("Reflection Color"), Render,
            QMetaType::QColor, Flow_RenderReflection, QColor(192, 213, 211));
  m_params[RenderReflectionAlpha] =
      Param("RenderReflectionAlpha", tr("Reflection Alpha"), Render,
            QMetaType::Double, Flow_RenderReflection, 0.0, 0.0, 1.0);

  ColorGradient gradient;
  gradient.setColorAt(0, QColor(121, 137, 163));
  gradient.setColorAt(0.5, QColor(211, 216, 225));
  gradient.setColorAt(1, Qt::white);
  m_params[RenderToneMap] =
      Param("RenderToneMap", tr("Tone Map"), Render, QMetaType::User,
            Flow_GetCloudImage, gradient);

  m_params[ToolbarVoxelSkip] =
      Param("ToolbarVoxelSkip", tr("Preview quality:"), Others, QMetaType::Int,
            Flow_Silhouette2Voxel, 3, 1, 4);
  m_params[ToolbarVoxelSkip].addFlow(Flow_GenerateSimplexNoise);
  m_params[ToolbarVoxelSkip].addFlow(Flow_GenerateWorlyNoise);
}

MyParams* MyParams::instance() {
  static MyParams _instance;
  return &_instance;
}

void MyParams::notifyParamChanged(ParamId paramId, bool isDragging) {
  for (FlowId& fid : m_params[paramId].flowIds) {
    // set the undone flag of coresponding flow.
    // 対応するフローの再計算のフラグを立てる
    setFlowUndone(fid);
    if (getCurrentCloud()) {
      if (fid != Flow_GenerateSky) getCurrentCloud()->suspendRender();
      if (autoPreview() && voxelSkip() >= 3 && !isDragging &&
          getMode() == PreviewMode)
        getCurrentCloud()->triggerRender();
    }
  }
  if (isCloudParam(paramId) && !isDragging) {
    getCurrentCloud()->setParam(paramId, m_params[paramId].value);
  }
  emit paramChanged(paramId);
}

void MyParams::addCloud(Cloud* c) {
  if (Cloud* oldCloud = getCurrentCloud()) {
    oldCloud->suspendRender();
    oldCloud->releaseBuffers();
  }
  m_clouds.push_front(c);
  setCurrentCloudIndex(0);
  emit cloudAdded(c);
  setMode(DrawMode);
}

void MyParams::removeCloud() { delete m_clouds.takeAt(m_currentCloudIndex); }

void MyParams::setSceneSize(QSize size) {
  if (m_sceneSize == size) return;
  m_sceneSize = size;

  setFlowUndone(Flow_GenerateSky);
  setFlowUndone(Flow_RayMarchingRender);
  setFlowUndone(Flow_GetSunRectOnViewer);
  setFlowUndone(Flow_GetEyeLevelPosOnViewer);

  for (Cloud* cloud : getClouds()) {
    auto temp = size;
    cloud->onResize(temp);
  }

  emit sceneSizeChanged();
}

MyParams::~MyParams() {
  for (int c = 0; c < m_clouds.size(); c++) delete m_clouds[c];
}

bool MyParams::isCloudParam(ParamId pId) {
  if (pId >= SunUseUniqueAngles && pId <= SunUniqueAzimuth)
    return true;
  else if (pId == LayerVisibility)
    return true;
  ParamCategory category = m_params.value(pId).category;
  return category >= Shape1 && category <= Render;
}

ParamId MyParams::getParamIdFromIdString(QString idString) {
  QMapIterator<ParamId, Param> i(m_params);
  while (i.hasNext()) {
    i.next();
    if (i.value().idString == idString) return i.key();
  }
  return InvalidParam;
}

// emitSignal is false when rendering. emitSignalはレンダリング時にfalse
void MyParams::setCurrentCloudIndex(int index, bool emitSignal) {
  if (index < 0 || index >= m_clouds.size()) return;
  if (Cloud* previousCloud = getCurrentCloud()) {
    previousCloud->suspendRender();
    previousCloud->releaseBuffers();
  }

  m_currentCloudIndex = index;

  // set current cloud parameters to the global ones. param に
  // 選択した雲の値を入れる
  Cloud* cloud = getCurrentCloud();
  for (int pId = 0; pId < ParamCount; pId++) {
    if (isCloudParam((ParamId)pId)) {
      getParam((ParamId)pId).value = cloud->getParam((ParamId)pId);
      if (emitSignal) emit paramChanged((ParamId)pId);
    }
  }

  setFlowUndone(Flow_Silhouette2Voxel);

  // reset undo when switching clouds
  // 雲を切り替えたときにUndoをリセットする
  UndoManager::instance()->stack()->clear();
}

void MyParams::save() {
  qRegisterMetaTypeStreamOperators<ColorGradient>("ColorGradient");
  QString fileName = QFileDialog::getSaveFileName(
      nullptr, tr("Save File"), m_sceneFilePath, tr("SkyTool files (*.sky)"));

  std::cout << "fileName = " << fileName.toStdString() << std::endl;
  if (fileName.isEmpty()) return;  // canceled case

  QSettings settings(fileName, QSettings::IniFormat);

  // save global parameters
  settings.beginGroup("Global");
  settings.setValue("sceneSize", m_sceneSize);
  settings.setValue("cloudCount", m_clouds.size());
  settings.setValue("totalCloudCount", m_totalCloudCount);
  for (int pId = 0; pId < ParamCount; pId++) {
    if (isGlobalParam((ParamId)pId)) {
      Param param = getParam((ParamId)pId);
      settings.setValue(param.idString, param.value);
    }
  }
  settings.endGroup();

  // save cloud parameters
  int cId = 0;
  for (Cloud* cloud : m_clouds) {
    settings.beginGroup(QString("Cloud%1").arg(cId));
    cloud->saveData(settings);
    settings.endGroup();
    cId++;
  }

  settings.beginGroup("RenderOptions");
  settings.setValue("target", (int)m_renderOptions.target);
  settings.setValue("region", (int)m_renderOptions.region);
  settings.setValue("background", (int)m_renderOptions.background);
  settings.setValue("voxelSkip", m_renderOptions.voxelSkip);
  settings.setValue("dirPath", m_renderOptions.dirPath);
  settings.setValue("fileName", m_renderOptions.fileName);
  settings.setValue("renderHiddenLayers", m_renderOptions.renderHiddenLayers);
  settings.endGroup();

  m_sceneFilePath = fileName;
  emit sceneFilePathChanged();
}

void MyParams::load() {
  qRegisterMetaTypeStreamOperators<ColorGradient>("ColorGradient");
  QString fileName = QFileDialog::getOpenFileName(
      nullptr, tr("Open File"), m_sceneFilePath, tr("SkyTool files (*.sky)"));
  if (fileName.isEmpty()) return;  // canceled case

  QSettings settings(fileName, QSettings::IniFormat);

  // load global parameters
  settings.beginGroup("Global");
  setSceneSize(settings.value("sceneSize", m_sceneSize).toSize());

  int cloudCount = settings.value("cloudCount").toInt();
  for (QString& key : settings.childKeys()) {
    ParamId pId = getParamIdFromIdString(key);
    if (pId == InvalidParam) continue;
    if (isGlobalParam((ParamId)pId)) {
      getParam((ParamId)pId).value = settings.value(key);
      notifyParamChanged((ParamId)pId, false);
    }
  }
  settings.endGroup();

  m_clouds.clear();
  emit cloudsCleared();

  m_totalCloudCount = 1;
  // load cloud parameters
  for (int cId = cloudCount - 1; cId >= 0; cId--) {
    settings.beginGroup(QString("Cloud%1").arg(cId));
    QString name = settings.value("Name", QString()).toString();
    Cloud* cloud = new Cloud(name);
    if (!name.isEmpty()) m_totalCloudCount++;
    cloud->loadData(settings);
    settings.endGroup();
    addCloud(cloud);
  }
  setCurrentCloudIndex(0);

  if (settings.childGroups().contains("RenderOptions")) {
    settings.beginGroup("RenderOptions");
    m_renderOptions.target =
        (RO_Target)settings.value("target", (int)m_renderOptions.target)
            .toInt();
    m_renderOptions.region =
        (RO_Region)settings.value("region", (int)m_renderOptions.region)
            .toInt();
    m_renderOptions.background =
        (RO_Background)settings
            .value("background", (int)m_renderOptions.background)
            .toInt();
    m_renderOptions.voxelSkip =
        settings.value("voxelSkip", m_renderOptions.voxelSkip).toInt();
    m_renderOptions.dirPath =
        settings.value("dirPath", m_renderOptions.dirPath).toString();
    m_renderOptions.fileName =
        settings.value("fileName", m_renderOptions.fileName).toString();
    m_renderOptions.renderHiddenLayers =
        settings.value("renderHiddenLayers", m_renderOptions.renderHiddenLayers)
            .toBool();
    settings.endGroup();
  }

  m_totalCloudCount =
      settings.value("totalCloudCount", m_totalCloudCount).toInt();

  m_sceneFilePath = fileName;
  emit sceneFilePathChanged();
  emit fitZoom();
}

//----------------------------------

int MyParams::estimateRenderCloudCount(RO_Target target,
                                       bool renderHiddenLayers) {
  int ret               = 0;
  int currentCloudIndex = m_currentCloudIndex;

  // when combining all clouds. 全ての雲を合成する場合
  if (target == CombineAllCloud) {
    for (int c = getCloudCount() - 1; c >= 0; c--) {
      Cloud* cloud = getCloud(c);
      if (!renderHiddenLayers && !cloud->isVisible()) continue;
      ret++;
    }
    return ret;
  }
  // when rendering each cloud.
  // ここから、個別に雲をレンダリングする場合

  int cId = 0;
  for (Cloud* cloud : m_clouds) {
    // in case of rendering the current cloud only. 現在の雲のみ出力の場合
    if (target == CurrentCloudOnly && cId != m_currentCloudIndex) {
      cId++;
      continue;
    } else if (target == EachCloud && !renderHiddenLayers &&
               !cloud->isVisible()) {
      cId++;
      continue;
    }
    ret++;
  }
  return ret;
}

//----------------------------------

QString MyParams::sceneName() {
  if (m_sceneFilePath.isEmpty())
    return tr("Untitled");
  else
    return QFileInfo(m_sceneFilePath).fileName();
}

void MyParams::checkImgSizeAndChangeSceneSizeIfNeeded(const ParamId id) {
  assert(id == LayoutImgPath || id == BackgroundImgPath);

  QPixmap loadedImg = (id == LayoutImgPath)
                          ? Sky::instance()->getLayoutImage()
                          : Sky::instance()->getBackgroundImage();

  if (loadedImg.isNull()) return;

  QSize imgSize = loadedImg.size();

  if (getSceneSize() == imgSize) return;

  QString str =
      tr("Loaded image has different size to the current scene size.\n\
Do you want to fit the scene size to the image?\n\
( Current scene size : %1 x %2  ->  New scene size : %3 x %4 )")
          .arg(getSceneSize().width())
          .arg(getSceneSize().height())
          .arg(imgSize.width())
          .arg(imgSize.height());

  if (QMessageBox::Yes == QMessageBox::question(nullptr, tr("Question"), str)) {
    setSceneSize(imgSize);
    emit fitZoom();

    // signal for updating the toolbar (and tile bar)
    // ツールバーを更新するためのシグナル。タイトルバーも更新しちゃうけど、まあよかろう
    emit sceneFilePathChanged();
  }
}

void MyParams::saveDefaultParameters() {
  qRegisterMetaTypeStreamOperators<ColorGradient>("ColorGradient");
  QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);

  // save global parameters
  settings.beginGroup("Global");
  settings.setValue("sceneSize", m_sceneSize);
  for (int pId = 0; pId < ParamCount; pId++) {
    Param param = getParam((ParamId)pId);
    settings.setValue(param.idString, param.value);
  }
  settings.endGroup();

  settings.beginGroup("RenderOptions");
  settings.setValue("target", (int)m_renderOptions.target);
  settings.setValue("region", (int)m_renderOptions.region);
  settings.setValue("background", (int)m_renderOptions.background);
  settings.setValue("voxelSkip", m_renderOptions.voxelSkip);
  settings.setValue("dirPath", m_renderOptions.dirPath);
  settings.setValue("fileName", m_renderOptions.fileName);
  settings.endGroup();
}

void MyParams::newScene() {
  m_clouds.clear();
  emit cloudsCleared();
  m_totalCloudCount = 1;
  Cloud* cloud      = new Cloud();
  addCloud(cloud);

  // if there is a default parameter setting file, load it
  // デフォルト値ファイルがある場合、そちらを読み込む
  if (QFile::exists(getDefaultSettingsPath())) {
    qRegisterMetaTypeStreamOperators<ColorGradient>("ColorGradient");
    QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);

    // save global parameters
    settings.beginGroup("Global");
    setSceneSize(settings.value("sceneSize", m_sceneSize).toSize());
    for (QString& key : settings.childKeys()) {
      ParamId pId = getParamIdFromIdString(key);
      if (pId == InvalidParam) continue;
      getParam((ParamId)pId).value = settings.value(key);
      if (isCloudParam((ParamId)pId))
        cloud->getParam(pId) = settings.value(key);
      notifyParamChanged((ParamId)pId, false);
    }
    settings.endGroup();

    settings.beginGroup("RenderOptions");
    m_renderOptions.target =
        (RO_Target)settings.value("target", (int)m_renderOptions.target)
            .toInt();
    m_renderOptions.region =
        (RO_Region)settings.value("region", (int)m_renderOptions.region)
            .toInt();
    m_renderOptions.background =
        (RO_Background)settings
            .value("background", (int)m_renderOptions.background)
            .toInt();
    m_renderOptions.voxelSkip =
        settings.value("voxelSkip", m_renderOptions.voxelSkip).toInt();
    m_renderOptions.dirPath =
        settings.value("dirPath", m_renderOptions.dirPath).toString();
    m_renderOptions.fileName =
        settings.value("fileName", m_renderOptions.fileName).toString();
    settings.endGroup();
  } else {
    setSceneSize(QSize(1280, 720));
    for (int pId = 0; pId < ParamCount; pId++) {
      Param param = getParam((ParamId)pId);
      param.value = param.defaultValue;
      if (isCloudParam((ParamId)pId))
        cloud->getParam((ParamId)pId) = param.defaultValue;
      notifyParamChanged((ParamId)pId, false);
    }
    m_renderOptions.target     = EachCloud;
    m_renderOptions.region     = WholeSceneSize;
    m_renderOptions.background = RenderCloudOnly;
    m_renderOptions.voxelSkip  = 2;
    m_renderOptions.dirPath    = "";
    m_renderOptions.fileName   = "";
  }
}
