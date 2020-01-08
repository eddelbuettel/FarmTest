/*This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/


# include <RcppArmadillo.h>
# include <algorithm>
# include <string>
// [[Rcpp::depends(RcppArmadillo)]]
// [[Rcpp::plugins(cpp11)]]

double f1(const double x, const arma::vec& resSq, const int n) {
  return arma::accu(arma::min(resSq, x * arma::ones(n))) / (n * x) - std::log(n) / n;
}

double rootf1(const arma::vec& resSq, const int n, double low, double up, const double tol = 0.0001, 
              const int maxIte = 500) {
  int ite = 0;
  double mid, val;
  while (ite <= maxIte && up - low > tol) {
    mid = (up + low) / 2;
    val = f1(mid, resSq, n);
    if (val == 0) {
      return mid;
    } else if (val < 0) {
      up = mid;
    } else {
      low = mid;
    }
    ite++;
  }
  return (low + up) / 2;
}

double f2(const double x, const arma::vec& resSq, const int n, const int d, const int N) {
  return arma::accu(arma::min(resSq, x * arma::ones(N))) / (N * x) - (2 * std::log(d) + std::log(n)) / n;
}

double rootf2(const arma::vec& resSq, const int n, const int d, const int N, double low, double up, 
              const double tol = 0.0001, const int maxIte = 500) {
  int ite = 0;
  double mid, val;
  while (ite <= maxIte && up - low > tol) {
    mid = (up + low) / 2;
    val = f2(mid, resSq, n, d, N);
    if (val == 0) {
      return mid;
    } else if (val < 0) {
      up = mid;
    } else {
      low = mid;
    }
    ite++;
  }
  return (low + up) / 2;
}

//' @title Huber mean estimation
//' @description Internal function implemented in C++ for tuning-free Huber mean estimation. This function is incorporated into \code{farm.mean}.
//' @param X An \eqn{n}-dimensional data vector.
//' @param n The length of \code{X}.
//' @param epsilon An \strong{optional} numerical value for tolerance level. The default value is 0.0001.
//' @param iteMax An \strong{optional} integer for maximun number of iteration. The default value is 500.
//' @seealso \code{\link{farm.mean}}
// [[Rcpp::export]]
double huberMean(const arma::vec& X, const int n, const double epsilon = 0.0001, const int iteMax = 500) {
  double muOld = 0;
  double muNew = arma::mean(X);
  double tauOld = 0;
  double tauNew = arma::stddev(X) * std::sqrt((long double)n / std::log(n));
  int iteNum = 0;
  arma::vec res(n), resSq(n), w(n);
  while (((std::abs(muNew - muOld) > epsilon) || (std::abs(tauNew - tauOld) > epsilon)) && iteNum < iteMax) {
    muOld = muNew;
    tauOld = tauNew;
    res = X - muOld;
    resSq = arma::square(res);
    tauNew = std::sqrt((long double)rootf1(resSq, n, arma::min(resSq), arma::accu(resSq)));
    w = arma::min(tauNew / arma::abs(res), arma::ones(n));
    muNew = arma::as_scalar(X.t() * w) / arma::accu(w);
    iteNum++;
  }
  return muNew;
}

arma::vec huberMeanVec(const arma::mat& X, const int n, const int p, const double epsilon = 0.0001, 
                       const int iteMax = 500) {
  arma::vec rst(p);
  for (int i = 0; i < p; i++) {
    rst(i) = huberMean(X.col(i), n, epsilon, iteMax);
  }
  return rst;
}

double hMeanCov(const arma::vec& Z, const int n, const int d, const int N, const double epsilon = 0.0001, 
                const int iteMax = 500) {
  double muOld = 0;
  double muNew = arma::mean(Z);
  double tauOld = 0;
  double tauNew = arma::stddev(Z) * std::sqrt((long double)n / (2 * std::log(d) + std::log(n)));
  int iteNum = 0;
  arma::vec res(n), resSq(n), w(n);
  while (((std::abs(muNew - muOld) > epsilon) || (std::abs(tauNew - tauOld) > epsilon)) && iteNum < iteMax) {
    muOld = muNew;
    tauOld = tauNew;
    res = Z - muOld;
    resSq = arma::square(res);
    tauNew = std::sqrt((long double)rootf2(resSq, n, d, N, arma::min(resSq), arma::accu(resSq)));
    w = arma::min(tauNew / arma::abs(res), arma::ones(N));
    muNew = arma::as_scalar(Z.t() * w) / arma::accu(w);
    iteNum++;
  }
  return muNew;
}

//' @title Huber-type covariance estimation
//' @description Internal function implemented in C++ for tuning-free Huber-type covariance estimation. This function is incorporated into \code{farm.cov}.
//' @param X An \eqn{n} by \eqn{p} data matrix.
//' @param n Number of rows of \code{X}.
//' @param p Number of columns of \code{X}.
//' @seealso \code{\link{farm.cov}}
// [[Rcpp::export]]
Rcpp::List huberCov(const arma::mat& X, const int n, const int p) {
  arma::vec mu(p);
  arma::mat sigmaHat(p, p);
  for (int j = 0; j < p; j++) {
    mu(j) = huberMean(X.col(j), n);
    double theta = huberMean(arma::square(X.col(j)), n);
    double temp = mu(j) * mu(j);
    if (theta > temp) {
      theta -= temp;
    }
    sigmaHat(j, j) = theta;
  }
  int N = n * (n - 1) >> 1;
  arma::mat Y(N, p);
  for (int i = 0, k = 0; i < n - 1; i++) {
    for (int j = i + 1; j < n; j++) {
      Y.row(k++) = X.row(i) - X.row(j);
    }
  }
  for (int i = 0; i < p - 1; i++) {
    for (int j = i + 1; j < p; j++) {
      sigmaHat(i, j) = sigmaHat(j, i) = hMeanCov(Y.col(i) % Y.col(j) / 2, n, p, N);
    }
  }
  return Rcpp::List::create(Rcpp::Named("means") = mu, Rcpp::Named("cov") = sigmaHat);
}

double mad(const arma::vec& x) {
  return arma::median(arma::abs(x - arma::median(x))) / 0.6744898;
}

int sgn(const double x) {
  return (x > 0) - (x < 0);
}

arma::vec huberDer(const arma::vec& x, const int n, const double tau) {
  arma::vec w(n);
  for (int i = 0; i < n; i++) {
    if (std::abs(x(i)) <= tau) {
      w(i) = -x(i);
    } else {
      w(i) = -tau * sgn(x(i));
    }
  }
  return w;
}

double huberLoss(const arma::vec& x, const int n, const double tau) {
  double loss = 0;
  for (int i = 0; i < n; i++) {
    double cur = x(i);
    loss += std::abs(cur) <= tau ? (cur * cur / 2) : (tau * std::abs(cur) - tau * tau / 2);
  }
  return loss / n;
}

arma::mat standardize(arma::mat X) {
  for (int i = 0; i < X.n_cols; i++) {
    X.col(i) = (X.col(i) - arma::mean(X.col(i))) / arma::stddev(X.col(i));
  }
  return X;
}

arma::vec huberReg(const arma::mat& X, const arma::vec& Y, const int n, const int p, 
                   const double tol = 0.00001, const double constTau = 1.345, const int iteMax = 500) {
  arma::mat Z(n, p + 1);
  Z.cols(1, p) = standardize(X);
  Z.col(0) = arma::ones(n);
  arma::vec betaOld = arma::zeros(p + 1);
  double tau = constTau * mad(Y);
  arma::vec gradOld = Z.t() * huberDer(Y, n, tau) / n;
  double lossOld = huberLoss(Y - Z * betaOld, n, tau);
  arma::vec betaNew = betaOld - gradOld;
  arma::vec res = Y - Z * betaNew;
  double lossNew = huberLoss(res, n, tau);
  arma::vec gradNew, gradDiff, betaDiff;
  int ite = 1;
  while (std::abs(lossNew - lossOld) > tol && arma::norm(betaNew - betaOld, "inf") > tol && ite <= iteMax) {
    tau = constTau * mad(res);
    gradNew = Z.t() * huberDer(res, n, tau) / n;
    gradDiff = gradNew - gradOld;
    betaDiff = betaNew - betaOld;
    double alpha = 1.0;
    double cross = arma::as_scalar(betaDiff.t() * gradDiff);
    if (cross > 0) {
      double a1 = cross / arma::as_scalar(gradDiff.t() * gradDiff);
      double a2 = arma::as_scalar(betaDiff.t() * betaDiff) / cross;
      alpha = std::min(std::min(a1, a2), 1.0);
    }
    betaOld = betaNew;
    gradOld = gradNew;
    lossOld = lossNew;
    betaNew -= alpha * gradNew;
    res += alpha * Z * gradNew; 
    lossNew = huberLoss(res, n, tau);
    ite++;
  }
  betaNew.rows(1, p) /= arma::stddev(X, 0, 0).t();
  betaNew(0) = huberMean(Y - X * betaNew.rows(1, p), n);
  return betaNew;
}

arma::vec getP(const arma::vec& T, const std::string alternative) {
  arma::vec rst;
  if (alternative == "two.sided") {
    rst = 2 * arma::normcdf(-arma::abs(T));
  } else if (alternative == "less") {
    rst = arma::normcdf(T);
  } else {
    rst = arma::normcdf(-T);
  }
  return rst;
}

arma::vec getPboot(const arma::vec& mu, const arma::mat& boot, const arma::vec& h0, 
                   const std::string alternative, const int p, const int B) {
  arma::vec rst(p);
  if (alternative == "two.sided") {
    for (int i = 0; i < p; i++) {
      rst(i) = arma::accu(arma::abs(boot.row(i) - mu(i)) >= std::abs(mu(i) - h0(i)));
    }
  } else if (alternative == "less") {
    for (int i = 0; i < p; i++) {
      rst(i) = arma::accu(boot.row(i) <= 2 * mu(i) - h0(i));
    }
  } else {
    for (int i = 0; i < p; i++) {
      rst(i) = arma::accu(boot.row(i) >= 2 * mu(i) - h0(i));
    }
  }
  return rst / B;
}

//' @title Multiple testing via an adaptive Benjamini-Hochberg procedure
//' @description Internal function implemented in C++ for an adaptive Benjamini-Hochberg procedure. This function is incorporated into \code{farm.fdr}.
//' @param Prob A sequence of p-values. Each entry of \code{Prob} must be between 0 and 1.
//' @param alpha A numerical value for controlling the false discovery rate. The value of \code{alpha} must be strictly between 0 and 1.
//' @param p The length of \code{Prob}.
//' @seealso \code{\link{farm.fdr}}
// [[Rcpp::export]]
arma::uvec getRej(const arma::vec& Prob, const double alpha, const int p) {
  double piHat = (double)arma::accu(Prob > alpha) / ((1 - alpha) * p);
  arma::vec z = arma::sort(Prob);
  double pAlpha = -1.0;
  for (int i = p - 1; i >= 0; i--) {
    if (z(i) * piHat * p <= alpha * (i + 1)) {
      pAlpha = z(i);
      break;
    }
  }
  return Prob <= pAlpha;
}

arma::vec getRatio(const arma::vec& eigenVal, const int n, const int p) {
  int temp = std::min(n, p);
  int len = temp < 4 ? temp - 1 : temp >> 1;
  if (len == 0) {
    arma::vec rst(1);
    rst(0) = eigenVal(p - 1);
    return rst;
  }
  arma::vec ratio(len);
  double comp = eigenVal(p - 1) / eigenVal(p - 2);
  ratio(0) = comp;
  for (int i = 1; i < len; i++) {
    ratio(i) = eigenVal(p - 1 - i) / eigenVal(p - 2 - i);
  }
  return ratio;
}

//' @title Robust multiple testing
//' @description Internal function implemented in C++ for robust multiple testing without factor-adjustment. This case is incorporated into \code{farm.test}.
//' @param X An \eqn{n} by \eqn{p} data matrix with each row being a sample.
//' @param h0 A \eqn{p}-vector of true means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List rmTest(const arma::mat& X, const arma::vec& h0, const double alpha = 0.05, 
                  const std::string alternative = "two.sided") {
  int n = X.n_rows, p = X.n_cols;
  arma::vec mu(p), sigma(p);
  for (int j = 0; j < p; j++) {
    mu(j) = huberMean(X.col(j), n);
    double theta = huberMean(arma::square(X.col(j)), n);
    double temp = mu(j) * mu(j);
    if (theta > temp) {
      theta -= temp;
    }
    sigma(j) = theta;
  }
  sigma = arma::sqrt(sigma / n);
  arma::vec T = (mu - h0) / sigma;
  arma::vec Prob = getP(T, alternative);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("means") = mu, Rcpp::Named("stdDev") = sigma,
                            Rcpp::Named("tStat") = T, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant);
}

//' @title Robust multiple testing with multiplier bootstrap
//' @description Internal function implemented in C++ for robust multiple testing without factor-adjustment, where p-values are obtained via multiplier bootstrap. 
//' This case is incorporated into \code{farm.test}.
//' @param X An \eqn{n} by \eqn{p} data matrix with each row being a sample.
//' @param h0 A \eqn{p}-vector of true means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @param B An \strong{optional} positive integer specifying the size of bootstrap sample. The dafault value is 500.
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List rmTestBoot(const arma::mat& X, const arma::vec& h0, const double alpha = 0.05, 
                      const std::string alternative = "two.sided", const int B = 500) {
  int n = X.n_rows, p = X.n_cols;
  arma::vec mu = huberMeanVec(X, n, p);
  arma::mat boot(p, B);
  for (int i = 0; i < B; i++) {
    arma::uvec idx = arma::find(arma::randu(n) > 0.5);
    int subn = idx.size();
    arma::mat subX = X.rows(idx);
    boot.col(i) = huberMeanVec(subX, subn, p);
  }
  arma::vec Prob = getPboot(mu, boot, h0, alternative, p, B);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("means") = mu, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant);
}

//' @title Two sample robust multiple testing
//' @description Internal function implemented in C++ for two sample robust multiple testing without factor-adjustment. This case is incorporated into \code{farm.test}.
//' @param X An \eqn{nX} by \eqn{p} data matrix with each row being a sample.
//' @param Y An \eqn{nY} by \eqn{p} data matrix with each row being a sample. The number of columns of \code{X} and \code{Y} must be the same.
//' @param h0 A \eqn{p}-vector of true difference in means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List rmTestTwo(const arma::mat& X, const arma::mat& Y, const arma::vec& h0, 
                     const double alpha = 0.05, const std::string alternative = "two.sided") {
  int nX = X.n_rows, nY = Y.n_rows, p = X.n_cols;
  arma::vec muX(p), sigmaX(p), muY(p), sigmaY(p);
  for (int j = 0; j < p; j++) {
    muX(j) = huberMean(X.col(j), nX);
    muY(j) = huberMean(Y.col(j), nY);
    double theta = huberMean(arma::square(X.col(j)), nX);
    double temp = muX(j) * muX(j);
    if (theta > temp) {
      theta -= temp;
    }
    sigmaX(j) = theta;
    theta = huberMean(arma::square(Y.col(j)), nY);
    temp = muY(j) * muY(j);
    if (theta > temp) {
      theta -= temp;
    }
    sigmaY(j) = theta;
  }
  arma::vec T = (muX - muY - h0) / arma::sqrt(sigmaX / nX + sigmaY / nY);
  sigmaX = arma::sqrt(sigmaX / nX);
  sigmaY = arma::sqrt(sigmaY / nY);
  arma::vec Prob = getP(T, alternative);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("meansX") = muX, Rcpp::Named("meansY") = muY, 
                            Rcpp::Named("stdDevX") = sigmaX, Rcpp::Named("stdDevY") = sigmaY,
                            Rcpp::Named("tStat") = T, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant);
}

//' @title Two sample robust multiple testing with multiplier bootstrap
//' @description Internal function implemented in C++ for two sample robust multiple testing without factor-adjustment, where p-values are obtained via multiplier bootstrap. 
//' This case is incorporated into \code{farm.test}.
//' @param X An \eqn{nX} by \eqn{p} data matrix with each row being a sample.
//' @param Y An \eqn{nY} by \eqn{p} data matrix with each row being a sample. The number of columns of \code{X} and \code{Y} must be the same.
//' @param h0 A \eqn{p}-vector of true difference in means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @param B An \strong{optional} positive integer specifying the size of bootstrap sample. The dafault value is 500.
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List rmTestTwoBoot(const arma::mat& X, const arma::mat& Y, const arma::vec& h0, 
                         const double alpha = 0.05, const std::string alternative = "two.sided",
                         const int B = 500) {
  int nX = X.n_rows, nY = Y.n_rows, p = X.n_cols;
  arma::vec muX = huberMeanVec(X, nX, p);
  arma::vec muY = huberMeanVec(Y, nY, p);
  arma::mat bootX(p, B), bootY(p, B);
  for (int i = 0; i < B; i++) {
    arma::uvec idx = arma::find(arma::randu(nX) > 0.5);
    int subn = idx.size();
    arma::mat subX = X.rows(idx);
    bootX.col(i) = huberMeanVec(subX, subn, p);
    idx = arma::find(arma::randu(nY) > 0.5);
    subn = idx.size();
    arma::mat subY = Y.rows(idx);
    bootY.col(i) = huberMeanVec(subY, subn, p);
  }
  arma::vec Prob = getPboot(muX - muY, bootX - bootY, h0, alternative, p, B);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("meansX") = muX, Rcpp::Named("meansY") = muY, 
                            Rcpp::Named("pValues") = Prob, Rcpp::Named("significant") = significant);
}

//' @title FarmTest with unknown factors
//' @description Internal function implemented in C++ for FarmTest with unknown factors. This case is incorporated into \code{farm.test}.
//' @param X An \eqn{n} by \eqn{p} data matrix with each row being a sample.
//' @param h0 A \eqn{p}-vector of true means.
//' @param K An \strong{optional} positive number of factors to be estimated for \code{X}. \code{K} cannot exceed the number of columns of \code{X}. If \code{K} is not specified or specified to be negative, it will be estimated internally.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List farmTest(const arma::mat& X, const arma::vec& h0, int K = -1, const double alpha = 0.05, 
                    const std::string alternative = "two.sided") {
  int n = X.n_rows, p = X.n_cols;
  Rcpp::List listCov = huberCov(X, n, p);
  arma::vec mu = listCov["means"];
  arma::mat sigmaHat = listCov["cov"];
  arma::vec sigma = sigmaHat.diag();
  arma::vec eigenVal;
  arma::mat eigenVec;
  arma::eig_sym(eigenVal, eigenVec, sigmaHat);
  arma::vec ratio;
  if (K <= 0) {
    ratio = getRatio(eigenVal, n, p);
    K = arma::index_max(ratio) + 1;
  }
  arma::mat B(p, K);
  for (int i = 1; i <= K; i++) {
    double lambda = std::sqrt((long double)std::max(eigenVal(p - i), 0.0));
    B.col(i - 1) = lambda * eigenVec.col(p - i);
  }
  arma::vec f = huberReg(B, arma::mean(X, 0).t(), p, K).rows(1, K);
  for (int j = 0; j < p; j++) {
    double temp = arma::norm(B.row(j), 2);
    if (sigma(j) > temp * temp) {
      sigma(j) -= temp * temp;
    }
  }
  mu -= B * f;
  sigma = arma::sqrt(sigma / n);
  arma::vec T = (mu - h0) / sigma;
  arma::vec Prob = getP(T, alternative);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("means") = mu, Rcpp::Named("stdDev") = sigma,
                            Rcpp::Named("loadings") = B, Rcpp::Named("nfactors") = K, 
                            Rcpp::Named("tStat") = T, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant, Rcpp::Named("eigens") = eigenVal,
                            Rcpp::Named("ratio") = ratio);
}

//' @title Two sample FarmTest with unknown factors
//' @description Internal function implemented in C++ for two sample FarmTest with unknown factors. This case is incorporated into \code{farm.test}.
//' @param X An \eqn{nX} by \eqn{p} data matrix with each row being a sample.
//' @param Y An \eqn{nY} by \eqn{p} data matrix with each row being a sample. The number of columns of \code{X} and \code{Y} must be the same.
//' @param h0 A \eqn{p}-vector of true difference in means.
//' @param KX An \strong{optional} positive number of factors to be estimated for \code{X}. \code{KX} cannot exceed the number of columns of \code{X}. If \code{KX} is not specified or specified to be negative, it will be estimated internally.
//' @param KY An \strong{optional} positive number of factors to be estimated for \code{Y}. \code{KY} cannot exceed the number of columns of \code{Y}. If \code{KY} is not specified or specified to be negative, it will be estimated internally.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List farmTestTwo(const arma::mat& X, const arma::mat& Y, const arma::vec& h0, int KX = -1, 
                       int KY = -1, const double alpha = 0.05, const std::string alternative = "two.sided") {
  int nX = X.n_rows, nY = Y.n_rows, p = X.n_cols;
  Rcpp::List listCov = huberCov(X, nX, p);
  arma::vec muX = listCov["means"];
  arma::mat sigmaHat = listCov["cov"];
  arma::vec sigmaX = sigmaHat.diag();
  arma::vec eigenValX, eigenValY;
  arma::mat eigenVec;
  arma::eig_sym(eigenValX, eigenVec, sigmaHat);
  arma::vec ratioX, ratioY;
  if (KX <= 0) {
    ratioX = getRatio(eigenValX, nX, p);
    KX = arma::index_max(ratioX) + 1;
  }
  arma::mat BX(p, KX);
  for (int i = 1; i <= KX; i++) {
    double lambda = std::sqrt((long double)std::max(eigenValX(p - i), 0.0));
    BX.col(i - 1) = lambda * eigenVec.col(p - i);
  }
  arma::vec fX = huberReg(BX, arma::mean(X, 0).t(), p, KX).rows(1, KX);
  listCov = huberCov(Y, nY, p);
  arma::vec muY = listCov["means"];
  sigmaHat = Rcpp::as<arma::mat>(listCov["cov"]);
  arma::vec sigmaY = sigmaHat.diag();
  arma::eig_sym(eigenValY, eigenVec, sigmaHat);
  if (KY <= 0) {
    ratioY = getRatio(eigenValY, nY, p);
    KY = arma::index_max(ratioY) + 1;
  }
  arma::mat BY(p, KY);
  for (int i = 1; i <= KY; i++) {
    double lambda = std::sqrt((long double)std::max(eigenValY(p - i), 0.0));
    BY.col(i - 1) = lambda * eigenVec.col(p - i);
  }
  arma::vec fY = huberReg(BY, arma::mean(Y, 0).t(), p, KY).rows(1, KY);
  for (int j = 0; j < p; j++) {
    double temp = arma::norm(BX.row(j), 2);
    if (sigmaX(j) > temp * temp) {
      sigmaX(j) -= temp * temp;
    }
    temp = arma::norm(BY.row(j), 2);
    if (sigmaY(j) > temp * temp) {
      sigmaY(j) -= temp * temp;
    }
  }
  muX -= BX * fX;
  muY -= BY * fY;
  arma::vec T = (muX - muY - h0) / arma::sqrt(sigmaX / nX + sigmaY / nY);
  sigmaX = arma::sqrt(sigmaX / nX);
  sigmaY = arma::sqrt(sigmaY / nY);
  arma::vec Prob = getP(T, alternative);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("meansX") = muX, Rcpp::Named("meansY") = muY, 
                            Rcpp::Named("stdDevX") = sigmaX, Rcpp::Named("stdDevY") = sigmaY,
                            Rcpp::Named("loadingsX") = BX, Rcpp::Named("loadingsY") = BY,
                            Rcpp::Named("nfactorsX") = KX, Rcpp::Named("nfactorsY") = KY,
                            Rcpp::Named("tStat") = T, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant, Rcpp::Named("eigensX") = eigenValX,
                            Rcpp::Named("eigensY") = eigenValY, Rcpp::Named("ratioX") = ratioX,
                            Rcpp::Named("ratioY") = ratioY);
}

//' @title FarmTest with known factors
//' @description Internal function implemented in C++ for FarmTest with known factors. This case is incorporated into \code{farm.test}.
//' @param X An \eqn{n} by \eqn{p} data matrix with each row being a sample.
//' @param fac A factor matrix with each column being a factor for \code{X}. The number of rows of \code{fac} and \code{X} must be the same.
//' @param h0 A \eqn{p}-vector of true means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List farmTestFac(const arma::mat& X, const arma::mat& fac, const arma::vec& h0, 
                       const double alpha = 0.05, const std::string alternative = "two.sided") {
  int n = X.n_rows, p = X.n_cols, K = fac.n_cols;
  arma::mat Sigma = arma::cov(fac);
  arma::vec mu(p), sigma(p);
  arma::vec theta, beta;
  arma::mat B(p, K);
  for (int j = 0; j < p; j++) {
    theta = huberReg(fac, X.col(j), n, K);
    mu(j) = theta(0);
    beta = theta.rows(1, K);
    B.row(j) = beta.t();
    double sig = huberMean(arma::square(X.col(j)), n);
    double temp = mu(j) * mu(j);
    if (sig > temp) {
      sig -= temp;
    }
    temp = arma::as_scalar(beta.t() * Sigma * beta);
    if (sig > temp * temp) {
      sig -= temp * temp;
    }
    sigma(j) = sig;
  }
  sigma = arma::sqrt(sigma / n);
  arma::vec T = (mu - h0) / sigma;
  arma::vec Prob = getP(T, alternative);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("means") = mu, Rcpp::Named("stdDev") = sigma,
                            Rcpp::Named("loadings") = B, Rcpp::Named("nfactors") = K,
                            Rcpp::Named("tStat") = T, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant);
}

//' @title FarmTest with known factors and multiplier bootstrap
//' @description Internal function implemented in C++ for FarmTest with known factors, where p-values are obtained via multiplier bootstrap. 
//' This case is incorporated into \code{farm.test}.
//' @param X An \eqn{n} by \eqn{p} data matrix with each row being a sample.
//' @param fac A factor matrix with each column being a factor for \code{X}. The number of rows of \code{fac} and \code{X} must be the same.
//' @param h0 A \eqn{p}-vector of true means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @param B An \strong{optional} positive integer specifying the size of bootstrap sample. The dafault value is 500.
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List farmTestFacBoot(const arma::mat& X, const arma::mat& fac, const arma::vec& h0, 
                           const double alpha = 0.05, const std::string alternative = "two.sided",
                           const int B = 500) {
  int n = X.n_rows, p = X.n_cols, K = fac.n_cols;
  arma::vec mu(p);
  for (int j = 0; j < p; j++) {
    mu(j) = huberReg(fac, X.col(j), n, K)(0);
  }
  arma::mat boot(p, B);
  for (int i = 0; i < B; i++) {
    arma::uvec idx = arma::find(arma::randu(n) > 0.5);
    int subn = idx.size();
    arma::mat subX = X.rows(idx);
    for (int j = 0; j < p; j++) {
      boot(j, i) = huberReg(fac.rows(idx), subX.col(j), subn, K)(0);
    }
  }
  arma::vec Prob = getPboot(mu, boot, h0, alternative, p, B);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("means") = mu, Rcpp::Named("nfactors") = K,
                            Rcpp::Named("pValues") = Prob, Rcpp::Named("significant") = significant);
}

//' @title Two sample FarmTest with known factors
//' @description Internal function implemented in C++ for two sample FarmTest with known factors. This case is incorporated into \code{farm.test}.
//' @param X An \eqn{nX} by \eqn{p} data matrix with each row being a sample.
//' @param facX A factor matrix with each column being a factor for \code{X}. The number of rows of \code{facX} and \code{X} must be the same.
//' @param Y An \eqn{nY} by \eqn{p} data matrix with each row being a sample. The number of columns of \code{X} and \code{Y} must be the same.
//' @param facY A factor matrix with each column being a factor for \code{Y}. The number of rows of \code{facY} and \code{Y} must be the same.
//' @param h0 A \eqn{p}-vector of true difference in means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List farmTestTwoFac(const arma::mat& X, const arma::mat& facX, const arma::mat& Y, 
                          const arma::mat& facY, const arma::vec& h0, const double alpha = 0.05, 
                          const std::string alternative = "two.sided") {
  int nX = X.n_rows, nY = Y.n_rows, p = X.n_cols, KX = facX.n_cols, KY = facY.n_cols;
  arma::mat SigmaX = arma::cov(facX);
  arma::mat SigmaY = arma::cov(facY);
  arma::vec muX(p), sigmaX(p), muY(p), sigmaY(p);
  arma::vec theta, beta;
  arma::mat BX(p, KX), BY(p, KY);
  for (int j = 0; j < p; j++) {
    theta = huberReg(facX, X.col(j), nX, KX);
    muX(j) = theta(0);
    beta = theta.rows(1, KX);
    BX.row(j) = beta.t();
    double sig = huberMean(arma::square(X.col(j)), nX);
    double temp = muX(j) * muX(j);
    if (sig > temp) {
      sig -= temp;
    }
    temp = arma::as_scalar(beta.t() * SigmaX * beta);
    if (sig > temp * temp) {
      sig -= temp * temp;
    }
    sigmaX(j) = sig;
    theta = huberReg(facY, Y.col(j), nY, KY);
    muY(j) = theta(0);
    beta = theta.rows(1, KY);
    BY.row(j) = beta.t();
    sig = huberMean(arma::square(Y.col(j)), nY);
    temp = muY(j) * muY(j);
    if (sig > temp) {
      sig -= temp;
    }
    temp = arma::as_scalar(beta.t() * SigmaY * beta);
    if (sig > temp * temp) {
      sig -= temp * temp;
    }
    sigmaY(j) = sig;
  }
  arma::vec T = (muX - muY - h0) / arma::sqrt(sigmaX / nX + sigmaY / nY);
  sigmaX = arma::sqrt(sigmaX / nX);
  sigmaY = arma::sqrt(sigmaY / nY);
  arma::vec Prob = getP(T, alternative);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("meansX") = muX, Rcpp::Named("meansY") = muY, 
                            Rcpp::Named("stdDevX") = sigmaX, Rcpp::Named("stdDevY") = sigmaY,
                            Rcpp::Named("loadingsX") = BX, Rcpp::Named("loadingsY") = BY,
                            Rcpp::Named("nfactorsX") = KX, Rcpp::Named("nfactorsY") = KY,
                            Rcpp::Named("tStat") = T, Rcpp::Named("pValues") = Prob, 
                            Rcpp::Named("significant") = significant);
}

//' @title Two sample FarmTest with known factors and multiplier bootstrap
//' @description Internal function implemented in C++ for two sample FarmTest with known factors, where p-values are obtained via multiplier bootstrap. 
//' This case is incorporated into \code{farm.test}.
//' @param X An \eqn{nX} by \eqn{p} data matrix with each row being a sample.
//' @param facX A factor matrix with each column being a factor for \code{X}. The number of rows of \code{facX} and \code{X} must be the same.
//' @param Y An \eqn{nY} by \eqn{p} data matrix with each row being a sample. The number of columns of \code{X} and \code{Y} must be the same.
//' @param facY A factor matrix with each column being a factor for \code{Y}. The number of rows of \code{facY} and \code{Y} must be the same.
//' @param h0 A \eqn{p}-vector of true difference in means.
//' @param alpha An \strong{optional} level for controlling the false discovery rate. The value of \code{alpha} must be between 0 and 1. The default value is 0.05.
//' @param alternative An \strong{optional} character string specifying the alternate hypothesis, must be one of "two.sided" (default), "less" or "greater".
//' @param B An \strong{optional} positive integer specifying the size of bootstrap sample. The dafault value is 500.
//' @seealso \code{\link{farm.test}}
// [[Rcpp::export]]
Rcpp::List farmTestTwoFacBoot(const arma::mat& X, const arma::mat& facX, const arma::mat& Y, 
                              const arma::mat& facY, const arma::vec& h0, const double alpha = 0.05, 
                              const std::string alternative = "two.sided", const int B = 500) {
  int nX = X.n_rows, nY = Y.n_rows, p = X.n_cols, KX = facX.n_cols, KY = facY.n_cols;
  arma::vec muX(p), muY(p);
  for (int j = 0; j < p; j++) {
    muX(j) = huberReg(facX, X.col(j), nX, KX)(0);
    muY(j) = huberReg(facY, Y.col(j), nY, KY)(0);
  }
  arma::mat bootX(p, B), bootY(p, B);
  for (int i = 0; i < B; i++) {
    arma::uvec idx = arma::find(arma::randu(nX) > 0.5);
    int subn = idx.size();
    arma::mat subX = X.rows(idx);
    for (int j = 0; j < p; j++) {
      bootX(j, i) = huberReg(facX.rows(idx), subX.col(j), subn, KX)(0);
    }
    idx = arma::find(arma::randu(nY) > 0.5);
    subn = idx.size();
    arma::mat subY = Y.rows(idx);
    for (int j = 0; j < p; j++) {
      bootY(j, i) = huberReg(facY.rows(idx), subY.col(j), subn, KY)(0);
    }
  }
  arma::vec Prob = getPboot(muX - muY, bootX - bootY, h0, alternative, p, B);
  arma::uvec significant = getRej(Prob, alpha, p);
  return Rcpp::List::create(Rcpp::Named("meansX") = muX, Rcpp::Named("meansY") = muY, 
                            Rcpp::Named("nfactorsX") = KX, Rcpp::Named("nfactorsY") = KY,
                            Rcpp::Named("pValues") = Prob, Rcpp::Named("significant") = significant);
}
