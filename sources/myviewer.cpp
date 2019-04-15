
#define _USE_MATH_DEFINES
#include "myviewer.h"

#include <iostream>
#include <cmath>
#include "sky.h"
#include "myparams.h"
#include "cloud.h"
#include "undomanager.h"
#include <array>

#include <QPainter>
#include <QMouseEvent>
#include <QTime>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QMessageBox>
#include <QTimer>

MyViewer::MyViewer(QWidget* parent) : QWidget(parent) {
  connect(MyParams::instance(), SIGNAL(paramChanged(ParamId)), this,
          SLOT(update()));
  connect(MyParams::instance(), SIGNAL(modeChanged(int)), this, SLOT(update()));
  connect(MyParams::instance(), SIGNAL(sceneSizeChanged()), this,
          SLOT(update()));
  connect(MyParams::instance(), SIGNAL(fitZoom()), this,
          SLOT(fitZoomToScene()));

  connect(UndoManager::instance(), SIGNAL(updateViewer()), this,
          SLOT(update()));

  setMouseTracking(true);
}

void MyViewer::paintEvent(QPaintEvent* e) {
  qreal scale = std::sqrt(m_transform.det());

  MyParams* p = MyParams::instance();
  QPainter painter(this);
  painter.fillRect(rect(), QColor(48, 48, 48));

  painter.setWorldTransform(m_transform);

  QRect sceneRect(QPoint(0, 0), p->getSceneSize());

  painter.drawImage(sceneRect, Sky::instance()->getCombinedBackground());

  if (p->getBool(SkyRenderGrid)) drawSkyGrid(painter);

  // draw clouds
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  for (int c = p->getCloudCount() - 1; c >= 0; c--) {
    if (c == p->getCurrentCloudIndex() && p->getMode() == DrawMode) continue;

    Cloud* cloud = p->getCloud(c);

    if (cloud && !cloud->getBBox().isEmpty() && cloud->isVisible()) {
      if (cloud->getCloudImage().isNull()) {
        painter.setCompositionMode(QPainter::CompositionMode_Screen);
        painter.drawPixmap(0, 0, cloud->getCanvasPm());
      } else {
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(cloud->getCloudImageBBox(), cloud->getCloudImage());
      }
    }
  }

  painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
  Cloud* cloud = p->getCurrentCloud();
  if (cloud && !cloud->getBBox().isEmpty()) {
    if (p->getMode() == DrawMode && cloud->isVisible()) {
      painter.setCompositionMode(QPainter::CompositionMode_Screen);
      painter.drawPixmap(0, 0, cloud->getCanvasPm());
    }
    // draw cloud bbox
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    if (cloud->isCloudDone())
      painter.setPen(QPen(Qt::green, 1.0 / scale));
    else
      painter.setPen(QPen(Qt::yellow, 1.0 / scale));
    painter.setBrush(Qt::NoBrush);
    QRectF cloudBBox = cloud->getBBox();
    painter.drawRect(cloudBBox);
  }

  // multiply background
  painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
  if (!Sky::instance()->getLayoutImage().isNull() &&
      p->getDouble(LayoutImgOpacity) > 0.0) {
    QSize layoutSize = Sky::instance()->getLayoutImage().size();
    QRect layoutRectOnViewer(
        0, 0, sceneRect.height() * layoutSize.width() / layoutSize.height(),
        sceneRect.height());
    layoutRectOnViewer.moveCenter(sceneRect.center());

    painter.setCompositionMode(QPainter::CompositionMode_Multiply);
    painter.setOpacity(p->getDouble(LayoutImgOpacity));
    painter.drawPixmap(layoutRectOnViewer, Sky::instance()->getLayoutImage());
    // restore the painter state
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setOpacity(1.0);
  }

  // draw sun
  painter.setPen(Qt::NoPen);
  if (m_activeObject == Sun)
    painter.setBrush(Qt::yellow);
  else
    painter.setBrush(Qt::white);

  bool useUnique = p->getBool(SunUseUniqueAngles);

  QRectF sunRect = Sky::instance()->getSunRectOnViewer(useUnique);
  if (!sunRect.isNull()) painter.drawEllipse(sunRect);

  // When using the unique sun direction, draw global sun with dotted line
  // 個別光源使用中は、グローバル光源位置を点線で表示
  if (useUnique) {
    painter.setPen(QPen(Qt::white, 1.0 / scale, Qt::DashLine));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(Sky::instance()->getSunRectOnViewer());
  }

  // draw eyeLevel
  int eyeLevelPos = (int)Sky::instance()->getEyeLevelPosOnViewer();
  if (m_activeObject == EyeLevel)
    painter.setPen(QPen(Qt::yellow, 3.0 / scale));
  else
    painter.setPen(QPen(Qt::yellow, 1.0 / scale, Qt::DashLine));
  painter.drawLine(0, eyeLevelPos, sceneRect.width(), eyeLevelPos);

  // if cloud rendering is not finished, call paintEvent again to advance the
  // rendering step
  if (p->getMode() == PreviewMode && cloud->isCloudProcessing())
    QTimer::singleShot(10, this, SLOT(update()));
}

void MyViewer::resizeEvent(QResizeEvent* e) {
  QSizeF offset = (e->size() - e->oldSize()) / 2.0;
  m_transform *= QTransform::fromTranslate(offset.width(), offset.height());
}

void MyViewer::onMove(const QPointF& pos) {
  QPointF mappedPos = m_transform.inverted().map(pos);

  ActiveObject oldObj = m_activeObject;
  MyParams* p         = MyParams::instance();
  bool useUniqueSun   = p->getBool(SunUseUniqueAngles);

  if (m_isSpaceKeyHeld)
    m_activeObject = SpaceKeyPan;
  else if (3.0 >
           std::abs(mappedPos.y() - Sky::instance()->getEyeLevelPosOnViewer()))
    m_activeObject = EyeLevel;
  else if (!Sky::instance()->getSunRectOnViewer(useUniqueSun).isNull() &&
           Sky::instance()
               ->getSunRectOnViewer(useUniqueSun)
               .adjusted(-3, -3, 3, 3)
               .contains(mappedPos))
    m_activeObject = Sun;
  else if (!p->getCurrentCloud()->isVisible())
    m_activeObject = Invalid;
  else if (p->getMode() == PreviewMode &&
           p->getCurrentCloud()->getBBox().contains(mappedPos.toPoint()))
    m_activeObject = CurrentCloud;
  else if (m_isCtrlKeyHeld)
    m_activeObject = Erase;
  else
    m_activeObject = Draw;

  if (oldObj != m_activeObject) {
    if (m_activeObject == EyeLevel)
      setCursor(Qt::SplitVCursor);
    else if (m_activeObject == Sun || m_activeObject == CurrentCloud)
      setCursor(Qt::SizeAllCursor);
    else if (m_activeObject == SpaceKeyPan)
      setCursor(Qt::OpenHandCursor);
    else if (m_activeObject == Erase)
      setCursor(QCursor(QPixmap(":Resources/eraser.png"), 7, 21));
    else if (m_activeObject == Invalid)
      setCursor(Qt::ForbiddenCursor);
    else
      setCursor(Qt::ArrowCursor);
    update();
  }
  // ズーム位置の中心に使う
  m_pointer.viewerPos = pos;
}

void MyViewer::onPress(const QPointF& pos) {
  Cloud* cloud = MyParams::instance()->getCurrentCloud();
  if (!cloud) return;

  // this is to enable drawing just after space-key panning
  // Spaceキーを押し、離し、カーソルを動かさずに左ドラッグで描画を開始した場合に
  // activeObjectをDrawに更新するため
  if (m_activeObject == None) onMove(m_pointer.viewerPos);

  // switch cursor with space-key panning
  // Spaceキーでパンのとき、カーソルを変更
  else if (m_activeObject == SpaceKeyPan) {
    setCursor(Qt::ClosedHandCursor);
    m_pointer.lastPos = pos;
    return;
  }

  else if (m_activeObject == CurrentCloud)
    cloud->onStartMove();

  else if (m_activeObject == Invalid) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Current cloud layer is not visible."));
    m_pointer.state = Pointer::NoDraw;
    return;
  }

  m_pointer.lastPos      = m_transform.inverted().map(pos);
  m_pointer.dragStartPos = m_pointer.lastPos;

  // In Drawing mode 描画の場合
  if (MyParams::instance()->getMode() == DrawMode &&
      (m_activeObject == Draw || m_activeObject == Erase)) {
    DrawUndo* undo =
        new DrawUndo(cloud, m_pointer.lastPos, m_activeObject == Draw);
    UndoManager::instance()->setTmpCommand(undo);
    if (m_activeObject == Draw) cloud->addPos(m_pointer.lastPos, 5.0f);
  }
}

void MyViewer::onDrag(const QPointF& pos, const qreal pressure) {
  Cloud* cloud = MyParams::instance()->getCurrentCloud();
  if (!cloud || m_activeObject == Invalid) return;

  QPointF mappedPos = m_transform.inverted().map(pos);

  // In Drawing mode 描画の場合
  if (MyParams::instance()->getMode() == DrawMode &&
      (m_activeObject == Draw || m_activeObject == Erase)) {
    QPainter painter(&cloud->getCanvasPm());
    QPen pen(
        (m_activeObject == Draw) ? QColor(100, 50, 50) : QColor(50, 100, 50),
        10.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    pen.setWidthF(pressure * 9.0 + 1.0);
    painter.setPen(pen);
    painter.drawLine(m_pointer.lastPos, mappedPos);
    painter.end();
    if (m_activeObject == Draw) cloud->addPos(mappedPos, pen.widthF() / 2.0f);

    DrawUndo* undo =
        dynamic_cast<DrawUndo*>(UndoManager::instance()->tmpCommand());
    if (undo) undo->addPos(mappedPos);
  } else if (m_activeObject == EyeLevel) {
    float d        = mappedPos.y() - m_pointer.lastPos.y();
    float eyeLevel = Sky::instance()->getEyeLevelPosOnViewer();
    Sky::instance()->setEyeLevelPosOnViewer(eyeLevel + d);
  } else if (m_activeObject == Sun) {
    bool useUnique = MyParams::instance()->getBool(SunUseUniqueAngles);
    QPointF d      = mappedPos - m_pointer.lastPos;
    Sky::instance()->moveSunPosOnViewer(d, useUnique);
  } else if (m_activeObject == CurrentCloud) {
    QPoint d = mappedPos.toPoint() - m_pointer.lastPos.toPoint();
    cloud->move(d);
  } else if (m_activeObject == SpaceKeyPan) {
    QPointF d = pos - m_pointer.lastPos;
    m_transform *= QTransform::fromTranslate(d.x(), d.y());
    m_pointer.lastPos = pos;
    update();
    return;
  }

  m_pointer.lastPos = mappedPos;
  update();
}

void MyViewer::onRelease() {
  Cloud* cloud = MyParams::instance()->getCurrentCloud();
  if (!cloud || m_activeObject == Invalid) return;
  if (MyParams::instance()->getMode() == DrawMode &&
      (m_activeObject == Draw || m_activeObject == Erase)) {
    if (m_activeObject == Draw) cloud->autoFill();
    DrawUndo* undo =
        dynamic_cast<DrawUndo*>(UndoManager::instance()->tmpCommand());
    if (undo) {
      if (m_activeObject == Erase)
        cloud->removeErasedStroke(undo->getStrokeBBox());
      undo->onEndProcess();
      UndoManager::instance()->stack()->push(undo);
      UndoManager::instance()->releaseTmpCommand();
    }
    update();
  } else if (m_activeObject == CurrentCloud) {
    QPoint d = m_pointer.lastPos.toPoint() - m_pointer.dragStartPos.toPoint();
    cloud->onEndMove(d);
  } else if (m_activeObject == SpaceKeyPan) {
    setCursor(Qt::OpenHandCursor);
  }
}

void MyViewer::onMiddlePress(const QPointF& pos) { m_pointer.lastPos = pos; }

// panning
void MyViewer::onMiddleDrag(const QPointF& pos) {
  QPointF d = pos - m_pointer.lastPos;
  m_transform *= QTransform::fromTranslate(d.x(), d.y());
  m_pointer.lastPos = pos;
  update();
}

void MyViewer::mousePressEvent(QMouseEvent* e) {
  Cloud* cloud = MyParams::instance()->getCurrentCloud();
  if (!cloud) return;

  if (m_pointer.state == Pointer::NoDraw) {
    m_pointer.button = e->button();
    m_pointer.state  = Pointer::Mouse;
    if (e->button() == Qt::MiddleButton)
      onMiddlePress(e->localPos());
    else
      onPress(e->localPos());
  }
}

void MyViewer::mouseMoveEvent(QMouseEvent* e) {
  if (e->buttons() == Qt::NoButton) {
    onMove(e->localPos());
    return;
  }

  if (m_pointer.state != Pointer::Mouse) return;

  if (m_pointer.button == Qt::MiddleButton)
    onMiddleDrag(e->localPos());
  else
    onDrag(e->localPos());
}

void MyViewer::mouseReleaseEvent(QMouseEvent* e) {
  if (e->button() != m_pointer.button) return;

  if (m_pointer.state == Pointer::Mouse) {
    m_pointer.state = Pointer::NoDraw;
    if (m_pointer.button == Qt::MiddleButton) {
    }  // do nothing
    else
      onRelease();
    m_pointer.button = Qt::NoButton;
  }
}

void MyViewer::tabletEvent(QTabletEvent* e) {
  switch (e->type()) {
  case QEvent::TabletPress:
    if (!rect().contains(e->pos())) return;
    if (m_pointer.state != Pointer::Tablet) {
      m_pointer.button = e->button();
      m_pointer.state  = Pointer::Tablet;
      if (e->button() == Qt::MiddleButton)
        onMiddlePress(e->posF());
      else
        onPress(e->posF());
    }
    break;
  case QEvent::TabletMove: {
    if (m_pointer.state != Pointer::Tablet) return;
    // discard too frequent move events
    static QTime clock;
    if (clock.isValid() && clock.elapsed() < 10) {
      e->accept();
      return;
    }
    clock.start();

    if (m_pointer.button == Qt::MiddleButton)
      onMiddleDrag(e->posF());
    else
      onDrag(e->posF(), e->pressure());
  } break;
  case QEvent::TabletRelease: {
    if (e->button() != m_pointer.button) return;

    if (m_pointer.state == Pointer::Tablet) {
      m_pointer.state = Pointer::NoDraw;
      if (m_pointer.button == Qt::MiddleButton) {
      }  // do nothing
      else
        onRelease();
      m_pointer.button = Qt::NoButton;
    }
  } break;
  default:
    break;
  }
  e->accept();
}

void MyViewer::keyPressEvent(QKeyEvent* e) {
  switch (e->key()) {
  case Qt::Key_Space:
    if (!e->isAutoRepeat()) setSpaceKeyHeld(true);
    break;
  case Qt::Key_Control:
    if (!e->isAutoRepeat()) setCtrlKeyHeld(true);
    break;
  // zoom in / out
  case Qt::Key_Plus:
    onZoom(true);
    break;
  case Qt::Key_Minus:
    onZoom(false);
    break;
  case Qt::Key_0:
    fitZoomToScene();
    break;
  }
  QWidget::keyPressEvent(e);
}

void MyViewer::keyReleaseEvent(QKeyEvent* e) {
  switch (e->key()) {
  case Qt::Key_Space:
    if (!e->isAutoRepeat()) setSpaceKeyHeld(false);
    break;
  case Qt::Key_Control:
    if (!e->isAutoRepeat()) setCtrlKeyHeld(false);
    break;
  }
  QWidget::keyReleaseEvent(e);
}

void MyViewer::wheelEvent(QWheelEvent* event) {
  int delta = 0;
  switch (event->source()) {
  case Qt::MouseEventNotSynthesized: {
    if (event->modifiers() & Qt::AltModifier)
      delta = event->angleDelta().x();
    else
      delta = event->angleDelta().y();
    break;
  }
  case Qt::MouseEventSynthesizedBySystem: {
    QPoint numPixels  = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    if (!numPixels.isNull()) {
      delta = event->pixelDelta().y();
    } else if (!numDegrees.isNull()) {
      QPoint numSteps = numDegrees / 15;
      delta           = numSteps.y();
    }
    break;
  }

  default:  // Qt::MouseEventSynthesizedByQt,
            // Qt::MouseEventSynthesizedByApplication
    {
      std::cout << "not supported event: Qt::MouseEventSynthesizedByQt, "
                   "Qt::MouseEventSynthesizedByApplication"
                << std::endl;
      break;
    }

  }  // end switch

  if (std::abs(delta) > 0) {
    onWheelZoom(std::exp(0.001 * delta));
  }
  event->accept();
}

void MyViewer::showEvent(QShowEvent*) { fitZoomToScene(); }

void MyViewer::onEnterPressed() {
  MyParams* p  = MyParams::instance();
  Cloud* cloud = p->getCurrentCloud();
  if (!cloud || !cloud->isVisible()) return;
  cloud->triggerRender();
  p->setMode(PreviewMode);
  update();
}

void MyViewer::onZoom(bool zoomIn) {
  static QList<qreal> zoomFactors = {0.125, 0.25, 0.5, 1.0, 2.0, 4.0, 8.0};
  qreal scale                     = std::sqrt(m_transform.det());
  qreal newScale                  = scale;
  if (zoomIn) {
    if (scale < zoomFactors[0])
      newScale = zoomFactors[0];
    else if (scale >= zoomFactors[zoomFactors.size() - 2])
      newScale = zoomFactors.last();
    else {
      for (int i = 1; i < zoomFactors.size(); i++) {
        if (scale < zoomFactors.at(i)) {
          newScale = zoomFactors.at(i);
          break;
        }
      }
    }
  } else {
    if (scale <= zoomFactors[1])
      newScale = zoomFactors[0];
    else if (scale > zoomFactors.last())
      newScale = zoomFactors.last();
    else {
      for (int i = zoomFactors.size() - 2; i >= 0; i--) {
        if (scale > zoomFactors.at(i)) {
          newScale = zoomFactors.at(i);
          break;
        }
      }
    }
  }

  QTransform matrix;
  matrix.translate(m_pointer.viewerPos.x(), m_pointer.viewerPos.y())
      .scale(newScale / scale, newScale / scale)
      .translate(-m_pointer.viewerPos.x(), -m_pointer.viewerPos.y());
  m_transform *= matrix;
  update();
  emit zoomChanged();
}

void MyViewer::onWheelZoom(double factor) {
  if (factor == 1.0) return;
  qreal scale = std::sqrt(m_transform.det());

  if ((scale < 8 || factor < 1) && (scale > 0.125 || factor > 1)) {
    QTransform matrix;
    matrix.translate(m_pointer.viewerPos.x(), m_pointer.viewerPos.y())
        .scale(factor, factor)
        .translate(-m_pointer.viewerPos.x(), -m_pointer.viewerPos.y());
    m_transform *= matrix;
    update();
    emit zoomChanged();
  }
}

void MyViewer::setSpaceKeyHeld(bool isHeld) {
  m_isSpaceKeyHeld = isHeld;
  if (m_isSpaceKeyHeld && m_pointer.button == Qt::NoButton &&
      m_pointer.state == Pointer::NoDraw) {
    m_activeObject = SpaceKeyPan;
    setCursor(Qt::OpenHandCursor);
  } else if (!m_isSpaceKeyHeld && m_activeObject == SpaceKeyPan) {
    m_activeObject = None;
    setCursor(Qt::ArrowCursor);
  }
}

void MyViewer::setCtrlKeyHeld(bool isHeld) {
  m_isCtrlKeyHeld = isHeld;
  if (m_isCtrlKeyHeld && m_pointer.button == Qt::NoButton &&
      m_pointer.state == Pointer::NoDraw && m_activeObject == Draw) {
    m_activeObject = Erase;
    setCursor(QCursor(QPixmap(":Resources/eraser.png"), 7, 21));
  } else if (!m_isCtrlKeyHeld /* && m_activeObject == SpaceKeyPan*/) {
    m_activeObject = Draw;
    setCursor(Qt::ArrowCursor);
  }
}

void MyViewer::fitZoomToScene() {
  QSize sceneSize = MyParams::instance()->getSceneSize();
  double factor   = std::min((double)width() / (double)sceneSize.width(),
                           (double)height() / (double)sceneSize.height());

  QPointF offset(((double)width() - (double)sceneSize.width() * factor) / 2,
                 ((double)height() - (double)sceneSize.height() * factor) / 2);

  m_transform =
      QTransform::fromTranslate(offset.x(), offset.y()).scale(factor, factor);
  update();
  emit zoomChanged();
}

void MyViewer::drawSkyGrid(QPainter& painter) {
  QSize sceneSize = MyParams::instance()->getSceneSize();
  QRectF bbox(QPoint(0, 0), sceneSize);
  painter.setClipping(true);
  painter.setClipRect(bbox);

  if (Sky::instance()->getEyeLevelPosOnViewer() < bbox.height())
    bbox.setBottom(Sky::instance()->getEyeLevelPosOnViewer());
  if (!bbox.isValid()) return;

  painter.setPen(Qt::white);
  painter.setBrush(Qt::NoBrush);
  int sep         = 9;  // per 10 degrees
  float angleStep = M_PI / (2.0 * (float)sep);
  std::array<QPointF, 37> points;  // sep * 4 + 1
  float elev = angleStep;

  for (int i = 0; i < sep; i++, elev += angleStep) {
    float azimuth = -M_PI;
    int start = -1, end = -1;
    for (int j = 0; j < sep * 4 + 1; j++, azimuth += angleStep) {
      points[j] = Sky::angleToPos(QPointF(azimuth, elev), sceneSize);
      if (bbox.contains(points[j]) && start == -1) start = j;
    }
    if (start != -1) {
      for (int j = sep * 4; j >= 0; j--) {
        if (bbox.contains(points[j])) {
          end = j + 1;
          break;
        }
      }
    }
    if (start != -1 && end != -1) {
      start = std::max(0, start - 1);
      end   = std::min(sep * 4 + 1, end + 1);
      painter.drawPolyline(&points[start], end - start);
    }
  }

  painter.setClipping(false);
}
