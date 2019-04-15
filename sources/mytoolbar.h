#pragma once

#ifndef MYTOOLBAR_H
#define MYTOOLBAR_H

#include "myparams.h"
#include <QToolBar>

class QButtonGroup;
class QLineEdit;
class QComboBox;
class QPushButton;
class QActionGroup;

class MyToolBar : public QToolBar {
  Q_OBJECT

  QButtonGroup *m_modeButtonGroup;
  QLineEdit *m_widthEdit, *m_heightEdit;
  QComboBox *m_previewQualityCombo;
  QPushButton *m_autoPrevBtn;

  QActionGroup *m_languagesGroup;

  void createLanguageActions(QMenu *);

public:
  MyToolBar(QWidget *parent = nullptr);

protected slots:
  void onModeButtonClicked(int id);
  void onModeChanged(int mode);
  void onRenderButtonClicked();
  void onSceneSizeEdited();
  void syncSceneSizeField();
  void onPreviewQualityChanged();
  void onParameterChanged(ParamId);
  void onAutoPrevButtonClicked(bool);
  void onLanguageTriggered(QAction *);
  void onAboutTriggered();
};

#endif
