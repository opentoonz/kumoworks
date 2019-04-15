#pragma once
#define _USE_MATH_DEFINES
#include <random>
#include <cmath>
#include <functional>

class UniformRandomGenerator01 {
public:
  UniformRandomGenerator01(unsigned int seed_in,
                           unsigned long long offset = 0) {
    MT.seed(seed_in);
    MT.discard(offset);
    std::uniform_real_distribution<Real> dist(0.0, 1.0);
    Rand = std::bind(dist, MT);
  };

  UniformRandomGenerator01() {
    MT.seed(1);
    std::uniform_real_distribution<Real> dist(0.0, 1.0);
    Rand = std::bind(dist, MT);
  };

  Real GenRand() { return Rand(); }

  void SetSeed(unsigned int seed_in) { MT.seed(seed_in); }

private:
  std::mt19937 MT;
  std::function<Real()> Rand;
};

class GaussianRandomGenerator {
public:
  GaussianRandomGenerator(Real mean, Real stddev, unsigned int seed_in) {
    MT.seed(seed_in);
    std::normal_distribution<Real> dist(mean, stddev);
    Rand = std::bind(dist, MT);
  };

  Real GenRand() { return Rand(); }

  void SetSeed(unsigned int seed_in) { MT.seed(seed_in); }

private:
  std::mt19937 MT;
  std::function<Real()> Rand;
};
