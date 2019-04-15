
#include "mycontrol.h"
#include "mygradientcontrol.h"
#include "undomanager.h"
#include "sky.h"

#include <QLineEdit>
#include <QSlider>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QColorDialog>
#include <QPainter>
#include <QPushButton>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QStackedLayout>
#include <QIcon>

PlusMinusButton::PlusMinusButton(bool isPlus, QWidget* parent)
    : QPushButton(parent) {
  setFixedSize(20, 20);
  setStyleSheet(
      "QPushButton {border: 0px} QPushButton:hover {background: rgb(180, 230, "
      "255)}");

  QString iconName = (isPlus) ? ":Resources/plus.svg" : ":Resources/minus.svg";
  setIcon(QIcon(iconName));
  setIconSize(QSize(17, 17));
}

//========================================

MySlider::MySlider(QWidget* parent, double value, double min, double max,
                   int decimals)
    : QWidget(parent), m_decimals(decimals) {
  m_edit   = new QLineEdit(QString::number(value, 'f', m_decimals), this);
  m_slider = new QSlider(Qt::Horizontal, this);

  QDoubleValidator* validator = new QDoubleValidator(min, max, decimals);
  m_edit->setValidator(validator);
  m_edit->setFixedWidth(50);

  m_slider->setRange(min * pow(10, decimals), max * pow(10, decimals));
  m_slider->setValue(value * pow(10, decimals));

  QHBoxLayout* layout = new QHBoxLayout();
  layout->setMargin(0);
  layout->setSpacing(3);
  {
    layout->addWidget(m_edit, 0);
    layout->addWidget(m_slider, 1);
  }
  setLayout(layout);

  connect(m_edit, SIGNAL(editingFinished()), this, SLOT(onLineEdited()));
  connect(m_slider, SIGNAL(sliderMoved(int)), this, SLOT(onSliderChanged(int)));
  connect(m_slider, SIGNAL(sliderReleased()), this, SLOT(onSliderReleased()));
}

void MySlider::setValue(double value) {
  if (value == -1) {
    // it will be called when the flow Flow_Silhouette2Voxel is undone
    if (isEnabled()) return;  // ignore if it is enabled
    m_edit->setText("- - -");
    m_slider->setValue(0.0);
    return;
  }
  int notUsed;
  auto temp = QString::number(value, 'f', m_decimals);
  if (m_edit->validator()->validate(temp, notUsed) != QValidator::Acceptable)
    return;

  m_edit->setText(QString::number(value, 'f', m_decimals));
  m_slider->setValue(value * pow(10, m_decimals));
}

double MySlider::getValue() { return m_edit->text().toDouble(); }

void MySlider::onSliderChanged(int sliderVal) {
  double value = (double)sliderVal * pow(10.0, -(double)m_decimals);
  m_edit->setText(QString::number(value, 'f', m_decimals));
  emit valueChanged(true);
}

void MySlider::onSliderReleased() { emit valueChanged(false); }

void MySlider::onLineEdited() {
  m_slider->setValue(getValue() * pow(10, m_decimals));
  emit valueChanged(false);
}

//========================================

MyColorEdit::MyColorEdit(QWidget* parent, QColor val)
    : QWidget(parent), m_color(val) {
  setFixedSize(150, 20);
}

void MyColorEdit::paintEvent(QPaintEvent*) {
  QPainter p(this);
  p.fillRect(rect(), m_color);

  double brightness = 0.299 * m_color.redF() + 0.587 * m_color.greenF() +
                      0.114 * m_color.blueF();
  if (brightness < 0.5)
    p.setPen(Qt::white);
  else
    p.setPen(Qt::black);

  QString str = QString("R%1, G%2, B%3")
                    .arg(m_color.red())
                    .arg(m_color.green())
                    .arg(m_color.blue());
  p.drawText(rect(), Qt::AlignCenter, str);
}

void MyColorEdit::mousePressEvent(QMouseEvent*) {
  QColor col = QColorDialog::getColor(m_color, this, tr("Select Color"),
                                      QColorDialog::ColorDialogOptions());
  if (col == m_color || !col.isValid()) return;
  m_color = col;
  update();
  emit valueChanged(false);
}

//========================================

MyCheckBox::MyCheckBox(QWidget* parent, bool value) : QCheckBox("", parent) {
  setChecked(value);
  connect(this, SIGNAL(clicked(bool)), this, SLOT(onClicked()));
}

//========================================

MyPathLabel::MyPathLabel(QString path, QWidget* parent)
    : QWidget(parent), m_path(path) {
  m_pathLabel  = new QLabel(this);
  m_loadButton = new QPushButton("...", this);

  m_loadButton->setFixedWidth(20);
  m_pathLabel->setText(QFileInfo(m_path).fileName());
  m_pathLabel->setStyleSheet("background:white;");

  QHBoxLayout* lay = new QHBoxLayout();
  lay->setMargin(0);
  lay->setSpacing(3);
  {
    lay->addWidget(m_pathLabel, 1);
    lay->addWidget(m_loadButton, 0);
  }
  setLayout(lay);

  connect(m_loadButton, SIGNAL(clicked(bool)), this,
          SLOT(onLoadButtonClicked()));
}

QString MyPathLabel::getPath() const { return m_path; }

void MyPathLabel::setPath(const QString path) {
  m_path = path;
  m_pathLabel->setText(QFileInfo(m_path).fileName());
}

//----------------------------------------

void MyImgPathLabel::onLoadButtonClicked() {
  QString path = QFileDialog::getOpenFileName(
      this, tr("Select File"), m_path, tr("Images (*.png *.jpg *.tif *.tga)"));

  if (!QFileInfo(path).isReadable()) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("The file %1 is not readable.").arg(path));
    return;
  }

  if (QPixmap(path).isNull()) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Failed to open %1.").arg(path));
    return;
  }

  m_path = path;
  m_pathLabel->setText(QFileInfo(m_path).fileName());

  emit valueChanged(false);
}

//----------------------------------------

void MyDirPathLabel::onLoadButtonClicked() {
  QString initialPath = (m_path.isEmpty()) ? QDir::currentPath() : m_path;
  QString path = QFileDialog::getExistingDirectory(this, tr("Select Directory"),
                                                   initialPath);

  m_path = path;
  m_pathLabel->setText(m_path);
}

void MyDirPathLabel::setPath(const QString path) {
  m_path = path;
  m_pathLabel->setText(m_path);
}
//========================================

namespace {
QStackedLayout* subStackLay;
};

QString MyControl::getCategoryLabelName(ParamCategory cat) {
  static QMap<ParamCategory, QString> names = {
      {Background, tr("Background")},
      {Camera, tr("Camera")},
      {Sun, tr("Sun")},
      {SkyRender, tr("Sky Render")},
      {Shape1, tr("Shape") + QString("1")},
      {Shape2, tr("Shape") + QString("2")},
      {Noise, tr("Noise")},
      {Render, tr("Render")}};

  return names.value(cat, QString(""));
}

MyControl::MyControl(QList<ParamCategory> categories, QWidget* parent)
    : QWidget(parent) {
  MyParams* p = MyParams::instance();

  QVBoxLayout* mainLay = new QVBoxLayout();
  mainLay->setMargin(5);
  mainLay->setSpacing(10);
  setLayout(mainLay);

  // create group box for each category. カテゴリごとにGroupBoxを作る
  QMap<ParamCategory, QGroupBox*> groupBoxes;

  QGridLayout* subLay[2];

  for (int c = 0; c < CategoryCount; c++) {
    ParamCategory cat = (ParamCategory)c;
    if (!categories.contains(cat)) continue;
    groupBoxes[cat] = new QGroupBox(getCategoryLabelName(cat), this);
    mainLay->addWidget(groupBoxes[(ParamCategory)c]);
    QGridLayout* lay = new QGridLayout();
    lay->setMargin(10);
    lay->setHorizontalSpacing(10);
    lay->setVerticalSpacing(10);
    lay->setColumnStretch(0, 0);
    lay->setColumnStretch(1, 1);
    groupBoxes[(ParamCategory)c]->setLayout(lay);

    if (c == Sun) {
      subStackLay = new QStackedLayout();
      for (int sl = 0; sl < 2; sl++) {
        QWidget* subW = new QWidget(this);
        subLay[sl]    = new QGridLayout();
        subLay[sl]->setMargin(0);
        subLay[sl]->setHorizontalSpacing(10);
        subLay[sl]->setVerticalSpacing(10);
        subLay[sl]->setColumnStretch(0, 0);
        subLay[sl]->setColumnStretch(1, 1);
        subW->setLayout(subLay[sl]);
        subStackLay->addWidget(subW);
      }
      subStackLay->setCurrentIndex(p->getBool(SunUseUniqueAngles) ? 0 : 1);
    }
  }

  // create control for each parameter. 各パラメータの格納
  for (int i = 0; i < ParamCount; i++) {
    Param param       = p->getParam((ParamId)i);
    ParamCategory cat = param.category;
    if (!categories.contains(cat)) continue;

    QGridLayout* layout =
        qobject_cast<QGridLayout*>(groupBoxes[param.category]->layout());
    if (i >= SunUniqueElevation && i <= SunAzimuth) {
      if (i == SunUniqueElevation)
        layout->addLayout(subStackLay, layout->rowCount(), 0, 1, 2);
      layout = subLay[(i <= SunUniqueAzimuth) ? 0 : 1];
    }

    int row = layout->rowCount();
    layout->addWidget(new QLabel(param.name, this), row, 0,
                      Qt::AlignRight | Qt::AlignVCenter);

    switch (param.type) {
    case QMetaType::Double: {
      MySlider* slider =
          new MySlider(this, param.value.toDouble(), param.min.toDouble(),
                       param.max.toDouble(), param.decimal);
      connect(slider, SIGNAL(valueChanged(bool)), this,
              SLOT(onControlChanged(bool)));
      m_idControlMap[(ParamId)i] = slider;

      if ((ParamId)i == ShapeDepth)
        connect(MyParams::instance(), SIGNAL(setAutoDepthTempValue(double)),
                slider, SLOT(setValue(double)));
    } break;
    case QMetaType::Int: {
      MySlider* slider = new MySlider(this, param.value.toInt(),
                                      param.min.toInt(), param.max.toInt(), 0);
      connect(slider, SIGNAL(valueChanged(bool)), this,
              SLOT(onControlChanged(bool)));
      m_idControlMap[(ParamId)i] = slider;
    } break;
    case QMetaType::QColor: {
      MyColorEdit* colorEdit =
          new MyColorEdit(this, param.value.value<QColor>());
      connect(colorEdit, SIGNAL(valueChanged(bool)), this,
              SLOT(onControlChanged(bool)));
      m_idControlMap[(ParamId)i] = colorEdit;
    } break;
    case QMetaType::Bool: {
      MyCheckBox* checkBox = new MyCheckBox(this, param.value.toBool());
      connect(checkBox, SIGNAL(valueChanged(bool)), this,
              SLOT(onControlChanged(bool)));
      m_idControlMap[(ParamId)i] = checkBox;
    } break;
    case QMetaType::QString: {
      MyImgPathLabel* imgPathLabel =
          new MyImgPathLabel(param.value.toString(), this);
      connect(imgPathLabel, SIGNAL(valueChanged(bool)), this,
              SLOT(onControlChanged(bool)));
      m_idControlMap[(ParamId)i] = imgPathLabel;
    } break;
    case QMetaType::User:  // ColorGradient
    {
      GradientControl* gradientControl = new GradientControl(this);
      ColorGradient gradient           = param.value.value<ColorGradient>();
      gradientControl->setSpectrum(gradient);
      connect(gradientControl, SIGNAL(valueChanged(bool)), this,
              SLOT(onControlChanged(bool)));
      m_idControlMap[(ParamId)i] = gradientControl;
    } break;
    default:
      break;
    }
    layout->addWidget(m_idControlMap[(ParamId)i], row, 1);
  }

  // sync UI when the parameter is externally changed.
  //外部からパラメータが変化したとき、UIを同期する
  connect(p, SIGNAL(paramChanged(ParamId)), this,
          SLOT(onParameterChanged(ParamId)));
}

void MyControl::onControlChanged(bool isDragging) {
  QWidget* senderWidget = qobject_cast<QWidget*>(sender());
  if (!senderWidget) return;
  ParamId id = m_idControlMap.key(senderWidget);

  MyParams* p  = MyParams::instance();
  Param& param = p->getParam(id);

  ParameterUndo* undo =
      dynamic_cast<ParameterUndo*>(UndoManager::instance()->tmpCommand());
  if (undo == nullptr) {
    Cloud* cloud = MyParams::instance()->getCurrentCloud();
    undo         = new ParameterUndo(cloud, id, param.value);
    UndoManager::instance()->setTmpCommand(undo);
  }

  switch (param.type) {
  case QMetaType::Double: {
    MySlider* slider = qobject_cast<MySlider*>(senderWidget);
    param.value      = slider->getValue();
  } break;
  case QMetaType::Int: {
    MySlider* slider = qobject_cast<MySlider*>(senderWidget);
    param.value      = (int)slider->getValue();
  } break;
  case QMetaType::QColor: {
    MyColorEdit* colorEdit = qobject_cast<MyColorEdit*>(senderWidget);
    param.value            = colorEdit->getValue();
  } break;
  case QMetaType::Bool: {
    MyCheckBox* checkBox = qobject_cast<MyCheckBox*>(senderWidget);
    param.value          = checkBox->isChecked();
  } break;
  case QMetaType::QString: {
    MyImgPathLabel* imgPathLabel = qobject_cast<MyImgPathLabel*>(senderWidget);
    param.value                  = imgPathLabel->getPath();
  } break;
  case QMetaType::User:  // ColorGradient
  {
    GradientControl* gradientControl =
        qobject_cast<GradientControl*>(senderWidget);
    param.value = QVariant::fromValue(gradientControl->getGradient());
  } break;
  default:
    break;
  }

  if (id == CameraFov && p->getBool(CameraFixEyeLevel)) {
    p->getParam(CameraElevation).value =
        Sky::instance()->getEyeElevationWithFixedEyeLevel();
    p->notifyParamChanged(CameraElevation, isDragging);
  } else if (id == ShapeDepthAuto) {
    bool isAuto      = param.value.toBool();
    MySlider* slider = qobject_cast<MySlider*>(m_idControlMap[ShapeDepth]);
    slider->setDisabled(isAuto);
    if (isAuto)
      slider->setValue(-1);
    else
      slider->setValue(p->getParam(ShapeDepth).value.toDouble());
  }

  p->notifyParamChanged(id, isDragging);

  // open confirmation dialog asking if the scene should be resized to the image
  // size
  // ここで、読み込んだ画像に合わせてシーンのサイズを変更するか確認させる
  if (id == LayoutImgPath || id == BackgroundImgPath)
    p->checkImgSizeAndChangeSceneSizeIfNeeded(id);

  if (!isDragging) {
    undo->setAfterVal(param.value);
    UndoManager::instance()->stack()->push(undo);
    UndoManager::instance()->releaseTmpCommand();
  }
}

// sync UI when the parameter is externally changed
// 外部からパラメータが変化したとき、UIを同期する
void MyControl::onParameterChanged(ParamId id) {
  QWidget* control = m_idControlMap.value(id, nullptr);
  if (!control) return;

  MyParams* p  = MyParams::instance();
  Param& param = p->getParam(id);
  switch (param.type) {
  case QMetaType::Double:
  case QMetaType::Int: {
    MySlider* slider = qobject_cast<MySlider*>(control);
    slider->setValue(param.value.toDouble());
  } break;
  case QMetaType::QColor: {
    MyColorEdit* colorEdit = qobject_cast<MyColorEdit*>(control);
    auto temp              = param.value.value<QColor>();
    colorEdit->setValue(temp);
  } break;
  case QMetaType::Bool: {
    MyCheckBox* checkBox = qobject_cast<MyCheckBox*>(control);
    checkBox->setChecked(param.value.toBool());
  } break;
  case QMetaType::QString: {
    MyImgPathLabel* imgPathLabel = qobject_cast<MyImgPathLabel*>(control);
    imgPathLabel->setPath(param.value.toString());
  } break;
  case QMetaType::User:  // ColorGradient
  {
    GradientControl* gradientControl = qobject_cast<GradientControl*>(control);
    ColorGradient gradient           = param.value.value<ColorGradient>();
    gradientControl->setSpectrum(gradient);
  }
  default:
    break;
  }

  // toggle sun direction to global / unique
  // 光源の向きをグローバル/個別にする切り替え
  if (id == SunUseUniqueAngles) {
    bool useUnique = p->getBool(SunUseUniqueAngles);
    // switch sliders. UIの表示/非表示
    subStackLay->setCurrentIndex(useUnique ? 0 : 1);
    // when toggling to unique direction, copy the global ones
    // 個別を有効にしたとき、グローバルに値を合わせる
    if (useUnique) {
      p->getParam(SunUniqueElevation).value = p->getDouble(SunElevation);
      p->getParam(SunUniqueAzimuth).value   = p->getDouble(SunAzimuth);
      onParameterChanged(SunUniqueElevation);
      onParameterChanged(SunUniqueAzimuth);
    }
  } else if (id == ShapeDepthAuto) {
    bool isAuto      = param.value.toBool();
    MySlider* slider = qobject_cast<MySlider*>(m_idControlMap[ShapeDepth]);
    slider->setDisabled(isAuto);
    if (isAuto)
      slider->setValue(-1);
    else
      slider->setValue(p->getParam(ShapeDepth).value.toDouble());
  }
}
