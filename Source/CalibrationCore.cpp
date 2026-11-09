#include "CalibrationCore.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace calib
{

namespace
{
    bool isSpace (char c) noexcept
    {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    }

    std::string trim (const std::string& s)
    {
        size_t b = 0, e = s.size();
        while (b < e && isSpace (s[b])) ++b;
        while (e > b && isSpace (s[e - 1])) --e;
        return s.substr (b, e - b);
    }

    /** Locale-independent strict float parse (like Python's float()). */
    bool parseDouble (const std::string& text, double& out)
    {
        const auto s = trim (text);
        if (s.empty())
            return false;

        std::istringstream in (s);
        in.imbue (std::locale::classic());
        double v = 0.0;
        in >> v;
        if (in.fail())
            return false;
        in >> std::ws;
        if (! in.eof())
            return false;
        out = v;
        return std::isfinite (v);
    }

    std::vector<std::string> splitLines (const std::string& text)
    {
        // Universal newlines like Python's text mode: \n, \r\n and \r
        std::vector<std::string> lines;
        std::string current;
        for (size_t i = 0; i < text.size(); ++i)
        {
            const char c = text[i];
            if (c == '\r' || c == '\n')
            {
                lines.push_back (current);
                current.clear();
                if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n')
                    ++i;
            }
            else
            {
                current += c;
            }
        }
        if (! current.empty())
            lines.push_back (current);
        return lines;
    }

    std::vector<std::string> splitFields (const std::string& line)
    {
        std::vector<std::string> fields;

        if (line.find (';') != std::string::npos)
        {
            // line.split(";") keeps empty fields, then "," -> "."
            std::string field;
            for (char c : line)
            {
                if (c == ';') { fields.push_back (field); field.clear(); }
                else          field += (c == ',' ? '.' : c);
            }
            fields.push_back (field);
        }
        else
        {
            // line.replace(","," ").split()
            std::string field;
            for (char c : line)
            {
                if (c == ',' || isSpace (c))
                {
                    if (! field.empty()) { fields.push_back (field); field.clear(); }
                }
                else
                {
                    field += c;
                }
            }
            if (! field.empty())
                fields.push_back (field);
        }

        return fields;
    }

    int sign (double v) noexcept { return (v > 0.0) - (v < 0.0); }

    // Exact comparison (like numpy), written without == to keep -Wfloat-equal quiet
    bool exactlyEqual (double a, double b) noexcept { return ! (a < b) && ! (b < a); }

    // scipy.interpolate._cubic.PchipInterpolator._edge_case
    double edgeDerivative (double h0, double h1, double m0, double m1) noexcept
    {
        double d = ((2.0 * h0 + h1) * m0 - h0 * m1) / (h0 + h1);

        if (sign (d) != sign (m0))
            d = 0.0;
        else if (sign (m0) != sign (m1) && std::abs (d) > 3.0 * std::abs (m0))
            d = 3.0 * m0;

        return d;
    }
}

//==============================================================================
MeasurementTable parseMeasurements (const std::string& rawText)
{
    MeasurementTable table;

    std::string text = rawText;
    if (text.size() >= 3 && (unsigned char) text[0] == 0xEF
                         && (unsigned char) text[1] == 0xBB
                         && (unsigned char) text[2] == 0xBF)
        text.erase (0, 3);

    const auto lines = splitLines (text);

    for (size_t i = 0; i < lines.size(); ++i)
    {
        const int lineNumber = (int) i + 1;
        const auto line = trim (lines[i]);

        if (line.empty() || line[0] == '#')
            continue;

        const auto fields = splitFields (line);

        double v[3] {};
        bool ok = fields.size() >= 3;
        for (int k = 0; ok && k < 3; ++k)
            ok = parseDouble (fields[(size_t) k], v[k]);

        if (! ok)
        {
            if (lineNumber != 1)
                table.messages.push_back ("Line " + std::to_string (lineNumber) + " ignored");
            continue;
        }

        table.freq.push_back (v[0]);
        table.script.push_back (v[1]);
        table.reference.push_back (v[2]);
    }

    return table;
}

//==============================================================================
ErrorPoints computeErrors (const MeasurementTable& table)
{
    const size_t n = table.freq.size();

    std::vector<size_t> order (n);
    std::iota (order.begin(), order.end(), size_t { 0 });
    std::stable_sort (order.begin(), order.end(),
                      [&] (size_t a, size_t b) { return table.freq[a] < table.freq[b]; });

    ErrorPoints out;
    std::vector<double> sums;

    for (auto idx : order)
    {
        const double f = table.freq[idx];
        const double e = table.script[idx] - table.reference[idx];

        if (out.freq.empty() || ! exactlyEqual (out.freq.back(), f))
        {
            out.freq.push_back (f);
            sums.push_back (e);
            out.count.push_back (1);
        }
        else
        {
            sums.back() += e;
            ++out.count.back();
        }
    }

    out.error.resize (sums.size());
    for (size_t i = 0; i < sums.size(); ++i)
        out.error[i] = sums[i] / (double) out.count[i];

    return out;
}

//==============================================================================
Pchip::Pchip (std::vector<double> x, std::vector<double> y)
    : xs (std::move (x)), ys (std::move (y))
{
    const size_t n = xs.size();
    if (n < 2 || ys.size() != n)
        throw std::invalid_argument ("PCHIP needs at least 2 points and equal-length x/y");

    for (size_t i = 1; i < n; ++i)
        if (! (xs[i] > xs[i - 1]))
            throw std::invalid_argument ("PCHIP x values must be strictly increasing");

    std::vector<double> h (n - 1), m (n - 1);
    for (size_t i = 0; i + 1 < n; ++i)
    {
        h[i] = xs[i + 1] - xs[i];
        m[i] = (ys[i + 1] - ys[i]) / h[i];
    }

    d.assign (n, 0.0);

    if (n == 2)
    {
        d[0] = d[1] = m[0];          // straight line
        return;
    }

    // Interior points: weighted harmonic mean, zero at local extrema / flat segments
    for (size_t k = 1; k + 1 < n; ++k)
    {
        const double m0 = m[k - 1], m1 = m[k];
        if (sign (m0) != sign (m1) || exactlyEqual (m0, 0.0) || exactlyEqual (m1, 0.0))
        {
            d[k] = 0.0;
            continue;
        }

        const double w1 = 2.0 * h[k] + h[k - 1];
        const double w2 = h[k] + 2.0 * h[k - 1];
        const double whmean = (w1 / m0 + w2 / m1) / (w1 + w2);
        d[k] = 1.0 / whmean;
    }

    d[0]     = edgeDerivative (h[0],     h[1],     m[0],     m[1]);
    d[n - 1] = edgeDerivative (h[n - 2], h[n - 3], m[n - 2], m[n - 3]);
}

double Pchip::operator() (double x) const
{
    const size_t n = xs.size();

    // Segment index; outside the range the end polynomials are extended (scipy extrapolate=True)
    size_t k;
    if (x <= xs.front())      k = 0;
    else if (x >= xs.back())  k = n - 2;
    else                      k = (size_t) (std::upper_bound (xs.begin(), xs.end(), x) - xs.begin()) - 1;

    const double h = xs[k + 1] - xs[k];
    const double t = (x - xs[k]) / h;
    const double t2 = t * t, t3 = t2 * t;

    const double h00 =  2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 =        t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 =        t3 -       t2;

    return h00 * ys[k] + h10 * h * d[k] + h01 * ys[k + 1] + h11 * h * d[k + 1];
}

//==============================================================================
std::vector<double> roundedLogGrid (double fMin, double fMax, int n)
{
    std::vector<double> grid ((size_t) std::max (n, 0));
    if (n <= 0)
        return grid;

    const double logStart = std::log10 (fMin);
    const double logStop  = std::log10 (fMax);
    const double step = n > 1 ? (logStop - logStart) / (double) (n - 1) : 0.0;

    for (int i = 0; i < n; ++i)
        grid[(size_t) i] = std::pow (10.0, (double) i * step + logStart);

    grid.front() = fMin;             // geomspace pins the end points exactly
    if (n > 1)
        grid.back() = fMax;

    for (auto& f : grid)
        f = std::nearbyint (f * 10.0) / 10.0;   // np.round(x, 1): half-to-even

    return grid;
}

GenerationResult generate (const MeasurementTable& table, const CurveSettings& settings)
{
    GenerationResult result;
    result.measured = computeErrors (table);
    const auto& f = result.measured.freq;
    const auto& e = result.measured.error;

    if (f.size() < 2)
    {
        result.errorMessage = "You need at least 2 different frequencies";
        return result;
    }
    if (f.front() <= 0.0)
    {
        result.errorMessage = "Frequencies need to be positive";
        return result;
    }
    if (! (settings.fMin > 0.0) || ! (settings.fMax > settings.fMin))
    {
        result.errorMessage = "Output range must satisfy 0 < f min < f max";
        return result;
    }
    if (settings.numPoints < 2)
    {
        result.errorMessage = "The output grid needs at least 2 points";
        return result;
    }

    std::vector<double> logF (f.size());
    std::transform (f.begin(), f.end(), logF.begin(), [] (double v) { return std::log10 (v); });

    const Pchip interp (logF, e);
    const double lo = logF.front(), hi = logF.back();

    result.curve.freq = roundedLogGrid (settings.fMin, settings.fMax, settings.numPoints);
    result.curve.error.reserve (result.curve.freq.size());

    for (double fo : result.curve.freq)
    {
        // outside the measured interval the error stays constant
        const double x = std::clamp (std::log10 (fo), lo, hi);
        result.curve.error.push_back (interp (x));
    }

    result.heldConstantBelow = f.front() > settings.fMin;
    result.heldConstantAbove = f.back()  < settings.fMax;
    result.ok = true;
    return result;
}

//==============================================================================
std::string formatCalibration (const Curve& curve)
{
    // iostream with the classic locale: always '.' as decimal separator, whatever the system locale
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << std::fixed;

    for (size_t i = 0; i < curve.freq.size(); ++i)
        out << std::setprecision (1) << curve.freq[i] << '\t'
            << std::setprecision (2) << curve.error[i] << '\n';

    return out.str();
}

std::string formatG (double value)
{
    std::ostringstream out;
    out.imbue (std::locale::classic());
    out << std::setprecision (6) << value;   // default float format == %g
    return out.str();
}

} // namespace calib
