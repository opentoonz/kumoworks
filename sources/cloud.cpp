
#define _USE_MATH_DEFINES
#include "cloud.h"

#include <QPainter>
#include <QVector3D>
#include <QMatrix4x4>
#include <QThreadPool>
#include <QSettings>
#include <QApplication>
#include <iostream>
#include <memory>

#include "sky.h"
#include "myparams.h"
#include "RandomNumberGenerator.h"
#include "simplexnoise.h"
#include "undomanager.h"

#define DO_TIMER

#ifdef DO_TIMER
#include <QElapsedTimer>
#endif

namespace {

const unsigned int fillColor  = qRgb(100, 10, 10);
const unsigned int lineColor  = qRgb(100, 50, 50);
const unsigned int eraseColor = qRgb(50, 100, 50);

void fillLine(QImage& img, QPoint& p) {
  QList<QPoint> fillPoints;
  // from right to left 右から左に
  for (int x = p.x(); x >= 0; x--) {
    if (img.pixel(x, p.y()) != fillColor) break;
    // adding new fill "seeds" one above and the other.
    // 上下に塗りつぶし候補を追加
    if (p.y() > 0 && img.pixel(x, p.y() - 1) == fillColor)
      fillPoints.append(QPoint(x, p.y() - 1));
    if (p.y() < img.height() - 1 && img.pixel(x, p.y() + 1) == fillColor)
      fillPoints.append(QPoint(x, p.y() + 1));
    img.setPixel(x, p.y(), qRgba(0, 0, 0, 0));
  }
  // from left to right 左から右に
  for (int x = p.x() + 1; x < img.width(); x++) {
    if (img.pixel(x, p.y()) != fillColor) break;
    // adding new fill "seeds" one above and the other.
    // 上下に塗りつぶし候補を追加
    if (p.y() > 0 && img.pixel(x, p.y() - 1) == fillColor)
      fillPoints.append(QPoint(x, p.y() - 1));
    if (p.y() < img.height() - 1 && img.pixel(x, p.y() + 1) == fillColor)
      fillPoints.append(QPoint(x, p.y() + 1));
    img.setPixel(x, p.y(), qRgba(0, 0, 0, 0));
  }

  for (int p = 0; p < fillPoints.size(); p++) fillLine(img, fillPoints[p]);
}

//------------------------------------

const float INF = (float)1e20;  // less than FLT_MAX

// distance transform of 1d function using squared distance
static float* dt(float* f, int n) {
  float* d = new float[n];
  int* v   = new int[n];
  float* z = new float[n + 1];
  // index of rightmost parabola in lower envelope
  int k = 0;
  // locations of parabolas in lower envelope
  v[0] = 0;
  // locations of boundaries between parabolas
  z[0] = -INF;
  z[1] = +INF;
  // compute lower envelope
  for (int q = 1; q <= n - 1; q++) {
    // compute intersection
    float s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k]);
    while (s <= z[k]) {
      k--;
      s = ((f[q] + q * q) - (f[v[k]] + v[k] * v[k])) / (2 * q - 2 * v[k]);
    }
    k++;
    v[k]     = q;
    z[k]     = s;
    z[k + 1] = +INF;
  }
  k = 0;
  // fill in values of distance transform
  for (int q = 0; q <= n - 1; q++) {
    while (z[k + 1] < q) k++;
    d[q] = (q - v[k]) * (q - v[k]) + f[v[k]];
  }

  delete[] v;
  delete[] z;
  return d;
}

float do_distance_transform(float* dst_p, QSize dim) {
  float* f = new float[std::max(dim.width(), dim.height())];

  float max_val = 0.0f;

  float* tmp_dst = dst_p;

  // transform along columns
  for (int i = 0; i < dim.width(); i++) {
    for (int j = 0; j < dim.height(); j++, *tmp_dst++) {
      f[j] = *tmp_dst;
    }

    tmp_dst -= dim.height();
    float* d = dt(f, dim.height());
    for (int j = 0; j < dim.height(); j++, tmp_dst++) {
      *tmp_dst = d[j];
    }
    delete[] d;
  }

  // transform along rows
  for (int j = 0; j < dim.height(); j++) {
    for (int i = 0; i < dim.width(); i++) {
      f[i] = dst_p[j * dim.width() + i];
    }
    float* d = dt(f, dim.width());
    for (int i = 0; i < dim.width(); i++, tmp_dst++) {
      dst_p[j * dim.width() + i]  = d[i];
      if (d[i] > max_val) max_val = d[i];
    }
    delete[] d;
  }

  tmp_dst = dst_p;
  max_val = std::sqrt(max_val);

  // square root and normalize
  for (int i = 0; i < dim.width() * dim.height(); i++, *tmp_dst++) {
    if (max_val > 0)
      *tmp_dst = std::pow(std::sqrt(*tmp_dst) / max_val, 1 / 2.2);
  }
  return max_val;
}

bool isInRange(int x, int min, int max) { return (x >= min) && (x < max); }

Real schlickScatteringAngle(Real theta, Real k = 0.6) {
  Real nume   = 1.0 - k * k;  // nume = 0.64
  Real v      = 1 + k * cos(theta);
  Real denomi = 4.0 * M_PI * v * v;
  return nume / denomi;
}

#ifdef DO_TIMER
QElapsedTimer myTimer;
void startTimer() { myTimer.start(); }
void printElapsedTime(std::string str) {
  std::cout << myTimer.elapsed() << " msecs for " << str << std::endl;
}
#else
// do nothing
void startTimer() {}
void printElapsedTime(std::string str) {}
#endif

QRect getSkippedRect(QRect& rect, int skip) {
  return QRect(rect.topLeft() / skip, rect.size() / skip);
}

}  // namespace

//========================================

// copy the current parameters to the Cloud 現在のパラメータをCloudにコピーする
Cloud::Cloud(QString name) : m_name(name) {
  MyParams* p = MyParams::instance();
  for (int pId = 0; pId < (int)ParamCount; pId++) {
    if (p->isCloudParam((ParamId)pId)) {
      m_params[(ParamId)pId] = p->getParam((ParamId)pId).value;
    }
  }

  QSize size = p->getSceneSize();
  m_canvasPm = QPixmap(size);
  m_canvasPm.fill(Qt::transparent);

  m_bottomRight = QPointF(0, 0);
  m_topLeft     = QPointF(size.width(), size.height());

  m_render = false;

  if (m_name.isEmpty()) m_name = MyParams::instance()->getNextCloudName();
}

Cloud::Cloud(Cloud* c) {
  MyParams* p = MyParams::instance();
  for (int pId = 0; pId < (int)ParamCount; pId++) {
    if (p->isCloudParam((ParamId)pId)) {
      m_params[(ParamId)pId] = c->getParam((ParamId)pId);
    }
  }

  m_canvasPm = QPixmap(c->getCanvasPm());
  setDrawingBBox(c->getDrawingBBox());
  m_render = false;
  m_name   = c->getName();
}

void Cloud::onResize(QSize& size) {
  QPixmap newPixmap = QPixmap(size);
  newPixmap.fill(Qt::transparent);
  QPainter painter(&newPixmap);
  if (!m_canvasPm.isNull()) painter.drawPixmap(0, 0, m_canvasPm);
  painter.end();
  m_canvasPm = newPixmap;

  suspendRender();
  MyParams* p = MyParams::instance();
  p->setFlowUndone(Flow_Silhouette2Voxel);
}

void Cloud::addPos(const QPointF& pos, float radius) {
  if (pos.x() - radius < m_topLeft.x()) m_topLeft.setX(pos.x() - radius);
  if (pos.y() - radius < m_topLeft.y()) m_topLeft.setY(pos.y() - radius);
  if (pos.x() + radius > m_bottomRight.x())
    m_bottomRight.setX(pos.x() + radius);
  if (pos.y() + radius > m_bottomRight.y())
    m_bottomRight.setY(pos.y() + radius);

  suspendRender();
  MyParams* p = MyParams::instance();
  p->setFlowUndone(Flow_Silhouette2Voxel);
}

QRect Cloud::getBBox() {
  if (m_topLeft.x() > m_bottomRight.x()) return QRect();
  MyParams* p = MyParams::instance();
  int margin  = p->getInt(ShapeBBoxMargin);
  QRect bbox  = QRect(m_topLeft.toPoint(), m_bottomRight.toPoint())
                   .adjusted(-margin, -margin, margin, margin);
  return bbox.intersected(QRect(QPoint(0, 0), m_canvasPm.size()));
}

// Used in DrawUndo. DrawUndo で使う
void Cloud::setDrawingBBox(const QRectF rect) {
  m_topLeft     = rect.topLeft();
  m_bottomRight = rect.bottomRight();
}

void Cloud::onStartMove() {
  if (m_canvasPm.isNull()) return;
  MoveCloudUndo* undo = new MoveCloudUndo(this);
  UndoManager::instance()->setTmpCommand(undo);
}

void Cloud::move(QPoint dPos) {
  MyParams* p   = MyParams::instance();
  QRect oldBBox = getBBox();

  m_cloudImageBBox.moveTo(m_cloudImageBBox.topLeft() + dPos);
  m_topLeft += dPos;
  m_bottomRight += dPos;

  suspendRender();
}

void Cloud::onEndMove(QPoint total_d) {
  if (m_canvasPm.isNull()) return;

  MoveCloudUndo* undo =
      dynamic_cast<MoveCloudUndo*>(UndoManager::instance()->tmpCommand());
  if (undo) {
    undo->setOffset(total_d);
    UndoManager::instance()->stack()->push(undo);
    UndoManager::instance()->releaseTmpCommand();
  }
}

void Cloud::autoFill() {
  if (m_canvasPm.isNull() || getBBox().isEmpty()) return;

  QRect bbox       = getBBox();
  QImage canvasImg = m_canvasPm.copy(bbox).toImage();

  for (int y = 0; y < bbox.height(); y++) {
    QRgb* rgb_p = (QRgb*)canvasImg.scanLine(y);
    for (int x = 0; x < bbox.width(); x++, rgb_p++) {
      if (qAlpha(*rgb_p) == 0) *rgb_p = fillColor;
    }
  }

  QSize sceneSize = MyParams::instance()->getSceneSize();

  QList<QPoint> fillPoints;
  for (int x = 0; x < bbox.width(); x++) {
    if (bbox.top() > 0) fillPoints.append(QPoint(x, 0));
    if (bbox.bottom() < sceneSize.height() - 1)
      fillPoints.append(QPoint(x, bbox.height() - 1));
  }
  for (int y = 1; y < bbox.height() - 1; y++) {
    if (bbox.left() > 0) fillPoints.append(QPoint(0, y));
    if (bbox.right() < sceneSize.width() - 1)
      fillPoints.append(QPoint(bbox.width() - 1, y));
  }

  for (int p = 0; p < fillPoints.size(); p++)
    fillLine(canvasImg, fillPoints[p]);

  m_canvasPm.fill(Qt::transparent);
  QPainter painter(&m_canvasPm);
  painter.drawPixmap(bbox.topLeft(), QPixmap::fromImage(canvasImg));
  painter.end();
}

void Cloud::removeErasedStroke(QRectF eraseStrokeRect) {
  if (m_canvasPm.isNull() || getBBox().isEmpty()) return;

  suspendRender();
  MyParams* p = MyParams::instance();
  p->setFlowUndone(Flow_Silhouette2Voxel);

  QRect rect      = eraseStrokeRect.toRect().adjusted(-10, -10, 10, 10);
  QSize sceneSize = MyParams::instance()->getSceneSize();
  rect            = rect.intersected(QRect(QPoint(0, 0), sceneSize));

  QImage strokeImg = m_canvasPm.copy(rect).toImage();

  for (int y = 1; y < rect.height() - 1; y++) {
    for (int x = 1; x < rect.width() - 1; x++) {
      // found the pixel with erase color. 消しゴムピクセルのとき
      if (strokeImg.pixel(x, y) == eraseColor) {
        // check the 4 neighbor pixels. 上下左右のピクセルを走査
        QList<QPoint> neighbors;
        neighbors << QPoint(x, y - 1) << QPoint(x - 1, y) << QPoint(x + 1, y)
                  << QPoint(x, y + 1);
        for (const QPoint& neighborPos : neighbors) {
          // convert fill color to line color to make 1-pixel border.
          // 塗りピクセルなら線ピクセルに変換
          if (strokeImg.pixel(neighborPos) == fillColor)
            strokeImg.setPixel(neighborPos, lineColor);
        }
        // make the erase pixel transparent
        // 自分自身を透明にする
        strokeImg.setPixel(x, y, qRgba(0, 0, 0, 0));
      }
    }
  }

  QPainter painter(&m_canvasPm);
  painter.setCompositionMode(QPainter::CompositionMode_Source);
  painter.drawPixmap(rect.topLeft(), QPixmap::fromImage(strokeImg));
  painter.end();

  // Update the bounding box
  // バウンディングボックスを更新
  QRectF drawingBBox = getDrawingBBox();
  QRect dRect        = drawingBBox.toRect().adjusted(-2, -2, 2, 2);
  dRect              = dRect.intersected(QRect(QPoint(0, 0), sceneSize));
  QImage drawingImg  = m_canvasPm.copy(dRect).toImage();
  // left 左
  bool found = false;
  if (rect.left() <= drawingBBox.left()) {
    for (int x = 0; x < dRect.width(); x++) {
      for (int y = 0; y < dRect.height(); y++)
        if (drawingImg.pixel(x, y) != qRgba(0, 0, 0, 0)) {
          m_topLeft.setX(dRect.left() + x);
          found = true;
          break;
        }
      if (found) break;
    }
    // If any opac pixel is not found, it means that the drawing is totally
    // erased
    // もし全ピクセル透明なら、絵が完全に消されたということ
    if (!found) {
      m_bottomRight = QPointF(0, 0);
      m_topLeft     = QPointF(sceneSize.width(), sceneSize.height());
      return;
    }
  }

  // right 右
  if (rect.right() >= drawingBBox.right()) {
    found = false;
    for (int x = dRect.width() - 1; x >= 0; x--) {
      for (int y = 0; y < dRect.height(); y++)
        if (drawingImg.pixel(x, y) != qRgba(0, 0, 0, 0)) {
          m_bottomRight.setX(dRect.left() + x);
          found = true;
          break;
        }
      if (found) break;
    }
  }

  // top 上
  if (rect.top() <= drawingBBox.top()) {
    found = false;
    for (int y = 0; y < dRect.height(); y++) {
      for (int x = 0; x < dRect.width(); x++)
        if (drawingImg.pixel(x, y) != qRgba(0, 0, 0, 0)) {
          m_topLeft.setY(dRect.top() + y);
          found = true;
          break;
        }
      if (found) break;
    }
  }

  // bottom 下
  if (rect.bottom() >= drawingBBox.bottom()) {
    found = false;
    for (int y = dRect.height() - 1; y >= 0; y--) {
      for (int x = 0; x < dRect.width(); x++)
        if (drawingImg.pixel(x, y) != qRgba(0, 0, 0, 0)) {
          m_bottomRight.setY(dRect.top() + y);
          found = true;
          break;
        }
      if (found) break;
    }
  }
}

//========================================
// Worley Noise

void WorleyNoiseTask_MakeFilter::run() {
  Real dz = m_fromZ - (m_filter->SizeZ() - 1) / 2;
  for (int k = m_fromZ; k < m_toZ; k++, dz += 1.0) {
    Real dy = (Real)(-(m_filter->SizeY() - 1) / 2);
    for (int j = 0; j < m_filter->SizeY(); j++, dy += 1.0) {
      Real dx = (Real)(-(m_filter->SizeX() - 1) / 2);
      for (int i = 0; i < m_filter->SizeX(); i++, dx += 1.0) {
        Real val = QVector3D(dx / m_oblateFilterRadius.x(),
                             dy / m_oblateFilterRadius.y(),
                             dz / m_oblateFilterRadius.z())
                       .length();
        m_filter->SetValue(val, i, j, k);
      }
    }
  }
}

void WorleyNoiseTask::run() {
  MyParams* p = MyParams::instance();

  int sizeX = m_worleyNoise->SizeX();
  int sizeY = m_worleyNoise->SizeY();
  int sizeZ = m_worleyNoise->SizeZ();

  // obtain amount of feature points by deviding pixel size by filter radius
  // size を radius で分割し、WorleyNoiseの基準点の個数を得る
  int dimX = std::ceil((Real)sizeX / (Real)m_oblateFilterRadius.x());
  int dimY = std::ceil((Real)sizeY / (Real)m_oblateFilterRadius.y());
  int dimZ = std::ceil((Real)sizeZ / (Real)m_oblateFilterRadius.z());

  for (int z = m_fromZ; z < m_toZ; z++) {
    int id_Z = z / m_oblateFilterRadius.z();
    for (int y = 0; y < sizeY; y++) {
      int id_Y = y / m_oblateFilterRadius.y();
      for (int x = 0; x < sizeX; x++) {
        int id_X = x / m_oblateFilterRadius.x();

        // compare with 26 neighbor voxels
        // 前後左右26近傍のボクセルの分布を比較する
        for (int vz = id_Z - 1; vz <= id_Z + 1; vz++) {
          if (vz < 0 || vz >= dimZ) continue;
          for (int vy = id_Y - 1; vy <= id_Y + 1; vy++) {
            if (vy < 0 || vy >= dimY) continue;
            for (int vx = id_X - 1; vx <= id_X + 1; vx++) {
              if (vx < 0 || vx >= dimX) continue;

              int* nc = &m_noisePivotCenter[vz * dimY * dimX * 3 +
                                            vy * dimX * 3 + vx * 3];
              int filX = x - nc[0] + std::ceil(m_oblateFilterRadius.x());
              int filY = y - nc[1] + std::ceil(m_oblateFilterRadius.y());
              int filZ = z - nc[2] + std::ceil(m_oblateFilterRadius.z());
              if (!isInRange(filX, 0, m_filter->SizeX()) ||
                  !isInRange(filY, 0, m_filter->SizeY()) ||
                  !isInRange(filZ, 0, m_filter->SizeZ()))
                continue;
              m_worleyNoise->SetMinValue(m_filter->GetValue(filX, filY, filZ),
                                         x, y, z);
            }
          }
        }
      }
    }
  }
}

bool Cloud::generateWorleyNoise() {
  if (getBBox().isEmpty()) return false;
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_GenerateWorlyNoise)) return true;

  // process previous flow, if needed
  if (!silhouette2Voxel()) return false;

  if (isSuspended()) return false;

  startTimer();

  QRect bbox       = getBBox();
  QPointF rayAngle = Sky::posToAngle(bbox.center(), p->getSceneSize());
  Real elevation   = rayAngle.y();

  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());

  int sizeX = bbox.width();
  int sizeY = bbox.height();
  int sizeZ = (int)(std::ceil(m_maxDepth) * 2.0);

  int filterRadius =
      std::max(1, (int)std::round((Real)p->getDouble(ShapeRadiusBase) *
                                  (Real)p->getDouble(WorleyNoiseSizeRatio)));
  Real oblateness         = (Real)p->getDouble(ShapeOblateness);
  unsigned int randomSeed = (unsigned int)p->getInt(WorleyNoiseRandomSeed);
  UniformRandomGenerator01 urg(randomSeed);

  m_worleyNoise      = Volume(sizeX, sizeY, sizeZ, INF);
  int maxThreadCount = QThreadPool::globalInstance()->maxThreadCount();

  // make a filter containing disntance from its center. 距離情報のひな型を作る
  QVector3D oblateFilterRadius(
      filterRadius,
      filterRadius * (1.0 - cos(elevation) * (oblateness - 1) / oblateness),
      filterRadius * (1.0 - sin(elevation) * (oblateness - 1) / oblateness));
  Volume filter(std::ceil(oblateFilterRadius.x()) * 2 + 1,
                std::ceil(oblateFilterRadius.y()) * 2 + 1,
                std::ceil(oblateFilterRadius.z()) * 2 + 1);

  int tmpStart = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int tmpEnd = (int)std::round((float)(filter.SizeZ() * (t + 1)) /
                                 (float)maxThreadCount);
    if (tmpStart == tmpEnd) continue;

    WorleyNoiseTask_MakeFilter* task = new WorleyNoiseTask_MakeFilter(
        tmpStart, tmpEnd, filter, oblateFilterRadius);

    QThreadPool::globalInstance()->start(task);

    tmpStart = tmpEnd;
  }
  QThreadPool::globalInstance()->waitForDone();

  // obtain amount of feature points by deviding pixel size by filter radius
  // size を radius で分割し、WorleyNoiseの基準点の個数を得る
  int dimX              = std::ceil((Real)sizeX / (Real)oblateFilterRadius.x());
  int dimY              = std::ceil((Real)sizeY / (Real)oblateFilterRadius.y());
  int dimZ              = std::ceil((Real)sizeZ / (Real)oblateFilterRadius.z());
  int* noisePivotCenter = new int[dimX * dimY * dimZ * 3];

  // center position of the feature points ノイズ基準点の中心座標
  int* n_p = noisePivotCenter;
  for (int k = 0; k < dimZ; k++) {
    Real baseZ = ((Real)k + 0.5) * (Real)oblateFilterRadius.z();
    for (int j = 0; j < dimY; j++) {
      Real baseY = ((Real)j + 0.5) * (Real)oblateFilterRadius.y();
      for (int i = 0; i < dimX; i++, n_p += 3) {
        Real baseX = ((Real)i + 0.5) * (Real)oblateFilterRadius.x();
        QVector3D rand(urg.GenRand() - 0.5, urg.GenRand() - 0.5,
                       urg.GenRand() - 0.5);
        QVector3D center =
            QVector3D(baseX, baseY, baseZ) + rand * oblateFilterRadius;
        n_p[0] = (int)std::round(center.x());
        n_p[1] = (int)std::round(center.y());
        n_p[2] = (int)std::round(center.z());
      }
    }
  }

  tmpStart = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int tmpEnd =
        (int)std::round((float)(sizeZ * (t + 1)) / (float)maxThreadCount);

    WorleyNoiseTask* task =
        new WorleyNoiseTask(tmpStart, tmpEnd, m_worleyNoise, filter,
                            noisePivotCenter, oblateFilterRadius);

    QThreadPool::globalInstance()->start(task);

    tmpStart = tmpEnd;
  }

  QThreadPool::globalInstance()->waitForDone();

  delete[] noisePivotCenter;

  m_worleyNoise.Normalize();

  p->setFlowDone(Flow_GenerateWorlyNoise);

  printElapsedTime("generateWorlyNoise");
  return false;
}

//========================================
// Simplex Noise

void SimplexNoiseTask::run() {
  int sizeX = m_noise->SizeX();
  int sizeY = m_noise->SizeY();
  int sizeZ = m_noise->SizeZ();

  Real nx, ny, nz;
  Real noiseStep            = 1.0 / m_noiseSize;
  QVector3D oblateNoiseStep = m_oblateNoiseScale * noiseStep;
  // IDEA: you can offset nx, ny, nz to move the noise pattern,
  // or use 4d noise and introduce an evolution
  // ここにオフセットを付けることもできる
  nz = 0.0 + m_fromZ * oblateNoiseStep.z();
  for (int k = m_fromZ; k < m_toZ; k++, nz += oblateNoiseStep.z()) {
    ny = 0.0;
    for (int j = 0; j < sizeY; j++, ny += oblateNoiseStep.y()) {
      nx = 0.0;
      for (int i = 0; i < sizeX; i++, nx += oblateNoiseStep.x()) {
        // return value range is (-0.5, 0.5). -0.5 - 0.5の範囲のノイズ
        Real noiseVal = SimplexNoise::noise(nx, ny, nz);
        m_noise->AddValue(m_intensity * (noiseVal + 0.5), i, j, k);
      }
    }
  }
}

bool Cloud::generateSimplexNoise() {
  if (getBBox().isEmpty()) return false;

  MyParams* p = MyParams::instance();

  if (p->isFlowDone(Flow_GenerateSimplexNoise)) return true;

  // process previous flow, if needed
  if (!silhouette2Voxel()) return false;

  if (isSuspended()) return false;

  startTimer();

  QRect bbox       = getBBox();
  QPointF rayAngle = Sky::posToAngle(bbox.center(), p->getSceneSize());

  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());
  int sizeX                    = bbox.width();
  int sizeY                    = bbox.height();
  int sizeZ                    = (int)(std::ceil(m_maxDepth) * 2.0);

  Real noiseSizeBase = (Real)p->getDouble(ShapeRadiusBase) *
                       (Real)p->getDouble(SimplexNoiseSizeBaseRatio);
  Real noiseSizeRatio      = 0.5;
  Real noiseIntensityRatio = 0.5;
  Real oblateness          = (Real)p->getDouble(ShapeOblateness);
  // oblate noise scale. 扁平状態のノイズのスケール
  QVector3D oblateNoiseScale(
      1.0, 1.0 / (1.0 - cos(rayAngle.y()) * (oblateness - 1) / oblateness),
      1.0 / (1.0 - sin(rayAngle.y()) * (oblateness - 1) / oblateness));
  Real intensity[3] = {1.0, noiseIntensityRatio,
                       noiseIntensityRatio * noiseIntensityRatio};
  // normalize ratios
  Real i_sum = intensity[0] + intensity[1] + intensity[2];
  for (int i = 0; i < 3; i++) intensity[i] /= i_sum;

  m_simplexNoise = Volume(sizeX, sizeY, sizeZ);

  int maxThreadCount = QThreadPool::globalInstance()->maxThreadCount();
  for (int gen = 0; gen < 3; gen++) {
    Real noiseSize = noiseSizeBase * pow(noiseSizeRatio, gen);

    int tmpStart = 0;
    for (int t = 0; t < maxThreadCount; t++) {
      int tmpEnd =
          (int)std::round((float)(sizeZ * (t + 1)) / (float)maxThreadCount);

      SimplexNoiseTask* task = new SimplexNoiseTask(
          tmpStart, tmpEnd, m_simplexNoise, (Real)noiseSize, intensity[gen],
          oblateNoiseScale);

      QThreadPool::globalInstance()->start(task);

      tmpStart = tmpEnd;
    }
    QThreadPool::globalInstance()->waitForDone();
  }

  p->setFlowDone(Flow_GenerateSimplexNoise);

  printElapsedTime("generateSimplexNoise");
  return false;
}

bool Cloud::compositeNoises() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_CompositeNoises)) return true;

  // process previous flow, if needed
  if (!generateWorleyNoise()) return false;
  if (!generateSimplexNoise()) return false;

  assert(m_worleyNoise.GetSize() == m_simplexNoise.GetSize());

  if (isSuspended()) return false;

  startTimer();

  int sizeX = m_worleyNoise.SizeX();
  int sizeY = m_worleyNoise.SizeY();
  int sizeZ = m_worleyNoise.SizeZ();

  Real w_rate = (Real)p->getDouble(WorleyNoiseRate);
  Real s_rate = (Real)p->getDouble(SimplexNoiseRate);

  m_compNoiseVol = Volume(sizeX, sizeY, sizeZ);

  for (int idx = 0; idx < m_worleyNoise.GetSize(); idx++) {
    Real val = (0.5 * (1.0 - w_rate) + m_worleyNoise.GetValue(idx) * w_rate) *
               (0.5 * (1.0 - s_rate) + m_simplexNoise.GetValue(idx) * s_rate);

    m_compNoiseVol.SetValue(val, idx);
  }

  p->setFlowDone(Flow_CompositeNoises);

  printElapsedTime("compositeNoises");
  return false;
}

bool Cloud::silhouette2Voxel() {
  if (m_canvasPm.isNull() || getBBox().isEmpty()) return false;

  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_Silhouette2Voxel)) return true;

  if (isSuspended()) return false;

  startTimer();

  double shapeDepth = p->getDouble(ShapeDepth);
  bool isDepthAuto  = p->getBool(ShapeDepthAuto);
  Real margin       = (Real)p->getInt(ShapeBBoxMargin) / (Real)p->voxelSkip();

  QRect bbox                   = getBBox();
  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());

  QImage canvasImg;
  if (p->voxelSkip() > 1)
    canvasImg = m_canvasPm.scaledToHeight(m_canvasPm.height() / p->voxelSkip())
                    .copy(bbox)
                    .toImage();
  else
    canvasImg = m_canvasPm.copy(bbox).toImage();

  //--- Compute distance image. 距離画像の生成

  // Rambda for initializing depth value. デプス値を初期化するラムダ式
  auto resetDepth = [&](float* depth_p) {
    for (int y = 0; y < bbox.height(); y++) {
      QRgb* rgb_p = (QRgb*)canvasImg.scanLine(y);
      for (int x = 0; x < bbox.width(); x++, rgb_p++, depth_p++) {
        if (qAlpha(*rgb_p) == 0)
          *depth_p = 0.0f;
        else
          *depth_p = INF;
      }
    }
  };

  std::unique_ptr<float[]> depth(new float[bbox.width() * bbox.height()]);
  resetDepth(&depth[0]);
  m_maxDepth = do_distance_transform(&depth[0], bbox.size());

  if (isDepthAuto)
    p->notifyAutoDepthTempValue(m_maxDepth * (Real)p->voxelSkip());
  else
    m_maxDepth = shapeDepth;

  m_maxDepth += margin;

  int sizeX = bbox.width();
  int sizeY = bbox.height();
  int sizeZ = (int)(std::ceil(m_maxDepth) * 2.0);

  m_samplingDensity = Volume(sizeX, sizeY, sizeZ);

  for (int x = 0; x < sizeX; x++) {
    for (int y = 0; y < sizeY; y++) {
      double cloudDepth_front = depth[x + sizeX * y] * (m_maxDepth - margin);
      int z;
      // front(near) side 手前側
      for (z = 0; z < sizeZ / 2; z++) {
        Real dz = (Real)(sizeZ) / 2.0f - (Real)z;

        if (dz < cloudDepth_front)
          m_samplingDensity.SetValue((cloudDepth_front - dz) / m_maxDepth, x, y,
                                     z);
        else
          m_samplingDensity.SetValue(-1.0, x, y, z);
      }
      double cloudDepth_back = depth[x + sizeX * y] * (m_maxDepth - margin);
      // back(far) side 奥側
      for (; z < sizeZ; z++) {
        Real dz = (Real)z - (Real)(sizeZ) / 2.0f;

        if (dz < cloudDepth_back)
          m_samplingDensity.SetValue((cloudDepth_back - dz) / m_maxDepth, x, y,
                                     z);
        else
          m_samplingDensity.SetValue(-1.0, x, y, z);
      }
    }
  }

  p->setFlowDone(Flow_Silhouette2Voxel);

  printElapsedTime("silhouette2Voxel");
  return false;
}

void ArrangeMetaballTask::run() {
  // oblate ball size. 扁平状態のボールサイズ
  QVector3D oblateBallSize(
      m_ballSize,
      m_ballSize * (1.0 - cos(m_elevation) * (m_oblateness - 1) / m_oblateness),
      m_ballSize *
          (1.0 - sin(m_elevation) * (m_oblateness - 1) / m_oblateness));

  auto posToIndex_x = [&](int pos) -> int {
    return (int)std::floor((float)pos / oblateBallSize.x());
  };
  auto posToIndex_y = [&](int pos) -> int {
    return (int)std::floor((float)pos / oblateBallSize.y());
  };
  auto posToIndex_z = [&](int pos) -> int {
    return (int)std::floor((float)pos / oblateBallSize.z());
  };
  auto getSeed = [&](int x, int y, int z) -> unsigned int {
    unsigned int codeX = (unsigned int)((x + 10000) % 256);
    unsigned int codeY = (unsigned int)((y + 10000) % 256);
    unsigned int codeZ = (unsigned int)((z + 10000) % 256);
    unsigned int codeS = m_randomSeed % 256;
    return (codeX << 12) + (codeY << 8) + (codeZ << 4) + codeS;
  };

  // obtain grid index range covered by the volume bounding box. Index can be
  // negative
  // 計算範囲に重なるグリッドのインデックス範囲を得る（負値もありうる）
  int grid_x_min, grid_x_max, grid_y_min, grid_y_max, grid_z_min, grid_z_max;
  grid_x_min = posToIndex_x(m_bbox.left() - m_posOffset.x()) - 1;
  grid_x_max = posToIndex_x(m_bbox.right() - m_posOffset.x());
  grid_y_min = posToIndex_y(m_bbox.top() + m_from - m_posOffset.y()) - 2;
  grid_y_max = posToIndex_y(m_bbox.top() + m_to - m_posOffset.y()) + 1;

  int halfDepth = m_possibleArea->SizeZ() / 2;
  grid_z_min    = posToIndex_z(-halfDepth) - 1;
  grid_z_max    = posToIndex_z(halfDepth);

  int BALL_VARIETY = m_metaballs.size();

  // the current grid position under the volume coordinate
  // (origin is the corner of the volume bounding box)
  // Volume座標系(Volumeの隅が原点)での現在のグリッドの基準点の位置
  Real base_z = (Real)halfDepth + (Real)grid_z_min * oblateBallSize.z();
  // for each grid containing the ball center 各グリッドについて
  for (int gz = grid_z_min; gz <= grid_z_max;
       gz++, base_z += oblateBallSize.z()) {
    // the current grid position under the volume coordinate
    // Volume座標系(Volumeの隅が原点)での現在のグリッドの基準点の位置
    Real base_y = (Real)grid_y_min * oblateBallSize.y() - (Real)m_bbox.top() +
                  (Real)m_posOffset.y();
    for (int gy = grid_y_min; gy <= grid_y_max;
         gy++, base_y += oblateBallSize.y()) {
      // the current grid position under the volume coordinate
      // Volume座標系(Volumeの隅が原点)での現在のグリッドの基準点の位置
      Real base_x = (Real)grid_x_min * oblateBallSize.x() -
                    (Real)m_bbox.left() + (Real)m_posOffset.x();
      for (int gx = grid_x_min; gx <= grid_x_max;
           gx++, base_x += oblateBallSize.x()) {
        // generate random seed from grid indices
        // グリッドのインデックスからランダムシード生成
        unsigned int tmp_seed = getSeed(gx, gy, gz);
        UniformRandomGenerator01 tmp_rand(tmp_seed);
        // compute the center position in the current grid
        // 現在のグリッドのボールの中心座標を得る
        Real centerX = base_x + oblateBallSize.x() * tmp_rand.GenRand();
        Real centerY = base_y + oblateBallSize.y() * tmp_rand.GenRand();
        Real centerZ = base_z + oblateBallSize.z() * tmp_rand.GenRand();
        int cx       = (int)std::floor(centerX);
        int cy       = (int)std::floor(centerY);
        int cz       = (int)std::floor(centerZ);
        // continue if the center position is out of the range
        // 中心位置が範囲外ならcontinue
        if (cx < 0 || cx >= m_possibleArea->SizeX() - 1 || cy < 0 ||
            cy >= m_possibleArea->SizeY() - 1 || cz < 0 ||
            cz >= m_possibleArea->SizeZ() - 1)
          continue;

        // continue if the center position is not in the area with possibleArea
        // > 0.2. linear interpolate the possibleArea value in rough quality
        // mode
        // in order to reproduce the ball arrangement as much as possible.
        // もし中心座標が possibleArea値 > 0.2 の範囲に入っていなければcontinue
        // 低画質モードのときは線形補間を行ってできるだけ玉の生成を再現できるようにする
        if (m_doLerp &&
            m_possibleArea->GetLerpValue(centerX, centerY, centerZ) <= 0.2)
          continue;
        else if (!m_doLerp && m_possibleArea->GetValue(cx, cy, cz) <= 0.2)
          continue;

        // define which size of balls to be put
        // ランダムを回してどのボールを配置するかを決める
        int ballId =
            std::min(BALL_VARIETY - 1,
                     (int)std::floor((Real)BALL_VARIETY * tmp_rand.GenRand()));

        MetaBall ball = m_metaballs.at(ballId);

        // composite ball density, taking the maximum values
        // ボールをResultに最大値合成する

        // bx, by, bz : position in the current ball volume
        // rx, ry, rz : position in the result volume
        // bx, by, bz はこれから配置するボールのball.volume上の座標
        // rx, ry, rz はm_resultVolume上の座標
        int rz = cz - ball.radius;
        for (int bz = 0; bz < ball.filterSize; bz++, rz++) {
          if (rz < 0) continue;
          if (rz >= m_resultVolume->SizeZ()) break;

          int ry = cy - ball.radius;
          for (int by = 0; by < ball.filterSize; by++, ry++) {
            if (ry < m_from) continue;
            if (ry >= m_to) break;

            int rx = cx - ball.radius;
            for (int bx = 0; bx < ball.filterSize; bx++, rx++) {
              if (rx < 0) continue;
              if (rx >= m_resultVolume->SizeX()) break;

              // taking the maximum value 最大値合成
              Real ballVal = ball.volume.GetValue(bx, by, bz);
              if (ballVal <= 0.0) continue;
              m_resultVolume->SetMaxValue(ballVal, rx, ry, rz);

            }  // bx
          }    // by
        }      // bz

      }  // gx
    }    // gy
  }      // gz
}

bool Cloud::arrangeMetaballs() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_ArrangeMetaballs)) return true;

  // process previous flow, if needed
  if (!silhouette2Voxel()) return false;

  // qApp->processEvents();
  if (isSuspended()) return false;

  startTimer();

  Real radiusBase = (Real)p->getDouble(ShapeRadiusBase);
  // (base +- random)倍で分布
  Real radiusRandom      = (Real)p->getDouble(ShapeRadiusRandom);
  Real radiusRatio       = (Real)p->getDouble(ShapeRadiusRatio);
  Real intensityRatio    = (Real)p->getDouble(ShapeIntensityRatio);
  Real followingAccuracy = (Real)p->getDouble(ShapeFollowingAccuracy);
  int randomSeed         = p->getInt(ShapeRandomSeed);
  Real oblateness        = (Real)p->getDouble(ShapeOblateness);

  std::vector<Real> radii = {radiusBase, radiusBase * radiusRatio,
                             radiusBase * radiusRatio * radiusRatio};
  std::vector<Real> intensities = {1.0f, intensityRatio,
                                   intensityRatio * intensityRatio};
  Real i_sum = intensities[0] + intensities[1] + intensities[2];
  for (Real& i : intensities) i /= i_sum;

  int sizeX = m_samplingDensity.SizeX();
  int sizeY = m_samplingDensity.SizeY();
  int sizeZ = m_samplingDensity.SizeZ();

  // generate metaball mask randomly
  m_metaballVol = Volume(sizeX, sizeY, sizeZ);

  const int BALL_VARIETY = 5;
  int maxThreadCount     = QThreadPool::globalInstance()->maxThreadCount();
  QRect bbox             = getBBox();
  QPointF rayAngle       = Sky::posToAngle(bbox.center(), p->getSceneSize());
  // upward vector 鉛直方向のベクトル
  QVector3D vecVertical(0.0, -cos(rayAngle.y()), sin(rayAngle.y()));

  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());

  Volume possibleArea(m_samplingDensity);

  int gen_max = 3;
  for (int gen = 0; gen < gen_max; gen++) {
    QList<MetaBall> metaballs;

    for (int r = 0; r < BALL_VARIETY; r++) {
      MetaBall ball;

      Real sizeRatio = 1.0 - radiusRandom +
                       (Real)r * 2.0 * radiusRandom / (Real)BALL_VARIETY;
      Real rad        = radii[gen] * sizeRatio;
      ball.radius     = (int)std::ceil(rad);
      ball.filterSize = ball.radius * 2 + 1;
      // Volume is initialized with 0. 0で初期化される
      ball.volume = Volume(ball.filterSize, ball.filterSize, ball.filterSize);
      int idx     = 0;
      Real rad2   = rad * rad;

      for (int fz = -ball.radius; fz <= ball.radius; fz++) {
        for (int fy = -ball.radius; fy <= ball.radius; fy++) {
          for (int fx = -ball.radius; fx <= ball.radius; fx++, idx++) {
            QVector3D pos(fx, fy, fz);
            float v = QVector3D::dotProduct(vecVertical, pos);
            pos += vecVertical * v * (oblateness - 1.0);
            Real dist2 = pos.lengthSquared();
            if (dist2 < rad2) {
              Real maskVal = std::min(1.0, 0.5 + (rad2 - dist2) / rad2);
              ball.volume.SetValue(maskVal, idx);
            }
          }  // fx
        }    // fy
      }      // fz

      metaballs.append(ball);
    }

    Volume tmpMetaballVolume(sizeX, sizeY, sizeZ);

    int yStart = 0;
    for (int t = 0; t < maxThreadCount; t++) {
      int yEnd = (int)std::round((float)(bbox.height() * (t + 1)) /
                                 (float)maxThreadCount);

      ArrangeMetaballTask* task = new ArrangeMetaballTask(
          yStart, yEnd, bbox, possibleArea, tmpMetaballVolume, radii[gen],
          metaballs, randomSeed, m_posOffset, (p->voxelSkip() > 1), oblateness,
          rayAngle.y());

      QThreadPool::globalInstance()->start(task);

      yStart = yEnd;
    }

    QThreadPool::globalInstance()->waitForDone();

    // add current generation to the result volume.
    // メタボール濃度マップに足しこむ
    for (int idx = 0; idx < sizeX * sizeY * sizeZ; idx++)
      m_metaballVol.AddValue(tmpMetaballVolume.GetValue(idx) * intensities[gen],
                             idx);
    if (gen < 2) {
      if (followingAccuracy == 0.0)
        possibleArea = tmpMetaballVolume;
      else if (followingAccuracy < 1.0)
        possibleArea =
            m_samplingDensity.lerpWith(tmpMetaballVolume, followingAccuracy);
    }

  }  // gen

  p->setFlowDone(Flow_ArrangeMetaballs);

  printElapsedTime("arrangeMetaballs");
  return false;
}

// calc intersection with noise
bool Cloud::compositeMetaballAndNoise() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_CompositeMetaballAndNoise)) return true;

  // process previous flow, if needed
  if (!arrangeMetaballs()) return false;
  if (!compositeNoises()) return false;

  if (isSuspended()) return false;

  startTimer();

  double camElev = p->getDouble(CameraElevation) * M_PI / 180.0;

  int sizeX = m_metaballVol.SizeX();
  int sizeY = m_metaballVol.SizeY();
  int sizeZ = m_metaballVol.SizeZ();

  // downward direction in the voxel space
  // ボクセル空間の下方向のベクトル
  QVector3D downVec(0.0, cos(camElev), -sin(camElev));

  // length from the center of the bounding box to the bottom of the box
  // ボクセル空間のボックス中心から下端までの距離
  double bottom = QVector3D::dotProduct(
      downVec, QVector3D(0.0, (Real)sizeY * 0.5, -(Real)sizeZ * 0.5));

  double bottomCutOff     = 1.0 - p->getDouble(ShapeBottomCutOff);
  double bottomCutOffBlur = p->getDouble(ShapeBottomCutOffBlur);
  double noisePrevalence  = p->getDouble(NoisePrevalence);

  m_cloudVol = Volume(sizeX, sizeY, sizeZ);

  int idx = 0;
  for (int z = 0; z < sizeZ; z++) {
    double zPos = (Real)z - (Real)sizeZ * 0.5;
    for (int y = 0; y < sizeY; y++) {
      double yPos = (Real)y - (Real)sizeY * 0.5;

      double downPos =
          QVector3D::dotProduct(downVec, QVector3D(0.0, yPos, zPos));
      // downRatio range is (-1, 1). 1 means bottommost. -1 ～ 1 の範囲。
      // 1が一番下
      double downRatio = downPos / bottom;
      double cutOffRatio =
          (downRatio <= bottomCutOff - bottomCutOffBlur)
              ? 1.0
              : (downRatio >= bottomCutOff + bottomCutOffBlur)
                    ? 0.0
                    : (bottomCutOff + bottomCutOffBlur - downRatio) /
                          (2.0 * bottomCutOffBlur);

      for (int x = 0; x < sizeX; x++, idx++) {
        Real metaball = m_metaballVol.GetValue(idx) * cutOffRatio;
        if (metaball > 0.0) {
          Real noise = m_compNoiseVol.GetValue(idx);
          Real samplingDensity =
              std::max(0.0f, m_samplingDensity.GetValue(idx));
          // considering noise prevaleence. noiseの深さ
          samplingDensity = samplingDensity / noisePrevalence;
          Real v          = metaball *
                   (samplingDensity + 4.0 * noise * (1.0 - samplingDensity));
          m_cloudVol.SetValue(std::min(1.0f, std::max(0.0f, v)), idx);
        } else
          m_cloudVol.SetValue(0.0, idx);
      }
    }
  }

  p->setFlowDone(Flow_CompositeMetaballAndNoise);

  printElapsedTime("compositeMetaballAndNoise");
  return false;
}

// Rendering
void RayMarchingRenderTask::run() {
  MyParams* p  = MyParams::instance();
  Real rayStep = (Real)p->getDouble(RenderRayStep);
  // 減弱係数
  Real attenuationFactor = rayStep / 2.0f;
  Real luminanceRatio    = rayStep / 2.0f;

  // scatteringFormFactor is used as the variable k in The Schlick Appoximation
  // Phase Function.
  // k ∈ [−1,1] controls the preferential scattering direction.
  // total backward scattering is obtained with k = −1, k = 1 gives total
  // forward scattering, and k = 0 corresponds to isotropic scattering.
  // 散乱角をschlick近似する関数のパラメータ
  Real scatteringFormFactor = 0.6f;

  Real cloudDensity = (Real)p->getDouble(RenderCloudDensity);
  QSize viewerSize  = p->getSceneSize();

  if (p->voxelSkip() > 1) {
    attenuationFactor *= (Real)p->voxelSkip();
    luminanceRatio *= (Real)p->voxelSkip();
    cloudDensity *= (Real)p->voxelSkip();
    viewerSize /= p->voxelSkip();
  }

  int sizeX = m_cloudVol->SizeX();
  int sizeY = m_cloudVol->SizeY();
  int sizeZ = m_cloudVol->SizeZ();

  // compute an affine matrix for converting global coordinate to volume
  // coordinate
  // ここで、レイ座標をボリューム座標に変換するアフィン行列を作成
  double camFov = p->getDouble(CameraFov) * M_PI / 180.0;

  QPointF rayAngle = Sky::posToAngle(m_bbox.center(), viewerSize);

  Real cos_Azimuth = cos(rayAngle.x());
  Real sin_Azimuth = sin(rayAngle.x());
  Real cos_Elev    = cos(rayAngle.y());
  Real sin_Elev    = sin(rayAngle.y());

  Real k = 2.0 * tan(camFov / 2.0) / (Real)viewerSize.height();
  QVector3D dx(k * cos_Azimuth, -k * sin_Azimuth, 0.0);
  QVector3D dy(k * sin_Azimuth, k * cos_Azimuth * sin_Elev, -k * cos_Elev);
  QVector3D dz(k * cos_Elev * sin_Azimuth, k * cos_Elev * cos_Azimuth,
               k * sin_Elev);

  QMatrix4x4 rayToVolumeAff(dx.x(), dx.y(), dx.z(), 0.0, dy.x(), dy.y(), dy.z(),
                            0.0, dz.x(), dz.y(), dz.z(), 0.0, 0.0, 0.0, 0.0,
                            1.0);

  bool useUniqueSun = p->getBool(SunUseUniqueAngles);
  ParamId sunElevId = (useUniqueSun) ? SunUniqueElevation : SunElevation;
  ParamId sunAzimId = (useUniqueSun) ? SunUniqueAzimuth : SunAzimuth;
  double sunElev    = p->getDouble(sunElevId) * M_PI / 180.0;
  double sunAzimuth = p->getDouble(sunAzimId) * M_PI / 180.0;

  UniformRandomGenerator01 rand(311, m_from * sizeX);

  int idx = m_from * sizeX;
  for (int v = m_from; v < m_to; v++) {
    for (int u = 0; u < sizeX; u++, idx++) {
      // unit eye vector 視線の単位ベクトル
      QPointF posOnScreen    = QPoint(u, v) + m_bbox.topLeft();
      QVector3D eyeDirection = Sky::posToVec(posOnScreen, viewerSize);
      QVector3D rayStepVec =
          rayToVolumeAff.map(eyeDirection).normalized() * rayStep;

      Real offsetU = -rayStepVec.x() * (Real)sizeZ * 0.5 / rayStepVec.z();
      Real offsetV = -rayStepVec.y() * (Real)sizeZ * 0.5 / rayStepVec.z();
      // current position of the ray. レイ現在位置
      QVector3D targetPos((Real)u + offsetU, (Real)v + offsetV, 0.0);

      bool isTracing                = true;
      Real luminance                = 0.0;
      Real eyeToTargetAttenuation   = 0.0;
      Real targetToLightAttenuation = 0.0;
      int step                      = 0;

      while (isTracing) {
        // 0. move ray forward
        targetPos = targetPos + rayStepVec;

        // boundary condition. 境界条件
        if (!isInRange((int)targetPos.z(), 0, sizeZ)) {
          break;
        }

        if (!isInRange((int)targetPos.x(), 0, sizeX) ||
            !isInRange((int)targetPos.y(), 0, sizeY)) {
          continue;
        }
        // 1. accumulate attenuation coef of current position to eye
        Real volumeValue = m_cloudVol->GetValue(
            (int)targetPos.x(), (int)targetPos.y(), (int)targetPos.z());

        if (volumeValue == 0.0) continue;

        eyeToTargetAttenuation += volumeValue * attenuationFactor;
        targetToLightAttenuation = 0.0;
        // 2. calc attenuation coef of current position to light source
        bool isInsideVolume     = true;
        QVector3D scatterRayPos = targetPos;

        // Add fluctuation to sun direction in order to reduce parallel-shaped
        // shadow artifact
        // 太陽光をピクセルごとに散らして、平行カゲのアーティファクトを無くす
        Real rand_range = 5.0;
        double sunElev_mod =
            sunElev + rand_range * (rand.GenRand() - 0.5) * M_PI / 180.0;
        double sunAzimuth_mod =
            sunAzimuth + rand_range * (rand.GenRand() - 0.5) * M_PI / 180.0;
        QVector3D sunVec(cos(sunElev_mod) * sin(sunAzimuth_mod),
                         cos(sunElev_mod) * cos(sunAzimuth_mod),
                         sin(sunElev_mod));
        QVector3D scatterRayStepVec =
            rayToVolumeAff.map(sunVec).normalized() * rayStep;

        while (isInsideVolume) {
          scatterRayPos = scatterRayPos + scatterRayStepVec;

          if (!isInRange((int)scatterRayPos.x(), 0, sizeX) ||
              !isInRange((int)scatterRayPos.y(), 0, sizeY) ||
              !isInRange((int)scatterRayPos.z(), 0, sizeZ)) {
            break;
          }
          targetToLightAttenuation +=
              m_cloudVol->GetValue((int)scatterRayPos.x(),
                                   (int)scatterRayPos.y(),
                                   (int)scatterRayPos.z()) *
              attenuationFactor;
        }

        // 3. calc scattering angle cross section
        Real theta = acos(QVector3D::dotProduct(sunVec, eyeDirection));
        Real scatterRatio =
            schlickScatteringAngle(theta, scatteringFormFactor) * volumeValue;

        // 4. calc and accumulate luminance
        luminance += 5.0 * scatterRatio *
                     exp(-eyeToTargetAttenuation * cloudDensity -
                         targetToLightAttenuation * cloudDensity) *
                     luminanceRatio;
      }  // end tracing

      Real opacity = exp(-eyeToTargetAttenuation * cloudDensity);

      // unmultiply ここでdepremultiplyする
      luminance /= (1.0 - opacity);

      (*m_luminances)[idx] = luminance;
      (*m_opacities)[idx]  = opacity;

      // update maximum luminance
      *m_threadMaxLuminance = std::max(*m_threadMaxLuminance, luminance);

    }  // u loop
  }    // v loop
}

bool Cloud::rayMarchingRender() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_RayMarchingRender)) return true;

  // process previous flow, if needed
  if (!compositeMetaballAndNoise()) return false;

  if (isSuspended()) return false;

  startTimer();

  QRect bbox                   = getBBox();
  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());
  m_luminances.clear();
  m_opacities.clear();
  for (int i = 0; i < bbox.width() * bbox.height(); i++) {
    m_luminances.append(0.0);
    m_opacities.append(0.0);
  }

  int maxThreadCount = QThreadPool::globalInstance()->maxThreadCount();

  float* threadMaxLuminance = new float[maxThreadCount];
  int tmpStart              = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int tmpEnd = (int)std::round((float)(bbox.height() * (t + 1)) /
                                 (float)maxThreadCount);
    threadMaxLuminance[t] = -1;

    RayMarchingRenderTask* task = new RayMarchingRenderTask(
        tmpStart, tmpEnd, m_cloudVol, bbox, m_luminances, m_opacities,
        &threadMaxLuminance[t]);

    QThreadPool::globalInstance()->start(task);

    tmpStart = tmpEnd;
  }

  QThreadPool::globalInstance()->waitForDone();

  // obtain maximum luminance
  float maxLuminance = -1;
  for (int t = 0; t < maxThreadCount; t++) {
    maxLuminance = std::max(maxLuminance, threadMaxLuminance[t]);
  }
  delete[] threadMaxLuminance;

  // std::cout << "max luminance = " << maxLuminance << std::endl;

  // normalize luminance to 0-1
  if (maxLuminance > 0) {
    for (Real& lum : m_luminances) {
      lum /= maxLuminance;
    }
  }

  p->setFlowDone(Flow_RayMarchingRender);

  printElapsedTime("rayMarchingRender");

  return false;
}

void RenderReflectionTask::run() {
  MyParams* p               = MyParams::instance();
  Real rayStep              = (Real)p->getDouble(RenderRayStep);
  Real attenuationFactor    = rayStep / 2.0f;
  Real luminanceRatio       = rayStep / 2.0f;
  Real scatteringFormFactor = 0.6f;
  Real cloudDensity         = (Real)p->getDouble(RenderCloudDensity);
  QSize viewerSize          = p->getSceneSize();

  if (p->voxelSkip() > 1) {
    attenuationFactor *= (Real)p->voxelSkip();
    luminanceRatio *= (Real)p->voxelSkip();
    cloudDensity *= (Real)p->voxelSkip();
    viewerSize /= p->voxelSkip();
  }

  int sizeX = m_cloudVol->SizeX();
  int sizeY = m_cloudVol->SizeY();
  int sizeZ = m_cloudVol->SizeZ();

  double camFov    = p->getDouble(CameraFov) * M_PI / 180.0;
  QPointF rayAngle = Sky::posToAngle(m_bbox.center(), viewerSize);

  Real cos_Azimuth = cos(rayAngle.x());
  Real sin_Azimuth = sin(rayAngle.x());
  Real cos_Elev    = cos(rayAngle.y());
  Real sin_Elev    = sin(rayAngle.y());

  Real k = 2.0 * tan(camFov / 2.0) / (Real)viewerSize.height();
  QVector3D dx(k * cos_Azimuth, -k * sin_Azimuth, 0.0);
  QVector3D dy(k * sin_Azimuth, k * cos_Azimuth * sin_Elev, -k * cos_Elev);
  QVector3D dz(k * cos_Elev * sin_Azimuth, k * cos_Elev * cos_Azimuth,
               k * sin_Elev);

  QMatrix4x4 rayToVolumeAff(dx.x(), dx.y(), dx.z(), 0.0, dy.x(), dy.y(), dy.z(),
                            0.0, dz.x(), dz.y(), dz.z(), 0.0, 0.0, 0.0, 0.0,
                            1.0);

  bool useUniqueSun = p->getBool(SunUseUniqueAngles);
  ParamId sunElevId = (useUniqueSun) ? SunUniqueElevation : SunElevation;
  ParamId sunAzimId = (useUniqueSun) ? SunUniqueAzimuth : SunAzimuth;
  // light from the ground
  double sunElev    = -90.0 * M_PI / 180.0;
  double sunAzimuth = 0.0 * M_PI / 180.0;

  UniformRandomGenerator01 rand(311, m_from * sizeX);

  int idx = m_from * sizeX;
  for (int v = m_from; v < m_to; v++) {
    for (int u = 0; u < sizeX; u++, idx++) {
      // skip transparent pixels
      if ((*m_opacities)[idx] == 1.0f) continue;

      QPointF posOnScreen    = QPoint(u, v) + m_bbox.topLeft();
      QVector3D eyeDirection = Sky::posToVec(posOnScreen, viewerSize);
      QVector3D rayStepVec =
          rayToVolumeAff.map(eyeDirection).normalized() * rayStep;

      Real offsetU = -rayStepVec.x() * (Real)sizeZ * 0.5 / rayStepVec.z();
      Real offsetV = -rayStepVec.y() * (Real)sizeZ * 0.5 / rayStepVec.z();
      QVector3D targetPos((Real)u + offsetU, (Real)v + offsetV, 0.0);

      bool isTracing                = true;
      Real luminance                = 0.0;
      Real eyeToTargetAttenuation   = 0.0;
      Real targetToLightAttenuation = 0.0;
      int step                      = 0;

      while (isTracing) {
        // 0. move ray forward
        targetPos = targetPos + rayStepVec;

        if (!isInRange((int)targetPos.z(), 0, sizeZ)) {
          break;
        }

        if (!isInRange((int)targetPos.x(), 0, sizeX) ||
            !isInRange((int)targetPos.y(), 0, sizeY)) {
          continue;
        }
        // 1. accumulate attenuation coef of current position to eye

        Real volumeValue = m_cloudVol->GetValue(
            (int)targetPos.x(), (int)targetPos.y(), (int)targetPos.z());

        if (volumeValue == 0.0) continue;

        eyeToTargetAttenuation += volumeValue * attenuationFactor;
        targetToLightAttenuation = 0.0;
        // 2. calc attenuation coef of current position to light source
        bool isInsideVolume     = true;
        QVector3D scatterRayPos = targetPos;

        Real rand_range = 5.0;
        double sunElev_mod =
            sunElev + rand_range * (rand.GenRand() - 0.5) * M_PI / 180.0;
        double sunAzimuth_mod =
            sunAzimuth + rand_range * (rand.GenRand() - 0.5) * M_PI / 180.0;
        QVector3D sunVec(cos(sunElev_mod) * sin(sunAzimuth_mod),
                         cos(sunElev_mod) * cos(sunAzimuth_mod),
                         sin(sunElev_mod));

        QVector3D scatterRayStepVec =
            rayToVolumeAff.map(sunVec).normalized() * rayStep;

        while (isInsideVolume) {
          scatterRayPos = scatterRayPos + scatterRayStepVec;

          if (!isInRange((int)scatterRayPos.x(), 0, sizeX) ||
              !isInRange((int)scatterRayPos.y(), 0, sizeY) ||
              !isInRange((int)scatterRayPos.z(), 0, sizeZ)) {
            break;
          }
          targetToLightAttenuation +=
              m_cloudVol->GetValue((int)scatterRayPos.x(),
                                   (int)scatterRayPos.y(),
                                   (int)scatterRayPos.z()) *
              attenuationFactor;
        }

        // 3. calc scattering angle cross section
        Real theta = acos(QVector3D::dotProduct(sunVec, eyeDirection));
        Real scatterRatio =
            schlickScatteringAngle(theta, scatteringFormFactor) * volumeValue;

        // 4. calc and accumulate luminance
        luminance += 5.0 * scatterRatio *
                     exp(-eyeToTargetAttenuation * cloudDensity -
                         targetToLightAttenuation * cloudDensity) *
                     luminanceRatio;
      }  // end tracing

      // update maximum luminance - note that the maximum value is obtained
      // before unpremultiplying
      *m_threadMaxLuminance = std::max(*m_threadMaxLuminance, luminance);

      // unpremultiply
      luminance /= (1.0 - (*m_opacities)[idx]);

      (*m_luminances)[idx] = luminance;

      // update maximum luminance
      //*m_threadMaxLuminance = std::max(*m_threadMaxLuminance, luminance);

    }  // u loop
  }    // v loop
}

bool Cloud::renderReflection() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_RenderReflection)) return true;

  // process previous flow, if needed
  if (!rayMarchingRender()) return false;

  if (isSuspended()) return false;

  // if reflection color is transparent, then do nothing
  QColor refColor = p->getColor(RenderReflectionColor);
  double refAlpha = p->getDouble(RenderReflectionAlpha);
  if (refAlpha == 0.0) {
    p->setFlowDone(Flow_RenderReflection);
    return true;
  }

  startTimer();

  QRect bbox                   = getBBox();
  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());
  m_refLuminances.clear();
  for (int i = 0; i < bbox.width() * bbox.height(); i++) {
    m_refLuminances.append(0.0);
  }

  int maxThreadCount = QThreadPool::globalInstance()->maxThreadCount();

  float* threadMaxLuminance = new float[maxThreadCount];
  int tmpStart              = 0;
  for (int t = 0; t < maxThreadCount; t++) {
    int tmpEnd = (int)std::round((float)(bbox.height() * (t + 1)) /
                                 (float)maxThreadCount);
    threadMaxLuminance[t] = -1;

    RenderReflectionTask* task = new RenderReflectionTask(
        tmpStart, tmpEnd, m_cloudVol, bbox, m_refLuminances, m_opacities,
        &threadMaxLuminance[t]);

    QThreadPool::globalInstance()->start(task);

    tmpStart = tmpEnd;
  }

  QThreadPool::globalInstance()->waitForDone();

  // obtain maximum luminance
  float maxLuminance = -1;
  for (int t = 0; t < maxThreadCount; t++) {
    maxLuminance = std::max(maxLuminance, threadMaxLuminance[t]);
  }
  delete[] threadMaxLuminance;

  // std::cout << "max reflection luminance = " << maxLuminance << std::endl;

  // normalize luminance to 0-1
  if (maxLuminance > 0) {
    for (Real& lum : m_refLuminances) {
      lum /= maxLuminance;
      // gain contrast
      lum = (lum < 0.5f) ? 0 : (lum - 0.5f) * 2;
    }
  }

  p->setFlowDone(Flow_RenderReflection);

  printElapsedTime("renderReflection");

  return false;
}

QImage& Cloud::getCloudImage() {
  MyParams* p = MyParams::instance();
  if (p->isFlowDone(Flow_GetCloudImage) || !m_render || getBBox().isEmpty())
    return m_cloudImage;

  // process previous flow, if needed
  if (!renderReflection()) return m_cloudImage;

  if (isSuspended()) return m_cloudImage;

  startTimer();

  ColorGradient colorGradient = p->getColorGradient(RenderToneMap);
  QColor refColor             = p->getColor(RenderReflectionColor);
  QVector3D refColorVec(refColor.redF(), refColor.greenF(), refColor.blueF());
  double refAlpha = p->getDouble(RenderReflectionAlpha);
  bool hasRef     = refAlpha > 0.0;

  QRect bbox                   = getBBox();
  if (p->voxelSkip() > 1) bbox = getSkippedRect(bbox, p->voxelSkip());

  m_cloudImage = QImage(bbox.size(), QImage::Format_ARGB32);

  QImage bgImage = Sky::instance()->getCombinedBackground();
  if (p->voxelSkip() > 1)
    bgImage =
        bgImage.scaledToHeight(bgImage.height() / p->voxelSkip()).copy(bbox);
  else
    bgImage = bgImage.copy(bbox);

  Real aerialPerspectiveRatio = p->getDouble(RenderAerialRatio);

  for (int y = 0; y < bbox.height(); y++) {
    QRgb* rgb_p   = (QRgb*)m_cloudImage.scanLine(y);
    QRgb* bgImg_p = (QRgb*)bgImage.scanLine(y);
    for (int x = 0; x < bbox.width(); x++, rgb_p++, bgImg_p++) {
      Real alpha = 1.0 - m_opacities[y * bbox.width() + x];
      if (alpha == 0.0) {
        *rgb_p = qRgba(0, 0, 0, 0);
        continue;
      }
      Real lum = m_luminances[y * bbox.width() + x];
      // the brighter the luminance, the lower the mixing ratio with sky color
      // ハイライトほど空との混合率を下げる
      Real aerial     = aerialPerspectiveRatio * (1.0 - lum);
      QVector3D color = colorGradient.getColorVecAt(lum);
      if (hasRef && lum * alpha < 0.05) {
        float refMatte =
            std::min(1.0, m_refLuminances[y * bbox.width() + x] * refAlpha) *
            (0.05 - lum * alpha) / 0.05;
        color = refColorVec * refMatte + color * (1.0 - refMatte);
      }
      // if (hasRef /*&& lum*alpha < 0.01*/) {
      //  float refMatte = std::min(1.0, m_refLuminances[y * bbox.width() + x] *
      //  refAlpha) /* * (0.01 - lum*alpha)/0.01 */;
      //  color = refColorVec * refMatte + color * (1.0 - refMatte);
      //}
      color = color * 255.0 * alpha;
      color.setX(color.x() * (1.0 - aerial) / alpha +
                 (Real)qRed(*bgImg_p) * aerial);
      color.setY(color.y() * (1.0 - aerial) / alpha +
                 (Real)qGreen(*bgImg_p) * aerial);
      color.setZ(color.z() * (1.0 - aerial) / alpha +
                 (Real)qBlue(*bgImg_p) * aerial);
      *rgb_p = qRgba((int)std::min(color.x(), 255.0f),
                     (int)std::min(color.y(), 255.0f),
                     (int)std::min(color.z(), 255.0f),
                     (int)std::min(alpha * 255.0f, 255.0f));
    }
  }

  // update bbox
  // 計算が完了したらbboxで更新する ※ Skip無しの値
  m_cloudImageBBox = getBBox();

  p->setFlowDone(Flow_GetCloudImage);

  printElapsedTime("getCloudImage");

  // notify for updating layerview icon. layerViewのアイコン更新のため
  p->notifyCloudImageRendered();

  return m_cloudImage;
}

bool Cloud::isCloudDone() {
  MyParams* p = MyParams::instance();
  return m_render && p->isFlowDone(Flow_GetCloudImage);
}

// rendering is triggered but unfinished
bool Cloud::isCloudProcessing() {
  MyParams* p = MyParams::instance();
  return m_render && !p->isFlowDone(Flow_GetCloudImage);
}

void Cloud::saveData(QSettings& settings, bool forPreset) {
  MyParams* p = MyParams::instance();

  QMap<ParamId, QVariant>::const_iterator i = m_params.constBegin();
  while (i != m_params.constEnd()) {
    Param param = p->getParam(i.key());
    settings.setValue(param.idString, i.value());
    ++i;
  }

  if (forPreset) return;

  settings.setValue("BoundingTopLeft", m_topLeft);
  settings.setValue("BoundingBottomRight", m_bottomRight);
  settings.setValue("PosOffset", m_posOffset);
  settings.setValue("Name", m_name);

  // save canvas image
  QString fileName = settings.fileName();
  fileName.chop(4);
  fileName.append("_" + settings.group() + ".png");
  m_canvasPm.save(fileName, "png");
}

void Cloud::loadData(QSettings& settings) {
  MyParams* p = MyParams::instance();

  for (QString& key : settings.childKeys()) {
    ParamId pId = p->getParamIdFromIdString(key);
    if (pId == InvalidParam) continue;
    if (p->isCloudParam((ParamId)pId)) {
      m_params[pId] = settings.value(key);
    }
  }

  m_topLeft     = settings.value("BoundingTopLeft").toPointF();
  m_bottomRight = settings.value("BoundingBottomRight").toPointF();
  m_posOffset   = settings.value("PosOffset", QPoint()).toPoint();

  // load canvas image
  QString fileName = settings.fileName();
  fileName.chop(4);
  fileName.append("_" + settings.group() + ".png");
  m_canvasPm.load(fileName, "png");
}

void Cloud::releaseBuffers() {
  m_samplingDensity = Volume();
  m_metaballVol     = Volume();
  m_worleyNoise     = Volume();
  m_simplexNoise    = Volume();
  m_compNoiseVol    = Volume();
  m_cloudVol        = Volume();

  m_luminances.clear();
  m_opacities.clear();
}