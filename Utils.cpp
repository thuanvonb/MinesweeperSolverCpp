#include "Utils.h"

// Precompute log(k!) using log(k!) = log((k-1)!) + log(k)
// Because nobody wants to compute factorials the hard way
std::vector<double> precomputeLogFactorials(int n) {
  std::vector<double> logFact(n + 1);
  logFact[0] = 0.0;
  for (int i = 1; i <= n; ++i) {
    logFact[i] = logFact[i - 1] + std::log(static_cast<double>(i));
  }
  return logFact;
}

// log(C(n, r)) = log(n!) - log(r!) - log((n-r)!)
// Math: making the impossible merely improbable since forever
double logBinomialWithWeight(int n, int r, const std::vector<double>& logFact, int weight) {
  if (r < 0 || r > n) return -std::numeric_limits<double>::infinity();
  return std::log(static_cast<double>(weight)) + logFact[n] - logFact[r] - logFact[n - r];
}

// The log-sum-exp trick: because naive summation of exp() is for people
// who enjoy numerical instability and undefined behavior
double logSumExp(const std::vector<double>& logValues) {
  if (logValues.empty()) return -std::numeric_limits<double>::infinity();

  double maxVal = *std::max_element(logValues.begin(), logValues.end());
  if (std::isinf(maxVal)) return maxVal;

  double sum = 0.0;
  for (double lv : logValues) {
    sum += std::exp(lv - maxVal);  // Shift to prevent overflow/underflow
  }
  return maxVal + std::log(sum);
}

std::vector<double> computeNormalizedBinomials(int n, const std::vector<int>& R, const std::vector<int>& weights) {
  // Step 1: Precompute log factorials (O(n) time and space)
  auto logFact = precomputeLogFactorials(n);

  // Step 2: Compute log(C(n, r_i)) for each r_i
  std::vector<double> logC(R.size());
  for (size_t i = 0; i < R.size(); ++i) {
    logC[i] = logBinomialWithWeight(n, R[i], logFact, weights[i]);
  }

  // Step 3: Compute log(sum(C(n, r_j))) using log-sum-exp
  double logSum = logSumExp(logC);

  // Step 4: Compute ratios: c_i / sum = exp(log(c_i) - log(sum))
  std::vector<double> ratios(R.size());
  for (size_t i = 0; i < R.size(); ++i) {
    ratios[i] = std::exp(logC[i] - logSum);
  }

  return ratios;
}
