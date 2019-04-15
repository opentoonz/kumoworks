#pragma once
#ifndef CLOUDPRESETCONTROL_H
#define CLOUDPRESETCONTROL_H

#include <QWidget>

class QComboBox;
class PlusMinusButton;

class CloudPresetControl : public QWidget {
  Q_OBJECT

  QComboBox* m_presetListCombo;
  PlusMinusButton* m_removeButton;

  void refreshPresetListCombo();

public:
  CloudPresetControl(QWidget* parent = nullptr);
protected slots:
  void onAddButtonClicked();
  void onRemoveButtonClicked();
  void onPresetSelected(const QString &text);
};


#endif
