#pragma once

#ifndef MYUNDOMANAGER_H
#define MYUNDOMANAGER_H

#include "myparams.h"

#include <QObject>
#include <QUndoCommand>
#include <QRectF>
#include <QPixmap>

class QUndoGroup;
class QUndoStack;
class Cloud;

class UndoManager : public QObject {  // singleton
  Q_OBJECT

  QUndoGroup* m_undoGroup;
  // For now all undos will be stored in the same stack.
  // パラメータ変更は別のQUndoStackとするかも
  QUndoStack* m_drawUndoStack;

  QUndoCommand* m_tmpCommand;

  UndoManager();

public:
  static UndoManager* instance();

  QUndoStack* stack();

  QUndoCommand* tmpCommand() { return m_tmpCommand; }
  void setTmpCommand(QUndoCommand* command);
  void releaseTmpCommand();

  void notifyViewer() { emit updateViewer(); }

signals:
  void updateViewer();
};

//---------------------------

class DrawUndo : public QUndoCommand {
  enum Type { Draw, Erase } m_type;

public:
  DrawUndo(Cloud* cloud, QPointF pos, bool isDraw, QUndoCommand* parent = 0);

  void setPixmap(bool isUndo);

  bool m_isOnPush = true;

  // QUndoCommand interface
public:
  void undo() override;
  void redo() override;

  void addPos(const QPointF& pos);
  void onEndProcess();

  QRectF getStrokeBBox() { return m_bbox; }

private:
  QRectF m_bbox, m_beforeDrawBBox, m_afterDrawBBox;
  Cloud* m_cloud;
  QPixmap m_beforePm, m_afterPm;
};

//---------------------------
class ParameterUndo : public QUndoCommand {
public:
  ParameterUndo(Cloud* cloud, ParamId pId, QVariant& beforeVal,
                QUndoCommand* parent = nullptr);
  bool m_isOnPush = true;

public:
  void undo() override;
  void redo() override;

  void setAfterVal(const QVariant& val);

private:
  Cloud* m_cloud;
  ParamId m_pId;
  QVariant m_beforeVal, m_afterVal;
};

//---------------------------

class MoveCloudUndo : public QUndoCommand {
  Cloud* m_cloud;
  QPixmap m_cloudPm;

  struct CloudGeom {
    QRect cloudImageBBox;
    QRectF drawingBBox;
    QPoint offset;
  } m_before, m_after;

  QRect m_beforeBBox;
  bool m_bboxChanged;

public:
  MoveCloudUndo(Cloud* cloud, QUndoCommand* parent = nullptr);
  void setOffset(const QPoint& offset);
  void undo() override;
  void redo() override;

  void apply(CloudGeom&);
  void resetFlows();
};

#endif
