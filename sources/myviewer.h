#pragma once

#ifndef MYVIEWER_H
#define MYVIEWER_H

#include <cmath>
#include <QWidget>
#include <QTransform>

class MyViewer : public QWidget {
  Q_OBJECT

  QTransform m_transform;

  enum ActiveObject {
    None = 0,
    Draw,
    Erase,
    EyeLevel,
    Sun,
    CurrentCloud,
    SpaceKeyPan,
    Invalid
  } m_activeObject = None;

  struct Pointer {
    Qt::MouseButton button = Qt::NoButton;
    enum State { NoDraw = 0, Mouse, Tablet } state = NoDraw;
    QPointF lastPos;

    // mouse position without coordinate transformation.
    // 座標変換無しのマウス位置
    QPointF viewerPos;

    QPointF dragStartPos;
  } m_pointer;

  bool m_isSpaceKeyHeld = false;
  bool m_isCtrlKeyHeld  = false;

  void onMove(const QPointF &pos);
  void onPress(const QPointF &pos);
  void onDrag(const QPointF &pos, const qreal pressure = 1.0);
  void onRelease();

  void onMiddlePress(const QPointF &pos);
  void onMiddleDrag(const QPointF &pos);

  void drawSkyGrid(QPainter &);

  void setSpaceKeyHeld(bool isHeld);
  void setCtrlKeyHeld(bool isHeld);

public:
  MyViewer(QWidget *parent = nullptr);
  void onEnterPressed();
  void onZoom(bool zoomIn);
  void onWheelZoom(double factor);
  double zoomFactor() { return std::sqrt(m_transform.det()); }

protected:
  void paintEvent(QPaintEvent *) override;
  void resizeEvent(QResizeEvent *) override;
  void mousePressEvent(QMouseEvent *) override;
  void mouseMoveEvent(QMouseEvent *) override;
  void mouseReleaseEvent(QMouseEvent *) override;
  void tabletEvent(QTabletEvent *) override;

  void keyPressEvent(QKeyEvent *) override;
  void keyReleaseEvent(QKeyEvent *) override;

  void wheelEvent(QWheelEvent *) override;
  void showEvent(QShowEvent *) override;

  void enterEvent(QEvent *event) override { setFocus(); }

public slots:
  void fitZoomToScene();
signals:
  void zoomChanged();
};

#endif
