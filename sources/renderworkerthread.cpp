#include "renderworkerthread.h"

#include "sky.h"
#include "cloud.h"

#include <QPainter>

void RenderWorkerThread::precomputeBg() {
  MyParams* p = MyParams::instance();
  // temporary set the sky skip to 1 and obtain the sky image
  // 空の描画ステップを１にして背景画像を取得
  int skySkip = p->getInt(SkyRenderSkip);
  if (skySkip > 1) {
    p->getParam(SkyRenderSkip).value = 1;
    p->notifyParamChanged(SkyRenderSkip);
  }
  m_bgImage = Sky::instance()->getCombinedBackground();
  if (skySkip > 1) {
    p->getParam(SkyRenderSkip).value = skySkip;
    p->notifyParamChanged(SkyRenderSkip);
  }
}

void RenderWorkerThread::run() {
  MyParams* p = MyParams::instance();

  RO_Target target         = m_ro.target;
  bool renderHiddenLayers  = m_ro.renderHiddenLayers;
  RO_Region region         = m_ro.region;
  RO_Background background = m_ro.background;
  int renderVoxelSkip      = m_ro.voxelSkip;
  QString folder           = m_ro.dirPath;
  QString fileName         = m_ro.fileName;

  QSize sceneSize = p->getSceneSize();

  auto getOutPath = [&](QString layerName = QString()) -> QString {
    QString ret = folder + "/" + fileName;
    if (!layerName.isEmpty()) ret += "-" + layerName;
    ret += ".png";
    return ret;
  };
  auto getSkyPath = [&]() -> QString {
    return folder + "/" + fileName + "_sky.png";
  };

  int oldVoxelSkip = p->voxelSkip();
  if (oldVoxelSkip != renderVoxelSkip) {
    p->getParam(ToolbarVoxelSkip).value = renderVoxelSkip;
    p->notifyParamChanged(ToolbarVoxelSkip);
  }

  auto revertParams = [&] {
    // revert the voxel skip. ボクセルスキップ元に戻す
    if (oldVoxelSkip != renderVoxelSkip) {
      p->getParam(ToolbarVoxelSkip).value = oldVoxelSkip;
      p->notifyParamChanged(ToolbarVoxelSkip);
    }
  };

  // when conbining all clouds. 全ての雲を合成する場合
  if (target == CombineAllCloud) {
    QImage img(sceneSize, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter painter(&img);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // composite with sky image 空画像描画
    if (background == CompositeSky)
      painter.drawImage(QRect(QPoint(0, 0), sceneSize), m_bgImage);

    QRect boundingRect;

    for (int c = p->getCloudCount() - 1; c >= 0; c--) {
      Cloud* refCloud = p->getCloud(c);

      if (!renderHiddenLayers && !refCloud->isVisible()) continue;

      if (m_isCanceled) {
        revertParams();
        emit renderFinished(Canceled);
        return;
      }

      // changing the current cloud index here will undone Flow_Silhouette2Voxel
      // ↓でFlow_Silhouette2VoxelがUndoneされるので、
      // 最初からレンダリングしなおすことになる
      p->setCurrentCloudIndex(c, false);

      Cloud cloud(refCloud);

      cloud.triggerRender();

      while (cloud.isCloudProcessing()) {
        cloud.getCloudImage();
        if (m_isCanceled) {
          revertParams();
          emit renderFinished(Canceled);
          return;
        }
      }

      QImage cloudImg = cloud.getCloudImage();
      painter.drawImage(cloud.getCloudImageBBox(), cloudImg);

      boundingRect = boundingRect.united(cloud.getCloudImageBBox());

      emit cloudRendered();
    }

    painter.end();

    // cut with the bounding box バウンディングボックスで切り抜き
    if (region == CloudBoundingBox) {
      img = img.copy(boundingRect);
    }

    // save the image 画像を保存
    bool ret = img.save(getOutPath(), "PNG", 100);

    // save the sky image separately 別途空画像を出力
    if (background == RenderSkySeparately)
      ret = ret && m_bgImage.save(getSkyPath(), "PNG", 100);

    revertParams();

    emit renderFinished((ret) ? Finished : Failed);
    return;
  }

  // In case of rendering each cloud
  // ここから、個別に雲をレンダリングする場合

  bool ret       = true;
  int cId        = 0;
  int currentCId = MyParams::instance()->getCurrentCloudIndex();

  QList<QString> savedNames;

  for (Cloud* refCloud : p->getClouds()) {
    // in case of rendering the current cloud only 現在の雲のみ出力の場合
    if (target == CurrentCloudOnly && cId != currentCId) {
      cId++;
      continue;
    } else if (target == EachCloud && !renderHiddenLayers &&
               !refCloud->isVisible()) {
      cId++;
      continue;
    }

    if (m_isCanceled) {
      revertParams();
      emit renderFinished(Canceled);
      return;
    }

    QImage img(sceneSize, QImage::Format_ARGB32);
    img.fill(Qt::transparent);
    QPainter painter(&img);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    if (background == CompositeSky)
      painter.drawImage(QRect(QPoint(0, 0), sceneSize), m_bgImage);

    // changing the current cloud index here will undone Flow_Silhouette2Voxel
    // ↓でFlow_Silhouette2VoxelがUndoneされるので、
    // 最初からレンダリングしなおすことになる
    p->setCurrentCloudIndex(cId, false);

    Cloud cloud(refCloud);

    cloud.triggerRender();

    while (cloud.isCloudProcessing()) {
      cloud.getCloudImage();
      if (m_isCanceled) {
        revertParams();
        emit renderFinished(Canceled);
        return;
      }
    }

    QImage cloudImg = cloud.getCloudImage();
    painter.drawImage(cloud.getCloudImageBBox(), cloudImg);

    painter.end();

    // cut with the bouding box バウンディングボックスで切り抜き
    if (region == CloudBoundingBox) {
      img = img.copy(cloud.getCloudImageBBox());
    }

    // save the image 画像を保存

    // obtain layer name to be put in the file name
    QString layerName = refCloud->getName();
    if (savedNames.contains(layerName)) {
      int suffix = 1;
      while (1) {
        QString tmpName = layerName + QString("-%1").arg(suffix);
        if (!savedNames.contains(tmpName)) {
          // cloud layer name to be put in the file name with suffix
          layerName = tmpName;
          break;
        }
        suffix++;
      }
    }

    ret = ret && img.save(getOutPath(layerName), "PNG", 100);

    emit cloudRendered();
    cId++;
    savedNames.append(layerName);
  }

  // save the sky image separately 別途空画像を出力
  if (background == RenderSkySeparately)
    ret = ret && m_bgImage.save(getSkyPath(), "PNG", 100);

  revertParams();

  p->setFlowUndone(Flow_Silhouette2Voxel);

  emit renderFinished((ret) ? Finished : Failed);
  return;
}

RenderWorkerThread::RenderWorkerThread(RenderOptions& ro, QObject* parent)
    : QThread(parent), m_ro(ro) {
  precomputeBg();
}
