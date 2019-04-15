#include "undomanager.h"

#include "myparams.h"
#include "cloud.h"

#include <assert.h>
#include <QUndoGroup>
#include <QUndoStack>
#include <QPainter>

UndoManager::UndoManager() {
  m_undoGroup     = new QUndoGroup();
  m_drawUndoStack = new QUndoStack();
  m_undoGroup->addStack(m_drawUndoStack);
  m_drawUndoStack->setActive(true);
}

UndoManager* UndoManager::instance() {
  static UndoManager _instance;
  return &_instance;
}

QUndoStack* UndoManager::stack() { return m_undoGroup->activeStack(); }

void UndoManager::setTmpCommand(QUndoCommand* command) {
  assert(m_tmpCommand == nullptr && command != nullptr);
  m_tmpCommand = command;
}
void UndoManager::releaseTmpCommand() {
  assert(m_tmpCommand != nullptr);
  m_tmpCommand = nullptr;
}

//---------------------------

DrawUndo::DrawUndo(Cloud* cloud, QPointF pos, bool isDraw, QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_cloud(cloud)
    , m_bbox(pos, pos + QPointF(1, 1))
    , m_beforePm(cloud->getCanvasPm().copy())
    , m_beforeDrawBBox(cloud->getDrawingBBox())
    , m_type((isDraw) ? Draw : Erase) {}

void DrawUndo::setPixmap(bool isUndo) {
  MyParams* p = MyParams::instance();
  assert(m_cloud == p->getCurrentCloud());
  QPainter painter(&m_cloud->getCanvasPm());
  QRect rect      = m_bbox.toRect().adjusted(-10, -10, 10, 10);
  QSize sceneSize = p->getSceneSize();
  rect            = rect.intersected(QRect(QPoint(0, 0), sceneSize));
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  if (isUndo) {
    painter.drawPixmap(rect, m_beforePm);
    m_cloud->setDrawingBBox(m_beforeDrawBBox);
  } else {
    painter.drawPixmap(rect, m_afterPm);
    m_cloud->setDrawingBBox(m_afterDrawBBox);
  }

  painter.end();

  if (m_type == Draw) m_cloud->autoFill();

  m_cloud->suspendRender();
  p->setFlowUndone(Flow_Silhouette2Voxel);
  UndoManager::instance()->notifyViewer();
}

void DrawUndo::undo() { setPixmap(true); }

void DrawUndo::redo() {
  if (m_isOnPush) {
    m_isOnPush = false;
    return;
  }
  setPixmap(false);
}

void DrawUndo::addPos(const QPointF& pos) {
  m_bbox = m_bbox.united(QRectF(pos, pos + QPointF(1, 1)));
}

void DrawUndo::onEndProcess() {
  QRect rect      = m_bbox.toRect().adjusted(-10, -10, 10, 10);
  QSize sceneSize = MyParams::instance()->getSceneSize();
  rect            = rect.intersected(QRect(QPoint(0, 0), sceneSize));
  m_beforePm      = m_beforePm.copy(rect);
  m_afterPm       = m_cloud->getCanvasPm().copy(rect);
  m_afterDrawBBox = m_cloud->getDrawingBBox();
}

//---------------------------

ParameterUndo::ParameterUndo(Cloud* cloud, ParamId pId, QVariant& beforeVal,
                             QUndoCommand* parent)
    : QUndoCommand(parent)
    , m_cloud(cloud)
    , m_pId(pId)
    , m_beforeVal(beforeVal) {}

void ParameterUndo::undo() {
  MyParams* p  = MyParams::instance();
  Cloud* cloud = p->getCurrentCloud();
  if (cloud != m_cloud) return;
  Param& param = p->getParam(m_pId);
  param.value  = m_beforeVal;
  p->notifyParamChanged(m_pId, false);
}

void ParameterUndo::redo() {
  if (m_isOnPush) {
    m_isOnPush = false;
    return;
  }
  MyParams* p  = MyParams::instance();
  Cloud* cloud = p->getCurrentCloud();
  if (cloud != m_cloud) return;
  Param& param = p->getParam(m_pId);
  param.value  = m_afterVal;
  p->notifyParamChanged(m_pId, false);
}

void ParameterUndo::setAfterVal(const QVariant& val) { m_afterVal = val; }

//---------------------------

MoveCloudUndo::MoveCloudUndo(Cloud* cloud, QUndoCommand* parent)
    : QUndoCommand(parent), m_cloud(cloud) {
  m_before.cloudImageBBox = cloud->getCloudImageBBox();
  m_before.drawingBBox    = cloud->getDrawingBBox();
  m_cloudPm               = cloud->getCanvasPm().copy(m_before.cloudImageBBox);
  m_beforeBBox            = cloud->getBBox();
}

void MoveCloudUndo::setOffset(const QPoint& offset) {
  m_before.offset        = -offset;
  m_after.offset         = offset;
  m_after.cloudImageBBox = m_cloud->getCloudImageBBox();
  m_after.drawingBBox    = m_cloud->getDrawingBBox();
  m_bboxChanged          = m_beforeBBox.size() != m_cloud->getBBox().size();
}

void MoveCloudUndo::apply(CloudGeom& geom) {
  m_cloud->getCanvasPm().fill(Qt::transparent);
  QPainter painter(&m_cloud->getCanvasPm());
  painter.drawPixmap(geom.cloudImageBBox, m_cloudPm);
  painter.end();
  m_cloud->addPosOffset(geom.offset);
  m_cloud->setDrawingBBox(geom.drawingBBox);
  m_cloud->setCloudImageBBox(geom.cloudImageBBox);

  resetFlows();
  UndoManager::instance()->notifyViewer();
}

void MoveCloudUndo::undo() { apply(m_before); }

void MoveCloudUndo::redo() { apply(m_after); }

void MoveCloudUndo::resetFlows() {
  MyParams* p = MyParams::instance();
  m_cloud->suspendRender();
  if (m_bboxChanged)
    p->setFlowUndone(Flow_Silhouette2Voxel);
  else if (p->getDouble(ShapeOblateness) > 1.0) {  // when the ball is oblate
    p->setFlowUndone(Flow_ArrangeMetaballs);
    p->setFlowUndone(Flow_GenerateWorlyNoise);
  } else
    p->setFlowUndone(Flow_RayMarchingRender);
}
