#pragma once
#ifndef ABOUTPOPUP_H
#define ABOUTPOPUP_H

#include <QDialog>

class AboutPopup : public QDialog {
  Q_OBJECT

public:
  AboutPopup(QWidget *parent   = Q_NULLPTR,
             Qt::WindowFlags f = Qt::WindowFlags());

protected:
  void paintEvent(QPaintEvent *) override;
};

#endif
