#pragma once
#ifndef MYGRADIENTCONTROL_H
#define MYGRADIENTCONTROL_H

#include "colorgradientparam.h"

#include <QWidget>
#include <QLinearGradient>

class MyColorEdit;
class QPushButton;
class QComboBox;
class PlusMinusButton;

//=============================================================================
// SpectrumBar
//-----------------------------------------------------------------------------

class GradientBar final : public QWidget {
  Q_OBJECT

  int m_x0;
  int m_currentKeyIndex;

  ColorGradient m_gradient;

public:
  GradientBar(QWidget *parent = 0);

  ~GradientBar();

  int getCurrentKeyIndex() { return m_currentKeyIndex; }
  void setCurrentKeyIndex(int index);

  int getCurrentPos();
  QColor getCurrentColor();

  ColorGradient &getGradient() { return m_gradient; }
  void setGradient(ColorGradient &gradient) {
    m_gradient = gradient;
    if (m_currentKeyIndex >= m_gradient.stops().count())
      setCurrentKeyIndex(getMaxPosKeyIndex());
    update();
  }

public slots:
  void setCurrentPos(int pos, bool isDragging);
  void setCurrentColor(const QColor &color);
  void addKeyAt(int pos);

signals:
  void currentPosChanged(bool isDragging);
  void currentKeyChanged();
  void currentKeyAdded(int);
  void currentKeyRemoved(int);

protected:
  double posToSpectrumValue(int pos);
  int spectrumValueToPos(double spectrumValue);

  void paintEvent(QPaintEvent *e) override;
  void mousePressEvent(QMouseEvent *e) override;
  void mouseMoveEvent(QMouseEvent *e) override;

  void mouseReleaseEvent(QMouseEvent *e) override;

  int getMaxPosKeyIndex();
  int getMinPosKeyIndex();
  int getNearPosKeyIndex(int pos);
};

//=============================================================================

class GradientPresetControl final : public QWidget {
  Q_OBJECT
  QComboBox *m_presetCombo;
  PlusMinusButton *m_addPresetBtn;

  void loadPresetItems();

public:
  GradientPresetControl(QWidget *parent);
  void addPreset(ColorGradient gradient);
  QPushButton *addButton() { return (QPushButton *)m_addPresetBtn; }

protected:
  void resizeEvent(QResizeEvent *e);
protected slots:
  void onPresetComboActivated(int);
  void onRemovePresetBtnClicked();
signals:
  void presetSelected(ColorGradient grad);
};

//=============================================================================
// SpectrumField
//-----------------------------------------------------------------------------

class GradientControl final : public QWidget {
  Q_OBJECT

  int m_margin;
  int m_spacing;

  MyColorEdit *m_colorEdit;
  GradientBar *m_gradientBar;
  GradientPresetControl *m_presetControl;

public:
  GradientControl(QWidget *parent = 0);

  ~GradientControl();

  ColorGradient &getGradient() { return m_gradientBar->getGradient(); }
  void setSpectrum(ColorGradient &gradient);

  int getCurrentKeyIndex() { return m_gradientBar->getCurrentKeyIndex(); }
  void setCurrentKeyIndex(int index);

protected slots:
  void onCurrentPosChanged(bool isDragging);
  void onCurrentKeyChanged();
  void onColorChanged();
  void onKeyAddedOrRemoved() { emit valueChanged(false); }

  void onAddPresetBtnPressed();
  void onPresetSelected(ColorGradient grad);

protected:
  void paintEvent(QPaintEvent *e) override;

signals:
  void valueChanged(bool);
};

#endif