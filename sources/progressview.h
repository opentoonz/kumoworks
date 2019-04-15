#pragma once
#ifndef PROGRESSVIEW_H
#define PROGRESSVIEW_H

#include <QWidget>

class ProgressView : public QWidget {
  Q_OBJECT
public:
  ProgressView(QWidget* parent);
protected:
  void paintEvent(QPaintEvent* event) override;
};

#endif
