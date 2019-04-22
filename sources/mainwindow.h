#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDockWidget>

class MyViewer;

class CloudControl : public QDockWidget {
  Q_OBJECT
public:
  CloudControl(QWidget *parent       = Q_NULLPTR,
               Qt::WindowFlags flags = Qt::WindowFlags());
};

class SkyControl : public QDockWidget {
  Q_OBJECT
public:
  SkyControl(QWidget *parent       = Q_NULLPTR,
             Qt::WindowFlags flags = Qt::WindowFlags());
};

class CloudLayerViewDock : public QDockWidget {
  Q_OBJECT
public:
  CloudLayerViewDock(QWidget *parent       = Q_NULLPTR,
                     Qt::WindowFlags flags = Qt::WindowFlags());
};

class MainWindow : public QMainWindow {
  Q_OBJECT
  MyViewer *m_viewer;
  SkyControl *m_skyControl;
  CloudLayerViewDock *m_layerView;

public:
  MainWindow(QWidget *parent       = Q_NULLPTR,
             Qt::WindowFlags flags = Qt::WindowFlags());

protected:
  void keyPressEvent(QKeyEvent *) override;
  void showEvent(QShowEvent *) override;
  void closeEvent(QCloseEvent *) override;
protected slots:
  void updateTitlebar();
};

#endif