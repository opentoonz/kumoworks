#include "progressview.h"

#include "myparams.h"
#include <QPainter>

ProgressView::ProgressView(QWidget* parent) 
: QWidget(parent)
{
  setFixedSize(150, 10);
  connect(MyParams::instance(), SIGNAL(flowStateChanged()), this, SLOT(update()));
}

void ProgressView::paintEvent(QPaintEvent* event) {
  QPainter p(this);
  MyParams* params = MyParams::instance();

  if (params->isFlowDone(Flow_GetCloudImage)) return;

  p.fillRect(rect(), Qt::gray);

  int flowCount = Flow_GetCloudImage - Flow_Silhouette2Voxel + 1;

  QRectF singleFlowRect = QRectF(rect());
  singleFlowRect.setWidth(singleFlowRect.width() / (float)flowCount);

  p.setPen(Qt::NoPen);

  for (int i = 0; i < flowCount; i++) {
    FlowId currentFlowId = (FlowId)((int)Flow_Silhouette2Voxel + i);
    
    if (params->isFlowDone(currentFlowId))
      p.setBrush(QColor(0, 200, 0));
    else
      p.setBrush(QColor(200, 200, 0));

    p.drawRect(singleFlowRect.adjusted(1, 1, -1, -1));

    singleFlowRect.translate(singleFlowRect.width(), 0.0);
  }
  p.end();
}
