#ifndef DATA_UTILS_HPP
#define DATA_UTILS_HPP

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace Etas {

// --------------------- Statistics ---------------------

inline double mean(const std::vector<double>& v)
{
    if (v.empty()) throw std::runtime_error("mean: vector empty");
    double s = 0.0;
    for (double x : v) s += x;
    return s / static_cast<double>(v.size());
}

inline double variance_sample(const std::vector<double>& v)
{
    if (v.size() < 2) throw std::runtime_error("variance_sample: need at least 2 values");
    const double m = mean(v);
    double s = 0.0;
    for (double x : v) {
        const double d = x - m;
        s += d * d;
    }
    return s / static_cast<double>(v.size() - 1); 
}

inline double stdev_sample(const std::vector<double>& v)
{
    return std::sqrt(variance_sample(v));
}

inline double quantile(std::vector<double> v, double p)
{
    if (v.empty()) throw std::runtime_error("quantile: vector empty");
    std::sort(v.begin(), v.end());
    const std::size_t n = v.size();
    const double idx = p * (n - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(idx));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
    if (lo == hi) return v[lo];
    const double frac = idx - lo;
    return v[lo] * (1.0 - frac) + v[hi] * frac;
}

inline double median(const std::vector<double>& v) { return quantile(v, 0.5); }

// --------------------- Outlier (IQR) ---------------------

struct IqrOutlier { std::size_t index; double value; };

inline std::vector<IqrOutlier> find_iqr_outliers(const std::vector<double>& v, double factor = 1.5)
{
    if (v.size() < 4) return {};
    std::vector<double> q = v;
    const double q1 = quantile(q, 0.25);
    const double q3 = quantile(q, 0.75);
    const double iqr = q3 - q1;
    const double lo = q1 - factor * iqr;
    const double hi = q3 + factor * iqr;
    std::vector<IqrOutlier> out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (v[i] < lo || v[i] > hi) out.push_back({i, v[i]});
    }
    return out;
}

// --------------------- Z-Score ---------------------

inline std::vector<double> zscore(const std::vector<double>& v)
{
    if (v.size() < 2) throw std::runtime_error("zscore: need at least 2 values");
    const double m = mean(v);
    const double s = stdev_sample(v);
    if (s == 0.0) {
        return std::vector<double>(v.size(), 0.0);
    }
    std::vector<double> out(v.size());
    for (std::size_t i = 0; i < v.size(); ++i) out[i] = (v[i] - m) / s;
    return out;
}

// --------------------- Korrelation ---------------------

inline double correlation(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) throw std::runtime_error("correlation: size mismatch or too small");
    const double mx = mean(x);
    const double my = mean(y);
    double num = 0.0, dx = 0.0, dy = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double dx_i = x[i] - mx;
        const double dy_i = y[i] - my;
        num += dx_i * dy_i;
        dx += dx_i * dx_i;
        dy += dy_i * dy_i;
    }
    const double denom = std::sqrt(dx * dy);
    if (denom == 0.0) return 0.0;
    return num / denom;
}

inline double rank_correlation(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) throw std::runtime_error("rank_correlation: size mismatch or too small");
    std::vector<std::pair<double, std::size_t>> rx(x.size()), ry(y.size());
    for (std::size_t i = 0; i < x.size(); ++i) { rx[i] = {x[i], i}; ry[i] = {y[i], i}; }
    std::sort(rx.begin(), rx.end());
    std::sort(ry.begin(), ry.end());
    std::vector<double> rankx(x.size()), ranky(y.size());
    for (std::size_t i = 0; i < x.size(); ++i) {
        std::size_t idx = rx[i].second;
        rankx[idx] = static_cast<double>(i + 1);
    }
    for (std::size_t i = 0; i < y.size(); ++i) {
        std::size_t idx = ry[i].second;
        ranky[idx] = static_cast<double>(i + 1);
    }
    auto average_ties = [](std::vector<double>& r) {
        const std::size_t n = r.size();
        std::size_t i = 0;
        while (i < n) {
            std::size_t j = i;
            while (j + 1 < n && r[j + 1] == r[i]) ++j;
            if (j > i) {
                const double avg = (r[i] + r[j]) * 0.5;
                for (std::size_t k = i; k <= j; ++k) r[k] = avg;
            }
            i = j + 1;
        }
    };
    average_ties(rankx);
    average_ties(ranky);
    return correlation(rankx, ranky);
}

// --------------------- Linear Regression (y = a + b*x) ---------------------

struct LinearRegression { double slope; double intercept; };

inline LinearRegression linear_fit(const std::vector<double>& x, const std::vector<double>& y)
{
    if (x.size() != y.size() || x.size() < 2) throw std::runtime_error("linear_fit: size mismatch or too small");
    const double mx = mean(x);
    const double my = mean(y);
    double num = 0.0, den = 0.0;
    for (std::size_t i = 0; i < x.size(); ++i) {
        const double dx = x[i] - mx;
        num += dx * (y[i] - my);
        den += dx * dx;
    }
    if (den == 0.0) return {0.0, my};
    const double b = num / den;
    const double a = my - b * mx;
    return {b, a};
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
        std::size_t b = static_cast<std::size_t>((x - mn) / w);
        if (b >= bins) b = bins - 1;
        ++h[b];
    }
    return h;
}

} // namespace Etas

#endif // DATA_UTILS_HPP
