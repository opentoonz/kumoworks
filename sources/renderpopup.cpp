#define _USE_MATH_DEFINES
#include "renderpopup.h"

#include "myparams.h"
#include "cloud.h"
#include "progressview.h"

#include <iostream>

#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QCheckBox>
#include <QProgressBar>

//========================================

RenderPopup::RenderPopup(QWidget* parent)
    : QDialog(parent, Qt::CustomizeWindowHint | Qt::WindowTitleHint) {
  std::cout << "myrenderpopup ctor" << std::endl;

  m_targetCombo        = new QComboBox(this);
  m_regionCombo        = new QComboBox(this);
  m_bgCombo            = new QComboBox(this);
  m_renderQualityCombo = new QComboBox(this);
  m_renderHiddenLayers =
      new QCheckBox(tr("Render hidden layers as well"), this);

  m_dirLabel     = new MyDirPathLabel("", this);
  m_fileNameEdit = new QLineEdit(this);

  m_renderButton = new QPushButton(tr("Render"), this);
  m_cancelButton = new QPushButton(tr("Cancel"), this);

  m_layerProgressBar  = new QProgressBar(this);
  m_cloudProgressView = new ProgressView(this);

  m_targetCombo->addItem(tr("Current cloud only"), CurrentCloudOnly);
  m_targetCombo->addItem(tr("Each cloud to separate image"), EachCloud);
  m_targetCombo->addItem(tr("Combine all clouds to one image"),
                         CombineAllCloud);

  m_regionCombo->addItem(tr("Whole scene size"), WholeSceneSize);
  m_regionCombo->addItem(tr("Cloud bounding box"), CloudBoundingBox);

  m_bgCombo->addItem(tr("No background"), RenderCloudOnly);
  m_bgCombo->addItem(tr("Composite background"), CompositeSky);
  m_bgCombo->addItem(tr("Render background separately"), RenderSkySeparately);

  m_renderQualityCombo->addItem(tr("Full"), 1);
  m_renderQualityCombo->addItem(tr("1/2"), 2);
  m_renderQualityCombo->addItem(tr("1/3"), 3);
  m_renderQualityCombo->addItem(tr("1/4"), 4);

  //--- layout ---

  QGridLayout* mainLay = new QGridLayout();
  mainLay->setMargin(10);
  mainLay->setHorizontalSpacing(5);
  mainLay->setVerticalSpacing(10);
  {
    mainLay->addWidget(new QLabel(tr("Target:"), this), 0, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_targetCombo, 0, 1);

    mainLay->addWidget(m_renderHiddenLayers, 1, 1,
                       Qt::AlignLeft | Qt::AlignVCenter);

    mainLay->addWidget(new QLabel(tr("Region:"), this), 2, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_regionCombo, 2, 1);

    mainLay->addWidget(new QLabel(tr("Background:"), this), 3, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_bgCombo, 3, 1);

    mainLay->addWidget(new QLabel(tr("Rendering quality:"), this), 4, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_renderQualityCombo, 4, 1);

    mainLay->addWidget(new QLabel(tr("Folder:"), this), 5, 0, 1, 2,
                       Qt::AlignLeft | Qt::AlignVCenter);

    mainLay->addWidget(m_dirLabel, 6, 0, 1, 2);

    mainLay->addWidget(new QLabel(tr("File name:"), this), 7, 0,
                       Qt::AlignRight | Qt::AlignVCenter);
    mainLay->addWidget(m_fileNameEdit, 7, 1);

    QHBoxLayout* progressLay = new QHBoxLayout();
    progressLay->setMargin(3);
    progressLay->setSpacing(15);
    {
      progressLay->addWidget(m_cloudProgressView, 0);
      progressLay->addWidget(m_layerProgressBar, 1);
    }
    mainLay->addLayout(progressLay, 8, 0, 1, 2);

    QHBoxLayout* buttonLay = new QHBoxLayout();
    buttonLay->setMargin(0);
    buttonLay->setSpacing(10);
    {
      buttonLay->addWidget(m_cancelButton, 1);
      buttonLay->addWidget(m_renderButton, 2);
    }
    mainLay->addLayout(buttonLay, 9, 0, 1, 2);
  }
  setLayout(mainLay);

  qRegisterMetaType<RenderWorkerThread::RenderResult>(
      "RenderWorkerThread::RenderResult");

  connect(m_renderButton, SIGNAL(clicked()), this,
          SLOT(onRenderButtonClicked()));
  connect(m_targetCombo, SIGNAL(currentIndexChanged(int)), this,
          SLOT(onTargetChanged(int)));
  connect(m_cancelButton, SIGNAL(clicked()), this,
          SLOT(onCancelButtonClicked()));
}

void RenderPopup::showEvent(QShowEvent* event) {
  // update fields. “à—e‚ðXV
  RenderOptions& ro = MyParams::instance()->getRenderOptions();
  m_targetCombo->setCurrentIndex(m_targetCombo->findData(ro.target));
  m_renderHiddenLayers->setChecked(ro.renderHiddenLayers);
  m_regionCombo->setCurrentIndex(m_regionCombo->findData(ro.region));
  m_bgCombo->setCurrentIndex(m_bgCombo->findData(ro.background));
  m_renderQualityCombo->setCurrentIndex(
      m_renderQualityCombo->findData(ro.voxelSkip));
  m_dirLabel->setPath(ro.dirPath);
  m_fileNameEdit->setText(ro.fileName);
}

void RenderPopup::hideEvent(QHideEvent* event) {
  // save the last used settings
  RenderOptions& ro     = MyParams::instance()->getRenderOptions();
  ro.target             = (RO_Target)(m_targetCombo->currentData().toInt());
  ro.region             = (RO_Region)(m_regionCombo->currentData().toInt());
  ro.background         = (RO_Background)(m_bgCombo->currentData().toInt());
  ro.voxelSkip          = m_renderQualityCombo->currentData().toInt();
  ro.dirPath            = m_dirLabel->getPath();
  ro.fileName           = m_fileNameEdit->text();
  ro.renderHiddenLayers = m_renderHiddenLayers->isChecked();
}

void RenderPopup::onRenderButtonClicked() {
  if (m_dirLabel->getPath().isEmpty() || m_fileNameEdit->text().isEmpty()) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("Output directory or filename is empty."));
    return;
  }

  int renderCloudCount = MyParams::instance()->estimateRenderCloudCount(
      (RO_Target)(m_targetCombo->currentData().toInt()),
      m_renderHiddenLayers->isChecked());

  if (renderCloudCount == 0 &&
      (RO_Background)(m_bgCombo->currentData().toInt()) !=
          RenderSkySeparately) {
    QMessageBox::warning(this, tr("Warning"),
                         tr("No clouds to be rendered in this scene."));
    return;
  }

  m_renderButton->setDisabled(true);

  m_layerProgressBar->setRange(0, renderCloudCount);
  m_layerProgressBar->setValue(0);
  RenderOptions ro;
  ro.target             = (RO_Target)(m_targetCombo->currentData().toInt());
  ro.region             = (RO_Region)(m_regionCombo->currentData().toInt());
  ro.background         = (RO_Background)(m_bgCombo->currentData().toInt());
  ro.voxelSkip          = m_renderQualityCombo->currentData().toInt();
  ro.dirPath            = m_dirLabel->getPath();
  ro.fileName           = m_fileNameEdit->text();
  ro.renderHiddenLayers = m_renderHiddenLayers->isChecked();

  m_renderWorkerThread = new RenderWorkerThread(ro, this);

  connect(m_renderWorkerThread, &RenderWorkerThread::renderFinished, this,
          &RenderPopup::onRenderFinished);
  connect(m_renderWorkerThread, SIGNAL(cloudRendered()), this,
          SLOT(advanceProgressBar()));

  m_renderWorkerThread->start();
}

void RenderPopup::onTargetChanged(int index) {
  // Choosing the current cloud option does render regardless of the "hidden
  // layer" checkbox.
  if (m_targetCombo->itemData(index).toInt() == CurrentCloudOnly) {
    m_renderHiddenLayers->setEnabled(false);
    m_layerProgressBar->setVisible(false);
  } else {
    m_renderHiddenLayers->setEnabled(true);
    m_layerProgressBar->setVisible(true);
  }
}

void RenderPopup::advanceProgressBar() {
  int val = m_layerProgressBar->value();
  if (val < m_layerProgressBar->maximum())
    m_layerProgressBar->setValue(val + 1);
}

void RenderPopup::onRenderFinished(RenderWorkerThread::RenderResult result) {
  if (result == RenderWorkerThread::Finished) {
    accept();
  } else {
    if (result == RenderWorkerThread::Failed)
      QMessageBox::critical(this, tr("Critical"),
                            tr("Failed to render clouds."));
    else if (result == RenderWorkerThread::Canceled)
      QMessageBox::information(this, tr("Information"),
                               tr("Rendering canceled."));

    m_layerProgressBar->setValue(0);
    m_renderButton->setEnabled(true);
  }
}

void RenderPopup::onCancelButtonClicked() {
  if (!m_renderWorkerThread || !m_renderWorkerThread->isRunning()) {
    reject();
    return;
  }
  m_renderWorkerThread->cancel();
  m_renderWorkerThread = nullptr;
}