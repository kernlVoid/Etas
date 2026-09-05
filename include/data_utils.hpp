#ifndef DATA_UTILS_HPP
#define DATA_UTILS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>
#include <numeric>
#include <limits>

namespace Etas {

// --------------------- Numeric helpers (Welford) ---------------------

struct Welford {
    std::size_t n = 0;
    double mean = 0.0;
    double M2 = 0.0; // sum of squares of differences from the current mean

    void add(double x) noexcept {
        ++n;
        const double delta = x - mean;
        mean += delta / static_cast<double>(n);
        const double delta2 = x - mean;
        M2 += delta * delta2;
    }

    template <typename It>
    static Welford from_range(It first, It last) noexcept {
        Welford w;
        for (; first != last; ++first) w.add(*first);
        return w;
    }

    double variance_sample() const {
        if (n < 2) throw std::runtime_error("variance_sample: need at least 2 values");
        return M2 / static_cast<double>(n - 1);
    }

    double variance_population() const {
        if (n == 0) return 0.0;
        return M2 / static_cast<double>(n);
    }

    double stdev_sample() const { return std::sqrt(variance_sample()); }
};

struct WelfordCov {
    std::size_t n = 0;
    double mean_x = 0.0;
    double mean_y = 0.0;
    double C = 0.0; // running covariance accumulator

    void add(double x, double y) noexcept {
        ++n;
        const double dx = x - mean_x;
        const double dy = y - mean_y;
        mean_x += dx / static_cast<double>(n);
        mean_y += dy / static_cast<double>(n);
        C += dx * (y - mean_y);
    }

    double covariance_sample() const {
        if (n < 2) throw std::runtime_error("covariance_sample: need at least 2 pairs");
        return C / static_cast<double>(n - 1);
    }
};

// --------------------- Statistics ---------------------

inline double mean(const std::vector<double>& v)
{
    if (v.empty()) throw std::runtime_error("mean: vector empty");
    return Welford::from_range(v.begin(), v.end()).mean;
}

inline double variance_sample(const std::vector<double>& v)
{
    return Welford::from_range(v.begin(), v.end()).variance_sample();
}

inline double stdev_sample(const std::vector<double>& v)
{
    return Welford::from_range(v.begin(), v.end()).stdev_sample();
}

inline double quantile(std::vector<double> v, double p)
{
    if (v.empty()) throw std::runtime_error("quantile: vector empty");
    if (p < 0.0 || p > 1.0) throw std::runtime_error("quantile: p must be in [0,1]");
    const std::size_t n = v.size();
    if (n == 1) return v[0];
    const double idx = p * (n - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(idx));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
    // use partial selection for performance (avoids full sort)
    std::nth_element(v.begin(), v.begin() + lo, v.end());
    const double vlo = v[lo];
    if (lo == hi) return vlo;
    // ensure hi element is in correct position; restrict to suffix for slight speedup
    std::nth_element(v.begin() + lo + 1, v.begin() + hi, v.end());
    const double vhi = v[hi];
    const double frac = idx - static_cast<double>(lo);
    return vlo * (1.0 - frac) + vhi * frac;
}

inline double median(const std::vector<double>& v) { return quantile(v, 0.5); }

// --------------------- Outlier (IQR) ---------------------

struct IqrOutlier { std::size_t index; double value; };

inline std::vector<IqrOutlier> find_iqr_outliers(const std::vector<double>& v, double factor = 1.5)
{
    if (v.size() < 4) return {};
    std::vector<double> q = v; // quantile works on a copy
    const double q1 = quantile(q, 0.25);
    const double q3 = quantile(q, 0.75);
    const double iqr = q3 - q1;
    const double lo = q1 - factor * iqr;
    const double hi = q3 + factor * iqr;
    std::vector<IqrOutlier> out;
    out.reserve(16);
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] < lo || v[i] > hi) out.push_back({i, v[i]});
    }
    return out;
}

// --------------------- Z-Score ---------------------

inline std::vector<double> zscore(const std::vector<double>& v)
{
    if (v.size() < 2) throw std::runtime_error("zscore: need at least 2 values");
    const Welford w = Welford::from_range(v.begin(), v.end());
    const double s = std::sqrt(w.variance_sample());
    if (s == 0.0) return std::vector<double>(v.size(), 0.0);
    std::vector<double> out;
    out.reserve(v.size());
    for (double x : v) out.push_back((x - w.mean) / s);
    return out;
}

// --------------------- Correlation ---------------------

inline double correlation(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) throw std::runtime_error("correlation: size mismatch or too small");
    // compute running covariance and variances in numerically stable one-pass ways
    WelfordCov wc;
    for (std::size_t i = 0; i < x.size(); ++i) wc.add(x[i], y[i]);
    // compute variances separately via Welford
    const Welford wx = Welford::from_range(x.begin(), x.end());
    const Welford wy = Welford::from_range(y.begin(), y.end());
    if (wx.n < 2 || wy.n < 2) return 0.0;
    const double sx2 = wx.variance_sample();
    const double sy2 = wy.variance_sample();
    if (!(sx2 > 0.0 && sy2 > 0.0)) return 0.0;
    const double cov = wc.covariance_sample();
    return cov / std::sqrt(sx2 * sy2);
}

inline double rank_correlation(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) throw std::runtime_error("rank_correlation: size mismatch or too small");
    const std::size_t n = x.size();
    auto ranks_from = [&](const std::vector<double>& v) {
        std::vector<std::pair<double, std::size_t>> tmp;
        tmp.reserve(n);
        for (std::size_t i = 0; i < n; ++i) tmp.emplace_back(v[i], i);
        std::sort(tmp.begin(), tmp.end(), [](const auto &a, const auto &b){ return a.first < b.first; });
        std::vector<double> ranks(n);
        std::size_t i = 0;
        while (i < n) {
            std::size_t j = i + 1;
            while (j < n && tmp[j].first == tmp[i].first) ++j;
            const double avg_rank = (static_cast<double>(i) + static_cast<double>(j - 1)) / 2.0 + 1.0; // 1-based
            for (std::size_t k = i; k < j; ++k) ranks[tmp[k].second] = avg_rank;
            i = j;
        }
        return ranks;
    };
    const std::vector<double> rankx = ranks_from(x);
    const std::vector<double> ranky = ranks_from(y);
    return correlation(rankx, ranky);
}

// --------------------- Linear Regression (y = a + b*x) ---------------------

struct LinearRegression { double slope; double intercept; };

inline LinearRegression linear_fit(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) throw std::runtime_error("linear_fit: size mismatch or too small");
    WelfordCov wc;
    for (std::size_t i = 0; i < x.size(); ++i) wc.add(x[i], y[i]);
    // Sxx is sum (xi - mean_x)^2 = variance_sample * (n-1)
    const Welford wx = Welford::from_range(x.begin(), x.end());
    if (wx.n < 2) return {0.0, wc.mean_y};
    const double Sxx = wx.M2; // M2 is sum (xi-mean)^2
    if (Sxx == 0.0) return {0.0, wc.mean_y};
    const double slope = wc.C / Sxx;
    const double intercept = wc.mean_y - slope * wc.mean_x;
    return {slope, intercept};
}

// --------------------- Histogram ---------------------

inline std::vector<std::size_t> histogram(const std::vector<double>& v, std::size_t bins)
{
    if (v.empty() || bins == 0) return {};
    const double mn = *std::min_element(v.begin(), v.end());
    const double mx = *std::max_element(v.begin(), v.end());
    if (mn == mx) {
        std::vector<std::size_t> h(bins, 0);
        h[0] = v.size();
        return h;
    }
    const double w = (mx - mn) / static_cast<double>(bins);
    std::vector<std::size_t> h(bins, 0);
    for (double x : v) {
        std::size_t b = static_cast<std::size_t>(std::floor((x - mn) / w));
        if (b >= bins) b = bins - 1;
        ++h[b];
    }
    return h;
}

// --------------------- Additional robust & utility tools ---------------------

inline double mad(const std::vector<double>& v)
{
    if (v.empty()) throw std::runtime_error("mad: vector empty");
    const double med = median(v);
    std::vector<double> devs;
    devs.reserve(v.size());
    for (double x : v) devs.push_back(std::abs(x - med));
    return median(devs);
}

inline std::vector<double> normalize_minmax(const std::vector<double>& v)
{
    if (v.empty()) return {};
    const double mn = *std::min_element(v.begin(), v.end());
    const double mx = *std::max_element(v.begin(), v.end());
    if (mn == mx) {
        return std::vector<double>(v.size(), 0.0);
    }
    std::vector<double> out;
    out.reserve(v.size());
    const double denom = mx - mn;
    for (double x : v) out.push_back((x - mn) / denom);
    return out;
}

inline std::vector<double> winsorize(const std::vector<double>& v, double lower_p = 0.05, double upper_p = 0.95)
{
    if (v.empty()) return {};
    if (!(lower_p >= 0.0 && lower_p <= 1.0 && upper_p >= 0.0 && upper_p <= 1.0 && lower_p <= upper_p))
        throw std::runtime_error("winsorize: invalid percentiles");
    const double lo_val = quantile(v, lower_p);
    const double hi_val = quantile(v, upper_p);
    std::vector<double> out;
    out.reserve(v.size());
    for (double x : v) {
        if (x < lo_val) out.push_back(lo_val);
        else if (x > hi_val) out.push_back(hi_val);
        else out.push_back(x);
    }
    return out;
}

inline std::vector<double> cumsum(const std::vector<double>& v)
{
    std::vector<double> out;
    out.reserve(v.size());
    double s = 0.0;
    for (double x : v) {
        s += x;
        out.push_back(s);
    }
    return out;
}

inline std::vector<double> moving_average(const std::vector<double>& v, std::size_t window)
{
    if (v.empty() || window == 0) return {};
    if (window == 1) return v;
    const std::size_t n = v.size();
    std::vector<double> out;
    out.reserve(n);
    double s = 0.0;
    std::size_t w = std::min(window, n);
    for (std::size_t i = 0; i < n; ++i) {
        s += v[i];
        if (i >= w) s -= v[i - w];
        const std::size_t denom = std::min(i + 1, w);
        out.push_back(s / static_cast<double>(denom));
    }
    return out;
}

} // namespace Etas

#endif // DATA_UTILS_HPP
