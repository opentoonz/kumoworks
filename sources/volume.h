#pragma once
#define _USE_MATH_DEFINES

#include <cassert>
#include <cstdio>
#include <vector>
#include <iostream>
#include <cmath>
#include <algorithm>

typedef float Real;

class Volume {
public:
  Volume() {}

  Volume(int sizeX_in, int sizeY_in, int sizeZ_in, Real init_val = 0.0)
      : sizeX(sizeX_in), sizeY(sizeY_in), sizeZ(sizeZ_in) {
    int size = sizeX * sizeY * sizeZ;
    values.resize(size, init_val);
  }

  Volume(int sizeX_in, int sizeY_in, int sizeZ_in, std::vector<Real>& values_in)
      : sizeX(sizeX_in), sizeY(sizeY_in), sizeZ(sizeZ_in) {
    int size = sizeX * sizeY * sizeZ;
    assert(size == values_in.size());
    values = values_in;
  }

  void SetValue(Real v, int x, int y, int z) {
    values[x + sizeX * y + sizeX * sizeY * z] = v;
  }

  void SetValue(Real v, int idx) { values[idx] = v; }

  void AddValue(Real v, int x, int y, int z) {
    values[x + sizeX * y + sizeX * sizeY * z] += v;
  }

  void AddValue(Real v, int idx) { values[idx] += v; }

  void SetMaxValue(Real v, int x, int y, int z) {
    if (values[x + sizeX * y + sizeX * sizeY * z] < v)
      values[x + sizeX * y + sizeX * sizeY * z] = v;
  }

  void SetMinValue(Real v, int x, int y, int z) {
    if (values[x + sizeX * y + sizeX * sizeY * z] > v)
      values[x + sizeX * y + sizeX * sizeY * z] = v;
  }

  Real GetValue(int x, int y, int z) const {
    return values[x + sizeX * y + sizeX * sizeY * z];
  }

  Real GetValue(int idx) const { return values[idx]; }

  std::vector<Real> GetValues() const { return values; }

  void SetValues(std::vector<Real> v_in) { values = v_in; }

  int GetSize() const { return sizeX * sizeY * sizeZ; }

  int SizeX() { return sizeX; }

  int SizeY() { return sizeY; }

  int SizeZ() { return sizeZ; }

  Volume Normalize() {
    Real tmpMax = 0.0;
    for (auto v : values) {
      tmpMax = std::max(tmpMax, v);
    }
    if (tmpMax != 0.0) {
      for (auto& v : values) {
        v /= tmpMax;
      }
    }
    return *this;
  }

  Volume operator+(const Volume& rhs) {
    assert(this->sizeX == rhs.sizeX && this->sizeY == rhs.sizeY &&
           this->sizeZ == rhs.sizeZ);

    int size = sizeX * sizeY * sizeZ;
    std::vector<Real> ret(size);
    for (int i = 0; i < size; i++) {
      ret[i] = this->GetValue(i) + rhs.GetValue(i);
    }
    return Volume(sizeX, sizeY, sizeZ, ret);
  }

  Volume operator-(const Volume& rhs) {
    assert(this->sizeX == rhs.sizeX && this->sizeY == rhs.sizeY &&
           this->sizeZ == rhs.sizeZ);

    int size = sizeX * sizeY * sizeZ;
    std::vector<Real> ret(size);
    for (int i = 0; i < size; i++) {
      ret[i] = this->GetValue(i) - rhs.GetValue(i);
    }
    return Volume(sizeX, sizeY, sizeZ, ret);
  }

  Volume EwiseProduct(const Volume& rhs) {
    assert(this->sizeX == rhs.sizeX && this->sizeY == rhs.sizeY &&
           this->sizeZ == rhs.sizeZ);

    int size = sizeX * sizeY * sizeZ;
    std::vector<Real> ret(size);
    for (int i = 0; i < size; i++) {
      ret[i] = this->GetValue(i) * rhs.GetValue(i);
    }
    return Volume(sizeX, sizeY, sizeZ, ret);
  }

  Volume Scalar(const Real r) {
    int size = sizeX * sizeY * sizeZ;
    std::vector<Real> ret(size);
    for (int i = 0; i < size; i++) {
      ret[i] = this->GetValue(i) * r;
    }
    return Volume(sizeX, sizeY, sizeZ, ret);
  }

  void Stat() {
    int size = sizeX * sizeY * sizeZ;
    Real sum = 0.0;
    Real max = 0.0;
    for (int i = 0; i < size; i++) {
      sum += this->GetValue(i);
      max = std::max(this->GetValue(i), max);
    }
    Real mean = sum / (Real)size;
    Real var  = 0.0;
    for (int i = 0; i < size; i++) {
      Real diff = this->GetValue(i) - mean;
      var += diff * diff;
    }
    Real stddev = sqrt(var / (Real)size);
  }

  Volume(const Volume& p) {
    sizeX  = p.sizeX;
    sizeY  = p.sizeY;
    sizeZ  = p.sizeZ;
    values = p.values;
  }

  Volume(Volume&& p) {
    sizeX  = p.sizeX;
    sizeY  = p.sizeY;
    sizeZ  = p.sizeZ;
    values = std::move(p.values);
  }

  Volume& operator=(const Volume& p) {
    sizeX  = p.sizeX;
    sizeY  = p.sizeY;
    sizeZ  = p.sizeZ;
    values = p.values;
    return *this;
  }

  Volume& operator=(Volume&& p) {
    sizeX  = p.sizeX;
    sizeY  = p.sizeY;
    sizeZ  = p.sizeZ;
    values = std::move(p.values);
    return *this;
  }

  Volume lerpWith(const Volume& vol, const Real ratio) {
    assert(this->sizeX == vol.sizeX && this->sizeY == vol.sizeY &&
           this->sizeZ == vol.sizeZ);

    int size           = sizeX * sizeY * sizeZ;
    Real oneMinusRatio = 1.0 - ratio;
    std::vector<Real> ret(size);
    for (int i = 0; i < size; i++) {
      ret[i] = this->GetValue(i) * ratio + vol.GetValue(i) * oneMinusRatio;
    }
    return Volume(sizeX, sizeY, sizeZ, ret);
  }

  Real GetLerpValue(Real x, Real y, Real z) const {
    auto lerp = [&](Real v1, Real v2, Real ratio) -> Real {
      return v1 * ratio + v2 * (1.0 - ratio);
    };
    int xId     = (int)x;
    int yId     = (int)y;
    int zId     = (int)z;
    Real xRatio = x - (Real)xId;
    Real yRatio = y - (Real)yId;
    Real zRatio = z - (Real)zId;

    return lerp(
        lerp(lerp(GetValue(xId + 1, yId + 1, zId + 1),
                  GetValue(xId, yId + 1, zId + 1), xRatio),
             lerp(GetValue(xId + 1, yId, zId + 1), GetValue(xId, yId, zId + 1),
                  xRatio),
             yRatio),
        lerp(lerp(GetValue(xId + 1, yId + 1, zId), GetValue(xId, yId + 1, zId),
                  xRatio),
             lerp(GetValue(xId + 1, yId, zId), GetValue(xId, yId, zId), xRatio),
             yRatio),
        zRatio);
  }

private:
  std::vector<Real> values;
  int sizeX = 0;
  int sizeY = 0;
  int sizeZ = 0;
};
