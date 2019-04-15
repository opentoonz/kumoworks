#pragma once
#ifndef RENDERWORKERTHREAD_H
#define RENDERWORKERTHREAD_H

#include "myparams.h"
#include <QThread>
#include <QList>

class Cloud;

class RenderWorkerThread : public QThread {
  Q_OBJECT
  bool m_isCanceled = false;
  RenderOptions m_ro;
  QImage m_bgImage;
  void precomputeBg();

public:
  void run() override;

  enum RenderResult { Finished = 0, Failed, Canceled };

  RenderWorkerThread(RenderOptions &ro, QObject *parent = nullptr);
  void cancel() { m_isCanceled = true; }
signals:
  // to advance a progressbar in the render popup
  void cloudRendered();
  void renderFinished(RenderWorkerThread::RenderResult);
};

Q_DECLARE_METATYPE(RenderWorkerThread::RenderResult)

#endif