#define _USE_MATH_DEFINES
#include "cloudlayerview.h"

#include "myparams.h"
#include "cloud.h"
#include "cloudpresetcontrol.h"
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QApplication>
#include <QStyle>
#include <QPainter>
#include <QDropEvent>
#include <iostream>

void CloudListWidgetItemDelegate::paint(QPainter* painter,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const {
  if (option.state & QStyle::State_Selected) {
    painter->fillRect(option.rect.adjusted(30, 0, 0, 0),
                      option.palette.color(QPalette::Highlight));
  }
  QString title  = index.data(Qt::DisplayRole).toString();
  QPixmap iconPm = qvariant_cast<QPixmap>(index.data(Qt::DecorationRole));

  QRect r = option.rect.adjusted(80, 0, 0, 0);
  painter->drawText(r, Qt::AlignVCenter | Qt::AlignLeft, title, &r);

  painter->drawPixmap(option.rect.topLeft() + QPoint(35, 5), iconPm);

  painter->save();
  painter->setPen(Qt::lightGray);
  painter->setBrush(Qt::gray);
  painter->drawRect(
      QRect(option.rect.topLeft(), QSize(30, option.rect.height() - 1)));
  painter->restore();

  bool visible    = index.data(Qt::UserRole).toBool();
  QString svgPath = (visible) ? ":Resources/eye.svg" : ":Resources/eye_off.svg";
  painter->drawPixmap(option.rect.topLeft() + QPoint(5, 10),
                      QIcon(svgPath).pixmap(20, 20));
}

CloudLayerViewItem::CloudLayerViewItem(Cloud* c)
    : QListWidgetItem(), m_cloud(c) {
  setData(Qt::DisplayRole, m_cloud->getName());
  updateIcon();
  setFlags(flags() | Qt::ItemIsEditable);
  setData(Qt::UserRole, c->getParam(LayerVisibility));
}

void CloudLayerViewItem::updateIcon() {
  QImage cloudImage = m_cloud->getCloudImage();
  if (cloudImage.isNull())
    setData(Qt::DecorationRole,
            QIcon(":Resources/drawmode.svg").pixmap(40, 40));
  else {
    QPixmap pm(40, 40);
    pm.fill(Qt::transparent);
    QPainter painter(&pm);
    QPixmap cloudIconPm = QPixmap::fromImage(
        cloudImage.scaled(QSize(40, 40), Qt::KeepAspectRatio));
    painter.drawPixmap((40 - cloudIconPm.width()) / 2,
                       (40 - cloudIconPm.height()) / 2, cloudIconPm);
    painter.end();
    setData(Qt::DecorationRole, pm);
  }
}

CloudListWidget::CloudListWidget(QWidget* parent) : QListWidget(parent) {
  setIconSize(QSize(40, 40));
  setAlternatingRowColors(true);
  setDragDropMode(QAbstractItemView::InternalMove);

  connect(this, SIGNAL(itemChanged(QListWidgetItem*)), this,
          SLOT(onItemChanged(QListWidgetItem*)));

  setItemDelegate(new CloudListWidgetItemDelegate(this));
}

void CloudListWidget::onItemChanged(QListWidgetItem* item) {
  CloudLayerViewItem* cloudItem = dynamic_cast<CloudLayerViewItem*>(item);
  if (!cloudItem) return;

  if (item->text().isEmpty())
    item->setText(cloudItem->cloud()->getName());
  else if (item->text() != cloudItem->cloud()->getName())
    cloudItem->cloud()->setName(item->text());
}

void CloudListWidget::dropEvent(QDropEvent* event) {
  QList<QListWidgetItem*> droppedItems = selectedItems();

  QListWidget::dropEvent(event);

  QList<Cloud*> newClouds;
  int newIndex;
  for (int i = 0; i < count(); i++) {
    if (item(i) == droppedItems.at(0)) newIndex = i;

    CloudLayerViewItem* cloudItem = dynamic_cast<CloudLayerViewItem*>(item(i));
    if (cloudItem) newClouds.push_back(cloudItem->cloud());
  }

  MyParams::instance()->setClouds(newClouds);
  setCurrentRow(newIndex);
  emit currentRowChanged(newIndex);
}

void CloudListWidget::mousePressEvent(QMouseEvent* event) {
  CloudLayerViewItem* item =
      dynamic_cast<CloudLayerViewItem*>(itemAt(event->pos()));
  if (item && event->pos().x() <= 30) {
    bool visible = item->cloud()->isVisible();
    item->cloud()->setParam(LayerVisibility, !visible);
    item->setData(Qt::UserRole, !visible);
    // setting isDragging to true in order to prevent global parameter to be
    // copied to cloud
    // isDraggingをtrueにすることでGlobalParamからCloudにパラメータがコピーされるのを防ぐ
    MyParams::instance()->notifyParamChanged(LayerVisibility, true);
    update();
    return;
  }
  QListWidget::mousePressEvent(event);
}

CloudLayerView::CloudLayerView(QWidget* parent) : QWidget(parent) {
  QToolBar* btnToolBar = new QToolBar(this);

  m_list = new CloudListWidget(this);

  CloudPresetControl* presetControl = new CloudPresetControl(this);

  btnToolBar->setIconSize(QSize(25, 25));
  btnToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
  btnToolBar->setOrientation(Qt::Vertical);
  m_add    = btnToolBar->addAction(QIcon(":Resources/addcloud.svg"), tr("Add"));
  m_remove = btnToolBar->addAction(
      QApplication::style()->standardIcon(QStyle::SP_DialogDiscardButton),
      tr("Remove"));

  QVBoxLayout* lay = new QVBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(5);
  {
    QHBoxLayout* upperLay = new QHBoxLayout();
    upperLay->setMargin(0);
    upperLay->setSpacing(0);
    {
      upperLay->addWidget(btnToolBar, 0);
      upperLay->addWidget(m_list, 1);
    }
    lay->addLayout(upperLay, 1);

    lay->addWidget(presetControl, 0);
  }
  setLayout(lay);

  connect(MyParams::instance(), SIGNAL(cloudAdded(Cloud*)), this,
          SLOT(onCloudAdded(Cloud*)));
  connect(MyParams::instance(), SIGNAL(cloudImageRendered()), this,
          SLOT(onCloudImageRendered()));
  connect(MyParams::instance(), SIGNAL(cloudsCleared()), m_list, SLOT(clear()));
  connect(m_add, SIGNAL(triggered()), this, SLOT(onAdd()));
  connect(m_remove, SIGNAL(triggered()), this, SLOT(onRemove()));
  connect(m_list, SIGNAL(currentRowChanged(int)), this,
          SLOT(onCurrentRowChanged(int)));

  MyParams* p = MyParams::instance();
  m_remove->setEnabled(p->getCloudCount() > 1);
}

void CloudLayerView::onCloudAdded(Cloud* c) {
  std::cout << "onCloudAdded" << std::endl;
  MyParams* p = MyParams::instance();
  m_list->insertItem(0, new CloudLayerViewItem(c));
  m_list->setCurrentRow(0);
  if (p->getCloudCount() > 1) m_remove->setEnabled(true);
}

void CloudLayerView::onAdd() {
  MyParams* p = MyParams::instance();
  p->addCloud(new Cloud());
}

void CloudLayerView::onRemove() {
  MyParams* p = MyParams::instance();
  p->removeCloud();
  delete m_list->takeItem(m_list->currentRow());
  p->setCurrentCloudIndex(0);
  m_list->setCurrentRow(0);
  if (p->getCloudCount() <= 1) m_remove->setDisabled(true);
}

void CloudLayerView::onCloudImageRendered() {
  CloudLayerViewItem* item =
      dynamic_cast<CloudLayerViewItem*>(m_list->currentItem());
  if (!item) return;
  item->updateIcon();
}

void CloudLayerView::onCurrentRowChanged(int row) {
  MyParams* p = MyParams::instance();
  p->setCurrentCloudIndex(row);
}
