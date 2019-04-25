#define _USE_MATH_DEFINES
#include "mainwindow.h"
#include "myviewer.h"
#include "mycontrol.h"
#include "mytoolbar.h"
#include "cloudlayerview.h"
#include "undomanager.h"
#include "cloud.h"
#include "pathutils.h"

#include <iostream>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSettings>

using namespace PathUtils;

CloudControl::CloudControl(QWidget* parent, Qt::WindowFlags flags)
    : QDockWidget(tr("Cloud Settings"), parent, flags) {
  setObjectName("CloudControl");  // for saveState()
  QWidget* widget  = new QWidget(this);
  QHBoxLayout* lay = new QHBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(0);
  {
    lay->addWidget(new MyControl({Shape1}), 0, Qt::AlignTop);
    lay->addWidget(new MyControl({Shape2}), 0, Qt::AlignTop);
    lay->addWidget(new MyControl({Noise}), 0, Qt::AlignTop);
    lay->addWidget(new MyControl({Render}), 0, Qt::AlignTop);
  }
  widget->setLayout(lay);
  setWidget(widget);
}

SkyControl::SkyControl(QWidget* parent, Qt::WindowFlags flags)
    : QDockWidget(tr("Sky Settings"), parent, flags) {
  setObjectName("SkyControl");  // for saveState()
  QWidget* widget  = new QWidget(this);
  QVBoxLayout* lay = new QVBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(0);
  {
    lay->addWidget(new MyControl({Background, Camera, Sun, SkyRender}), 0,
                   Qt::AlignTop);
  }
  widget->setLayout(lay);
  setWidget(widget);
}

CloudLayerViewDock::CloudLayerViewDock(QWidget* parent, Qt::WindowFlags flags)
    : QDockWidget(tr("Cloud Layers"), parent, flags) {
  setObjectName("CloudLayerViewDock");  // for saveState()
  setWidget(new CloudLayerView());
}

MainWindow::MainWindow(QWidget* parent, Qt::WindowFlags flags)
    : QMainWindow(parent, flags) {
  setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);

  m_viewer                   = new MyViewer(this);
  m_skyControl               = new SkyControl(this);
  CloudControl* cloudControl = new CloudControl(this);
  m_layerView                = new CloudLayerViewDock(this);
  MyToolBar* toolbar         = new MyToolBar(this);

  setCentralWidget(m_viewer);
  addDockWidget(Qt::LeftDockWidgetArea, m_skyControl);
  addDockWidget(Qt::BottomDockWidgetArea, cloudControl);
  addDockWidget(Qt::LeftDockWidgetArea, m_layerView, Qt::Vertical);
  addToolBar(toolbar);

  MyParams::instance()->setMainWindow(this);

  connect(m_viewer, SIGNAL(zoomChanged()), this, SLOT(updateTitlebar()));
  connect(MyParams::instance(), SIGNAL(sceneFilePathChanged()), this,
          SLOT(updateTitlebar()));
}

void MainWindow::keyPressEvent(QKeyEvent* e) {
  switch (e->key()) {
  case Qt::Key_Return:
    // std::cout << "return pressed" << std::endl;
    m_viewer->onEnterPressed();
    break;
  case Qt::Key_Enter:
    m_viewer->onEnterPressed();
    break;
  case Qt::Key_Escape:
    // std::cout << "escape pressed" << std::endl;
    MyParams::instance()->getCurrentCloud()->suspendRender();
    break;
  default:
    if (e->matches(QKeySequence::Undo)) {
      if (UndoManager::instance()->stack()->canUndo())
        UndoManager::instance()->stack()->undo();
      else
        QMessageBox::warning(this, tr("Warning"), tr("Nothing to Undo."));
    } else if (e->matches(QKeySequence::Redo)) {
      if (UndoManager::instance()->stack()->canRedo())
        UndoManager::instance()->stack()->redo();
      else
        QMessageBox::warning(this, tr("Warning"), tr("Nothing to Redo."));
    }
    break;
  }
  QMainWindow::keyPressEvent(e);
}

void MainWindow::updateTitlebar() {
  QString separatorStr = "  |  ";
  QString sceneName    = MyParams::instance()->sceneName();
  QString zoomFactor   = QString::number(m_viewer->zoomFactor() * 100) + "%";

  QString title = qApp->applicationName() + separatorStr + sceneName +
                  separatorStr + zoomFactor;

  setWindowTitle(title);
}

void MainWindow::showEvent(QShowEvent*) {
  QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);
  // if the geometry infomation is in setting, restore it
  if (settings.childGroups().contains("MainWindow")) {
    settings.beginGroup("MainWindow");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
    settings.endGroup();
  } else {
    // expand the layer view
    resizeDocks({m_skyControl, m_layerView}, {0, 1}, Qt::Vertical);
  }
}

void MainWindow::closeEvent(QCloseEvent*) {
  QSettings settings(getDefaultSettingsPath(), QSettings::IniFormat);
  settings.beginGroup("MainWindow");
  settings.setValue("geometry", saveGeometry());
  settings.setValue("windowState", saveState());
  settings.endGroup();
}