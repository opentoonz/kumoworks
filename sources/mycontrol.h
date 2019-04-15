#pragma once

#ifndef MYCONTROL_H
#define MYCONTROL_H

#include <QWidget>
#include <QCheckBox>
#include <QPushButton>

#include "myparams.h"

class QLineEdit;
class QSlider;
class QPushButton;
class QLabel;

class PlusMinusButton : public QPushButton {
  Q_OBJECT
public:
  PlusMinusButton(bool isPlus, QWidget* parent = nullptr);
};

//========================================

class MySlider : public QWidget {
  Q_OBJECT

  QLineEdit* m_edit;
  QSlider* m_slider;
  int m_decimals;

public:
  MySlider(QWidget* parent, double value, double min, double max,
           int decimals = 1);

  double getValue();
protected slots:
  void onSliderChanged(int);
  void onSliderReleased();
  void onLineEdited();
public slots:
  void setValue(double value);
signals:
  void valueChanged(bool);
};

//========================================

class MyColorEdit : public QWidget {
  Q_OBJECT

  QColor m_color;

public:
  MyColorEdit(QWidget* parent, QColor val);

  void setValue(QColor& color) {
    m_color = color;
    update();
  }
  QColor getValue() { return m_color; }

protected:
  void paintEvent(QPaintEvent*) override;
  void mousePressEvent(QMouseEvent*) override;

signals:
  void valueChanged(bool);
};

//========================================

class MyCheckBox : public QCheckBox {
  Q_OBJECT
public:
  MyCheckBox(QWidget* parent = nullptr, bool value = false);
protected slots:
  void onClicked() { emit valueChanged(false); }
signals:
  void valueChanged(bool);
};

//========================================

class MyPathLabel : public QWidget {
  Q_OBJECT
protected:
  QString m_path;
  QLabel* m_pathLabel;
  QPushButton* m_loadButton;

public:
  MyPathLabel(QString path, QWidget* parent = nullptr);
  QString getPath() const;
  virtual void setPath(const QString path);
protected slots:
  virtual void onLoadButtonClicked() = 0;
signals:
  void valueChanged(bool);
};

class MyImgPathLabel : public MyPathLabel {
  Q_OBJECT
  QString m_filterString;

public:
  MyImgPathLabel(QString path, QWidget* parent = nullptr)
      : MyPathLabel(path, parent) {}
protected slots:
  void onLoadButtonClicked() override;
};

class MyDirPathLabel : public MyPathLabel {
  Q_OBJECT
public:
  MyDirPathLabel(QString path, QWidget* parent = nullptr)
      : MyPathLabel(path, parent) {}
  void setPath(const QString path) override;
protected slots:
  void onLoadButtonClicked() override;
};

//========================================

class MyControl : public QWidget {
  Q_OBJECT

  QMap<ParamId, QWidget*> m_idControlMap;
  QString getCategoryLabelName(ParamCategory);

public:
  MyControl(QList<ParamCategory>, QWidget* parent = 0);

protected slots:
  void onControlChanged(bool isDragging);
  // sync UI when the parameter is externally changed
  // 外部からパラメータが変化したとき、UIを同期する
  void onParameterChanged(ParamId);
};

#endif
