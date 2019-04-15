#pragma once
#ifndef MYRENDERPOPUP_H
#define MYRENDERPOPUP_H

#include "myparams.h"
#include "mycontrol.h"
#include "renderworkerthread.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QCheckBox;
class QProgressBar;
class ProgressView;

class RenderPopup : public QDialog {
  Q_OBJECT

  QComboBox * m_targetCombo, * m_regionCombo, 
    * m_bgCombo, *m_renderQualityCombo;
  MyDirPathLabel * m_dirLabel;
  QLineEdit * m_fileNameEdit;
  QCheckBox * m_renderHiddenLayers;

  QProgressBar* m_layerProgressBar;
  ProgressView* m_cloudProgressView;
  RenderWorkerThread* m_renderWorkerThread = nullptr;

  QPushButton * m_renderButton, * m_cancelButton;

public:
  RenderPopup(QWidget* parent = nullptr);
protected:
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
protected slots:
  void onRenderButtonClicked();
  void onTargetChanged(int);
  void advanceProgressBar();
  void onRenderFinished(RenderWorkerThread::RenderResult);
  void onCancelButtonClicked();
};

#endif
