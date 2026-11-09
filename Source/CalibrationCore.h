#pragma once

// Pure C++ port of eq_generator.py: CSV parsing, error averaging, PCHIP interpolation
// on a logarithmic frequency axis and calibration-file formatting.
// No GUI dependencies, so it is shared by the app and the test program.

#include <string>
#include <vector>

namespace calib
{

//==============================================================================
/** Rows read from the measurement CSV: frequency, script value, reference value. */
struct MeasurementTable
{
    std::vector<double> freq, script, reference;
    std::vector<std::string> messages;   // e.g. "Line 7 ignored"
};

/** Parses CSV text exactly like read_csv() in the Python script:
    - UTF-8 BOM is skipped, empty lines and lines starting with '#' are skipped
    - if a line contains ';' fields are split on ';' and ',' is treated as decimal separator
    - otherwise ',' and whitespace are both separators
    - the first three fields must be numbers; a non-numeric line 1 is a silent header,
      any other bad line produces "Line N ignored". */
MeasurementTable parseMeasurements (const std::string& text);

//==============================================================================
/** Measured error (script - reference) sorted by frequency, duplicates averaged. */
struct ErrorPoints
{
    std::vector<double> freq, error;
    std::vector<int> count;              // how many CSV rows were averaged per frequency
};

ErrorPoints computeErrors (const MeasurementTable& table);

//==============================================================================
/** Piecewise Cubic Hermite Interpolating Polynomial, identical to
    scipy.interpolate.PchipInterpolator (Fritsch-Butland derivatives, scipy edge rule).
    x must be strictly increasing and contain at least 2 values. */
class Pchip
{
public:
    Pchip (std::vector<double> x, std::vector<double> y);

    double operator() (double x) const;

    const std::vector<double>& derivatives() const noexcept { return d; }

private:
    std::vector<double> xs, ys, d;
};

//==============================================================================
struct CurveSettings
{
    double fMin = 20.0;
    double fMax = 20000.0;
    int numPoints = 300;                 // ~100 points/decade
};

struct Curve
{
    std::vector<double> freq, error;
};

struct GenerationResult
{
    bool ok = false;
    std::string errorMessage;
    ErrorPoints measured;
    Curve curve;
    bool heldConstantBelow = false;      // fMin below the first measured frequency
    bool heldConstantAbove = false;      // fMax above the last measured frequency
};

/** np.round(np.geomspace(fMin, fMax, n), 1) */
std::vector<double> roundedLogGrid (double fMin, double fMax, int n);

/** Runs the full pipeline of main(): errors -> validation -> PCHIP on log10(f) -> grid. */
GenerationResult generate (const MeasurementTable& table, const CurveSettings& settings);

/** np.savetxt(..., fmt="%.1f\t%.2f") */
std::string formatCalibration (const Curve& curve);

/** Same formatting as Python's f"{x:g}". */
std::string formatG (double value);

} // namespace calib
