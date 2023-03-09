#define _USE_MATH_DEFINES
#include "mygradientcontrol.h"

#include "mycontrol.h"
#include "pathutils.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QPainter>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QImage>
#include <QPushButton>
#include <QDir>
#include <QSettings>
#include <QPixmap>
#include <QPainterPath>
#include <QIcon>
#include <QComboBox>
#include <QMessageBox>
#include <iostream>

using namespace PathUtils;

namespace {
void drawArrow(QPainter &p, const QPointF a, const QPointF b, const QPointF c,
               bool fill, const QColor colorFill,
               const QColor colorLine = Qt::black) {
  p.setPen(colorLine);
  QPolygonF E0Polygon;
  E0Polygon << a << b << c << a;

  QPainterPath E0Path;
  E0Path.addPolygon(E0Polygon);
  if (fill) p.fillPath(E0Path, QBrush(colorFill));
  p.drawPath(E0Path);
}

QSettings::Format presetFormat() {
#ifdef __APPLE__
  return QSettings::IniFormat;
#else
  return QSettings::IniFormat;
#endif
}
}
//=============================================================================

GradientBar::GradientBar(QWidget *parent)
    : QWidget(parent), m_x0(10), m_currentKeyIndex(0), m_gradient() {
  setMinimumWidth(200);
  setFixedHeight(25);
}

//-----------------------------------------------------------------------------

GradientBar::~GradientBar() {}

//-----------------------------------------------------------------------------
/*! Return current key position.
*/
int GradientBar::getCurrentPos() {
  if (m_currentKeyIndex == -1) return -1;
  return spectrumValueToPos(m_gradient.stops().at(m_currentKeyIndex).first);
}

//-----------------------------------------------------------------------------
/*! Return current key color.
*/
QColor GradientBar::getCurrentColor() {
  if (m_currentKeyIndex == -1)
    return m_gradient.stops().at(getMaxPosKeyIndex()).second;
  return m_gradient.stops().at(m_currentKeyIndex).second;
}

//-----------------------------------------------------------------------------
/*! Set current key index to \b index. Update spectrum bar and emit a signal to
notify current key changed \b currentKeyChanged().
*/
void GradientBar::setCurrentKeyIndex(int index) {
  if (m_currentKeyIndex == index) return;
  m_currentKeyIndex = index;
  update();
  emit currentKeyChanged();
}

//-----------------------------------------------------------------------------
/*! Add a new key to spectrum. Key position is computed \b x using
\b posToSpectrumValue() and color come from position.
*/
void GradientBar::addKeyAt(int x) {
  qreal gradientPos = posToSpectrumValue(x);
  QColor color      = m_gradient.getColorAt(gradientPos);
  m_gradient.setColorAt(gradientPos, color);
  int newIndex = m_gradient.getKeyIndexAt(gradientPos, 0);
  setCurrentKeyIndex(newIndex);
  emit currentKeyAdded(newIndex);
}

//-----------------------------------------------------------------------------
/*! Set current key position to \b pos. Update spectrum bar and emit a signal to
notify current key changed \b currentKeyChanged().
*/
void GradientBar::setCurrentPos(int pos, bool isDragging) {
  QGradientStops keys           = m_gradient.stops();
  keys[m_currentKeyIndex].first = posToSpectrumValue(pos);
  m_gradient.setStops(keys);
  update();
  emit currentPosChanged(isDragging);
}

//-----------------------------------------------------------------------------
/*! Set current key color to \b color.  Update spectrum bar.
*/
void GradientBar::setCurrentColor(const QColor &color) {
  if (m_currentKeyIndex == -1) return;
  QGradientStop key = m_gradient.stops()[m_currentKeyIndex];
  QColor oldColor   = key.second;
  if (oldColor == color) return;

  m_gradient.setColorAt(key.first, color);
  update();
}

//-----------------------------------------------------------------------------
/*! Return spectrum value from widget position \b pos.
*/
double GradientBar::posToSpectrumValue(int pos) {
  return (double)(pos - m_x0) / (double)(width() - 2 * m_x0 - 1);
}

//-----------------------------------------------------------------------------
/*! Return widget position from spectrum value \b spectrumValue.
*/
int GradientBar::spectrumValueToPos(double spectrumValue) {
  return m_x0 + int(0.5 + spectrumValue * (width() - 2 * m_x0));
}

//-----------------------------------------------------------------------------
/*! Paint widget object:
\li a linear gradient computed with spectrum value;
\li arrows for each spectrum key.
*/
void GradientBar::paintEvent(QPaintEvent *e) {
  QPainter p(this);

  int h          = height() - 1;
  int y1         = height() / 2;
  int x1         = width() - m_x0;
  QRectF rectBar = QRectF(m_x0, 0, x1 - m_x0 + 1, y1);

  int spectrumSize = m_gradient.stops().count();
  int i;
  for (i = 0; i < spectrumSize; i++) {
    int pos = spectrumValueToPos(m_gradient.stops().at(i).first);

    int f = 4;
    drawArrow(p, QPointF(pos - f, y1 + f), QPointF(pos, y1),
              QPointF(pos + f, y1 + f), true,
              (m_currentKeyIndex == i) ? Qt::black : Qt::white);
  }

  p.setPen(Qt::NoPen);

  // Draw the gradient
  m_gradient.setStart(QPointF(m_x0, h));
  m_gradient.setFinalStop(QPointF(x1, h));
  p.setBrush(m_gradient);
  p.drawRect(rectBar);
}

//-----------------------------------------------------------------------------
/*! Manage mouse press event. Search exsisting key near click position, if
there is set it as current key; otherwise add a new key in click
position.
*/
void GradientBar::mousePressEvent(QMouseEvent *e) {
  QPoint pos = e->pos();
  int x      = pos.x();

  // Verifico se esiste una key vicino alla posizione in cui ho cliccato
  int index        = -1;
  int spectrumSize = m_gradient.stops().count();
  if (x < m_x0)
    index = getMinPosKeyIndex();
  else if (x > width() - m_x0)
    index = getMaxPosKeyIndex();
  else
    index = getNearPosKeyIndex(x);

  // Se x e' vicino a una key esistente setto questa come corrente
  if (index != -1)
    setCurrentKeyIndex(index);
  else  // Altrimenti aggiungo una nuova key
    addKeyAt(x);
}

//-----------------------------------------------------------------------------
/*! Manage mouse move event. If y position value greater than widget height
erase current key; otherwise move current key position.
*/
void GradientBar::mouseMoveEvent(QMouseEvent *e) {
  QPoint pos = e->pos();
  int x      = pos.x();
  int y      = pos.y();
  if (x < m_x0 || x > width() - m_x0 - 1) return;

  if (y > height()) {
    if (m_currentKeyIndex == -1 || m_gradient.stops().count() == 2) return;

    QGradientStops stops = m_gradient.stops();
    stops.remove(m_currentKeyIndex);
    m_gradient.setStops(stops);
    int keyIndex = m_currentKeyIndex;
    setCurrentKeyIndex(-1);
    emit currentKeyRemoved(keyIndex);
    return;
  }
  if (m_currentKeyIndex == -1) addKeyAt(x);

  setCurrentPos(x, true);
}
//-----------------------------------------------------------------------------

void GradientBar::mouseReleaseEvent(QMouseEvent *e) {
  if (m_currentKeyIndex == -1) return;
  QPoint pos = e->pos();
  int x      = pos.x();
  int y      = pos.y();
  if (x < m_x0 || x > width() - m_x0 - 1) return;
  if (y > height()) return;

  setCurrentPos(x, false);
}

//-----------------------------------------------------------------------------
/*! Return index of key with maximum position.
*/
int GradientBar::getMaxPosKeyIndex() {
  return (m_gradient.stops().isEmpty()) ? -1 : m_gradient.stops().count() - 1;
}

//-----------------------------------------------------------------------------
/*! Return index of key with minimum position.
*/
int GradientBar::getMinPosKeyIndex() {
  return (m_gradient.stops().isEmpty()) ? -1 : 0;
}

//-----------------------------------------------------------------------------
/*! Return index of key "near" \b pos. "near" mean with a dinstace less than
a constant size. If there aren't key near pos return \b -1.
*/
int GradientBar::getNearPosKeyIndex(int pos) {
  int i;
  int gap = 20;
  for (i = 0; i < m_gradient.stops().count(); i++)
    if (std::abs(pos - spectrumValueToPos(m_gradient.stops().at(i).first)) <
        gap)
      return i;
  return -1;
}

//=============================================================================
namespace {
int presetId = 1;
}

GradientPresetControl::GradientPresetControl(QWidget *parent)
    : QWidget(parent) {
  m_presetCombo                    = new QComboBox(this);
  m_addPresetBtn                   = new PlusMinusButton(true, this);
  PlusMinusButton *removePresetBtn = new PlusMinusButton(false, this);

  m_presetCombo->setIconSize(QSize(100, 15));

  loadPresetItems();

  QHBoxLayout *lay = new QHBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(3);
  {
    lay->addWidget(m_presetCombo, 1);
    lay->addWidget(m_addPresetBtn, 0);
    lay->addWidget(removePresetBtn, 0);
  }
  setLayout(lay);

  connect(m_presetCombo, SIGNAL(activated(int)), this,
          SLOT(onPresetComboActivated(int)));
  connect(removePresetBtn, SIGNAL(clicked()), this,
          SLOT(onRemovePresetBtnClicked()));
}

void GradientPresetControl::loadPresetItems() {
  qRegisterMetaTypeStreamOperators<ColorGradient>("ColorGradient");
  QSettings gradPreset(getGradientPresetPath(), presetFormat());
  QStringList keys = gradPreset.childKeys();
  for (int i = 0; i < keys.size(); i++) {
    ColorGradient grad = gradPreset.value(keys.at(i)).value<ColorGradient>();

    presetId = std::max(presetId, keys.at(i).toInt() + 1);

    QPixmap iconPm(300, 20);
    QPainter p(&iconPm);
    grad.setStart(QPointF(0, 0));
    grad.setFinalStop(QPointF(300, 0));
    p.setBrush(grad);
    p.drawRect(QRect(0, 0, 300, 20));
    QIcon icon(iconPm);
    m_presetCombo->addItem(icon, keys.at(i), gradPreset.value(keys.at(i)));
  }
}

void GradientPresetControl::addPreset(ColorGradient gradient) {
  qRegisterMetaTypeStreamOperators<ColorGradient>("ColorGradient");
  QSettings gradPreset(getGradientPresetPath(), presetFormat());
  gradPreset.setValue(QString::number(presetId), QVariant::fromValue(gradient));

  QPixmap iconPm(300, 20);
  QPainter p(&iconPm);
  gradient.setStart(QPointF(0, 0));
  gradient.setFinalStop(QPointF(300, 0));
  p.setBrush(gradient);
  p.drawRect(QRect(0, 0, 300, 20));
  QIcon icon(iconPm);
  m_presetCombo->addItem(icon, QString::number(presetId),
                         QVariant::fromValue(gradient));
  m_presetCombo->setCurrentText(QString::number(presetId));
  presetId++;
}

void GradientPresetControl::resizeEvent(QResizeEvent *e) {
  m_presetCombo->setIconSize(QSize(m_presetCombo->width() - 60, 15));
}

void GradientPresetControl::onPresetComboActivated(int) {
  emit presetSelected(m_presetCombo->currentData().value<ColorGradient>());
}

void GradientPresetControl::onRemovePresetBtnClicked() {
  if (m_presetCombo->currentIndex() == -1) return;
  QString key = m_presetCombo->currentText();
  QMessageBox msgBox;
  msgBox.setText(
      tr("Are you sure you want to remove the preset %1 ?").arg(key));
  msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
  msgBox.setDefaultButton(QMessageBox::Ok);
  int ret = msgBox.exec();
  if (ret == QMessageBox::Cancel) return;

  QSettings gradPreset(getGradientPresetPath(), presetFormat());
  gradPreset.remove(key);
  m_presetCombo->removeItem(m_presetCombo->currentIndex());
}

//=============================================================================

GradientControl::GradientControl(QWidget *parent)
    : QWidget(parent), m_margin(0), m_spacing(0) {
  QVBoxLayout *layout = new QVBoxLayout();
  layout->setMargin(m_margin);
  layout->setSpacing(m_spacing);

  m_gradientBar = new GradientBar(this);
  connect(m_gradientBar, SIGNAL(currentPosChanged(bool)),
          SLOT(onCurrentPosChanged(bool)));
  connect(m_gradientBar, SIGNAL(currentKeyChanged()),
          SLOT(onCurrentKeyChanged()));
  connect(m_gradientBar, SIGNAL(currentKeyAdded(int)), this,
          SLOT(onKeyAddedOrRemoved()));
  connect(m_gradientBar, SIGNAL(currentKeyRemoved(int)), this,
          SLOT(onKeyAddedOrRemoved()));

  layout->addWidget(m_gradientBar);

  m_colorEdit = new MyColorEdit(this, Qt::black);
  connect(m_colorEdit, SIGNAL(valueChanged(bool)), this,
          SLOT(onColorChanged()));
  layout->addWidget(m_colorEdit, 0, Qt::AlignLeft);

  layout->addSpacing(3);

  m_presetControl = new GradientPresetControl(this);
  connect(m_presetControl->addButton(), SIGNAL(clicked()), this,
          SLOT(onAddPresetBtnPressed()));
  connect(m_presetControl, SIGNAL(presetSelected(ColorGradient)), this,
          SLOT(onPresetSelected(ColorGradient)));
  layout->addWidget(m_presetControl, 0);

  setLayout(layout);
}

//-----------------------------------------------------------------------------
/*! Update all widget and emit keyPositionChanged() signal.
*/
void GradientControl::onCurrentPosChanged(bool isDragging) {
  if (m_gradientBar->getCurrentKeyIndex() == -1) return;
  update();
  emit valueChanged(isDragging);
}

//-----------------------------------------------------------------------------
/*! Set color field to spectrum current key color and update.
*/
void GradientControl::onCurrentKeyChanged() {
  if (m_gradientBar->getCurrentKeyIndex() != -1) {
    auto temp = m_gradientBar->getCurrentColor();
    m_colorEdit->setValue(temp);
  }
  update();
}

//-----------------------------------------------------------------------------
/*! Set spectrum current key color to \b color and emit keyColorChanged().
*/
void GradientControl::onColorChanged() {
  m_gradientBar->setCurrentColor(m_colorEdit->getValue());
  emit valueChanged(false);
}

//-----------------------------------------------------------------------------

void GradientControl::onAddPresetBtnPressed() {
  m_presetControl->addPreset(m_gradientBar->getGradient());
}

//-----------------------------------------------------------------------------
/*! Paint an arrow to connect current spectrum key and square color field.
*/
void GradientControl::paintEvent(QPaintEvent *e) {
  int WidgetHeight = 24;
  int curPos       = m_gradientBar->getCurrentPos();
  if (curPos == -1) return;
  QPainter p(this);
  int x0 = 18 + m_margin;
  int y0 = 2 * m_margin + WidgetHeight + m_spacing;
  int y  = m_margin + m_spacing +
          (WidgetHeight * 0.5 -
           4);  // 4 e' l'altezza della freccia nella spectrum bar
  int y1 = y0 - y * 0.5 + 1;
  int y2 = y0 - y;
  curPos += m_margin;
  p.setPen(Qt::black);
  p.drawLine(x0, y0, x0, y1);
  p.drawLine(x0, y1, curPos, y1);
  p.drawLine(curPos, y1, curPos, y2);
}

//-----------------------------------------------------------------------------

void GradientControl::setSpectrum(ColorGradient &gradient) {
  m_gradientBar->setGradient(gradient);
  auto temp = m_gradientBar->getCurrentColor();
  m_colorEdit->setValue(temp);
}

//-----------------------------------------------------------------------------

void GradientControl::setCurrentKeyIndex(int index) {
  m_gradientBar->setCurrentKeyIndex(index);
  auto temp = m_gradientBar->getCurrentColor();
  m_colorEdit->setValue(temp);
}

//-----------------------------------------------------------------------------

void GradientControl::onPresetSelected(ColorGradient grad) {
  m_gradientBar->setGradient(grad);
  emit valueChanged(false);
}

//-----------------------------------------------------------------------------

GradientControl::~GradientControl() {}
