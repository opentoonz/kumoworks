#pragma once

#ifndef CLOUDLAYERVIEW_H
#define CLOUDLAYERVIEW_H

#include <QWidget>
#include <QListWidgetItem>
#include <QListWidget>
#include <QStyledItemDelegate>

class QListWidget;
class Cloud;

class CloudListWidgetItemDelegate : public QStyledItemDelegate {
public:
  CloudListWidgetItemDelegate(QObject* parent = nullptr)
    : QStyledItemDelegate(parent) {}

  void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index) const;

};

class CloudLayerViewItem : public QListWidgetItem {
  Cloud* m_cloud;
public:
  CloudLayerViewItem(Cloud*);
  void updateIcon();
  Cloud* cloud() const { return m_cloud; }
};

class CloudListWidget : public QListWidget {
  Q_OBJECT
public:
  CloudListWidget(QWidget* parent);
protected:
  void dropEvent(QDropEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

protected slots:
  void onItemChanged(QListWidgetItem * item);

};

class CloudLayerView : public QWidget {
  Q_OBJECT
  CloudListWidget* m_list;
  QAction * m_add, * m_remove;
public:
  CloudLayerView(QWidget* parent = 0);
protected slots:
  void onCloudAdded(Cloud* c);
  void onAdd();
  void onRemove();
  void onCloudImageRendered();
  void onCurrentRowChanged(int);
};

#endif
