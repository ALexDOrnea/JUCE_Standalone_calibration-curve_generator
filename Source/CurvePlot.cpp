#include "CurvePlot.h"
#include "AppLookAndFeel.h"

#include <cmath>

namespace
{
    juce::String freqLabel (double f)
    {
        if (f >= 1000.0)
            return juce::String (calib::formatG (f / 1000.0)) + "k";
        return calib::formatG (f);
    }

    juce::String freqReadout (double f)
    {
        if (f >= 1000.0)
            return juce::String (f / 1000.0, 2) + " kHz";
        return juce::String (f, f < 100.0 ? 1 : 0) + " Hz";
    }

    juce::String dbText (double e, int decimals = 2)
    {
        return (e >= 0.0 ? "+" : "") + juce::String (e, decimals) + " dB";
    }

    double niceStep (double span)
    {
        const double raw = span / 6.0;
        const double mag = std::pow (10.0, std::floor (std::log10 (raw)));
        for (double m : { 1.0, 2.0, 2.5, 5.0, 10.0 })
            if (m * mag >= raw)
                return m * mag;
        return 10.0 * mag;
    }
}

CurvePlot::CurvePlot()
{
    setRepaintsOnMouseActivity (false);
}

void CurvePlot::setResult (const calib::GenerationResult& result, const calib::CurveSettings& settings)
{
    if (! result.ok)
    {
        clear();
        return;
    }

    measured = result.measured;
    curve = result.curve;
    heldBelow = result.heldConstantBelow;
    heldAbove = result.heldConstantAbove;
    xMin = settings.fMin;
    xMax = settings.fMax;

    std::vector<double> logF;
    for (double f : measured.freq)
        logF.push_back (std::log10 (f));
    interp = std::make_unique<calib::Pchip> (logF, measured.error);

    // Y range: everything visible, symmetric padding, at least +-1 dB, snapped to nice ticks
    double lo = 0.0, hi = 0.0;
    for (double e : curve.error)    { lo = juce::jmin (lo, e); hi = juce::jmax (hi, e); }
    for (double e : measured.error) { lo = juce::jmin (lo, e); hi = juce::jmax (hi, e); }

    const double pad = juce::jmax (0.25, (hi - lo) * 0.12);
    lo -= pad;
    hi += pad;
    if (hi - lo < 2.0)
    {
        const double mid = 0.5 * (hi + lo);
        lo = mid - 1.0;
        hi = mid + 1.0;
    }

    yStep = niceStep (hi - lo);
    yMin = std::floor (lo / yStep) * yStep;
    yMax = std::ceil (hi / yStep) * yStep;

    if (highlighted >= (int) measured.freq.size())
        highlighted = -1;

    hasData = true;
    repaint();
}

void CurvePlot::clear()
{
    hasData = false;
    interp.reset();
    measured = {};
    curve = {};
    highlighted = -1;
    repaint();
}

void CurvePlot::setHighlightedPoint (int index)
{
    if (highlighted != index)
    {
        highlighted = index;
        repaint();
    }
}

void CurvePlot::setDropHighlight (bool shouldHighlight)
{
    if (dropHighlight != shouldHighlight)
    {
        dropHighlight = shouldHighlight;
        repaint();
    }
}

//==============================================================================
juce::Rectangle<float> CurvePlot::plotArea() const
{
    auto r = getLocalBounds().toFloat();
    r.removeFromLeft (64.0f);
    r.removeFromRight (20.0f);
    r.removeFromTop (44.0f);
    r.removeFromBottom (46.0f);
    return r;
}

float CurvePlot::xToPx (double f) const
{
    const auto a = plotArea();
    const double t = (std::log10 (f) - std::log10 (xMin)) / (std::log10 (xMax) - std::log10 (xMin));
    return a.getX() + (float) t * a.getWidth();
}

float CurvePlot::yToPx (double e) const
{
    const auto a = plotArea();
    const double t = (e - yMin) / (yMax - yMin);
    return a.getBottom() - (float) t * a.getHeight();
}

double CurvePlot::pxToFreq (float x) const
{
    const auto a = plotArea();
    const double t = (x - a.getX()) / a.getWidth();
    return std::pow (10.0, std::log10 (xMin) + t * (std::log10 (xMax) - std::log10 (xMin)));
}

double CurvePlot::errorAt (double f) const
{
    const double lo = std::log10 (measured.freq.front());
    const double hi = std::log10 (measured.freq.back());
    return (*interp) (juce::jlimit (lo, hi, std::log10 (f)));
}

int CurvePlot::nearestPoint (juce::Point<float> p, float maxDistance) const
{
    int best = -1;
    float bestDist = maxDistance;
    for (size_t i = 0; i < measured.freq.size(); ++i)
    {
        const juce::Point<float> q (xToPx (measured.freq[i]), yToPx (measured.error[i]));
        const float d = q.getDistanceFrom (p);
        if (d < bestDist)
        {
            bestDist = d;
            best = (int) i;
        }
    }
    return best;
}

//==============================================================================
void CurvePlot::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (bounds, 12.0f);
    g.setColour (dropHighlight ? theme::accent : theme::border);
    g.drawRoundedRectangle (bounds, 12.0f, dropHighlight ? 2.0f : 1.0f);

    if (! hasData)
    {
        paintEmptyState (g);
        return;
    }

    const auto area = plotArea();

    paintHeldRegions (g, area);
    paintGrid (g, area);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (area.expanded (6.0f).toNearestInt());
        paintCurve (g, area);
        paintPoints (g);
    }

    paintLegend (g, area);

    if (hovering && area.expanded (1.0f).contains (mousePos))
        paintHover (g, area);
}

void CurvePlot::paintEmptyState (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (28.0f);

    juce::Path dashed;
    dashed.addRoundedRectangle (r, 10.0f);
    juce::Path stroked;
    const float dashes[] { 8.0f, 6.0f };
    juce::PathStrokeType (1.5f).createDashedStroke (stroked, dashed, dashes, 2);
    g.setColour ((dropHighlight ? theme::accent : theme::border.brighter (0.2f)));
    g.fillPath (stroked);

    // simple curve icon
    auto icon = r.withSizeKeepingCentre (120.0f, 60.0f).translated (0.0f, -40.0f);
    juce::Path wave;
    for (int i = 0; i <= 60; ++i)
    {
        const float t = (float) i / 60.0f;
        const float y = icon.getCentreY() - std::sin (t * 6.0f) * std::exp (-t * 1.5f) * icon.getHeight() * 0.45f;
        const float x = icon.getX() + t * icon.getWidth();
        if (i == 0) wave.startNewSubPath (x, y); else wave.lineTo (x, y);
    }
    g.setColour (theme::accent.withAlpha (dropHighlight ? 1.0f : 0.7f));
    g.strokePath (wave, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (theme::text);
    g.setFont (theme::font (19.0f, true));
    g.drawText (dropHighlight ? "Release to load the file" : "Drop a measurement CSV here",
                r.withSizeKeepingCentre (r.getWidth(), 30.0f).translated (0.0f, 14.0f),
                juce::Justification::centred);

    g.setColour (theme::textDim);
    g.setFont (theme::font (14.0f));
    g.drawText ("or click \"Load CSV\"  -  columns: frequency, script value, reference value",
                r.withSizeKeepingCentre (r.getWidth(), 22.0f).translated (0.0f, 42.0f),
                juce::Justification::centred);
}

void CurvePlot::paintHeldRegions (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto drawRegion = [&] (float x0, float x1)
    {
        if (x1 - x0 < 1.0f)
            return;

        juce::Rectangle<float> r (x0, area.getY(), x1 - x0, area.getHeight());
        g.setColour (theme::warning.withAlpha (0.05f));
        g.fillRect (r);

        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (r.toNearestInt());
        g.setColour (theme::warning.withAlpha (0.08f));
        for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 10.0f)
            g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);

        if (r.getWidth() > 70.0f)
        {
            g.setColour (theme::warning.withAlpha (0.8f));
            g.setFont (theme::font (11.5f, true));
            g.drawText ("held constant", r.removeFromTop (22.0f), juce::Justification::centred);
        }
    };

    if (heldBelow) drawRegion (area.getX(), xToPx (measured.freq.front()));
    if (heldAbove) drawRegion (xToPx (measured.freq.back()), area.getRight());
}

void CurvePlot::paintGrid (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setFont (theme::font (12.0f));

    // vertical: 1..9 x 10^k, decades stronger, labels on 1-2-5
    const int kMin = (int) std::floor (std::log10 (xMin));
    const int kMax = (int) std::ceil (std::log10 (xMax));
    float lastLabelRight = -1e9f;

    for (int k = kMin; k <= kMax; ++k)
    {
        for (int m = 1; m <= 9; ++m)
        {
            const double f = m * std::pow (10.0, k);
            if (f < xMin * 0.9999 || f > xMax * 1.0001)
                continue;

            const float x = xToPx (f);
            const bool decade = (m == 1);
            g.setColour (decade ? theme::border.brighter (0.15f) : theme::grid);
            g.drawVerticalLine ((int) std::round (x), area.getY(), area.getBottom());

            if (m == 1 || m == 2 || m == 5)
            {
                const auto text = freqLabel (f);
                juce::Rectangle<float> lr (x - 24.0f, area.getBottom() + 6.0f, 48.0f, 16.0f);
                if (lr.getX() > lastLabelRight + 4.0f)
                {
                    g.setColour (decade ? theme::text : theme::textDim);
                    g.drawText (text, lr, juce::Justification::centredTop);
                    lastLabelRight = lr.getRight() - 12.0f;
                }
            }
        }
    }

    // end labels (e.g. 20 and 20k) when they are not on the 1-2-5 grid
    for (double f : { xMin, xMax })
    {
        const double mant = f / std::pow (10.0, std::floor (std::log10 (f)));
        if (std::abs (mant - 1.0) < 1e-9 || std::abs (mant - 2.0) < 1e-9 || std::abs (mant - 5.0) < 1e-9)
            continue;
        const float x = xToPx (f);
        g.setColour (theme::textDim);
        g.drawText (freqLabel (f), juce::Rectangle<float> (x - 24.0f, area.getBottom() + 6.0f, 48.0f, 16.0f),
                    juce::Justification::centredTop);
    }

    // horizontal
    const int decimals = yStep < 0.1 ? 2 : (yStep < 1.0 ? 1 : 0);
    for (double e = yMin; e <= yMax + yStep * 0.01; e += yStep)
    {
        const double v = std::abs (e) < yStep * 1e-6 ? 0.0 : e;
        const float y = yToPx (v);
        const bool zero = juce::exactlyEqual (v, 0.0);
        g.setColour (zero ? theme::textDim.withAlpha (0.6f) : theme::grid);
        g.drawHorizontalLine ((int) std::round (y), area.getX(), area.getRight());

        g.setColour (zero ? theme::text : theme::textDim);
        g.drawText ((v > 0 ? "+" : "") + juce::String (v, decimals),
                    juce::Rectangle<float> (area.getX() - 58.0f, y - 8.0f, 50.0f, 16.0f),
                    juce::Justification::centredRight);
    }

    g.setColour (theme::border.brighter (0.15f));
    g.drawRect (area, 1.0f);

    // axis titles
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.5f, true));
    g.drawText ("Frequency [Hz]", juce::Rectangle<float> (area.getX(), area.getBottom() + 24.0f, area.getWidth(), 18.0f),
                juce::Justification::centred);

    juce::Graphics::ScopedSaveState s (g);
    const float cx = 14.0f, cy = area.getCentreY();
    g.addTransform (juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi, cx, cy));
    g.drawText ("Error [dB]", juce::Rectangle<float> (cx - 60.0f, cy - 9.0f, 120.0f, 18.0f), juce::Justification::centred);
}

void CurvePlot::paintCurve (juce::Graphics& g, juce::Rectangle<float> area)
{
    if (curve.freq.empty())
        return;

    // Dense rendering of the actual interpolant so the line is smooth at any window size
    juce::Path line;
    const int steps = juce::jmax (2, (int) area.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) steps;
        const float y = yToPx (errorAt (pxToFreq (x)));
        if (i == 0) line.startNewSubPath (x, y); else line.lineTo (x, y);
    }

    // area fill towards 0 dB
    const float zeroY = juce::jlimit (area.getY(), area.getBottom(), yToPx (0.0));
    juce::Path fill (line);
    fill.lineTo (area.getRight(), zeroY);
    fill.lineTo (area.getX(), zeroY);
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient::vertical (theme::accent.withAlpha (0.20f), area.getY(),
                                                       theme::accent.withAlpha (0.02f), area.getBottom()));
    g.fillPath (fill);

    // glow + line
    g.setColour (theme::accent.withAlpha (0.15f));
    g.strokePath (line, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (theme::accent);
    g.strokePath (line, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CurvePlot::paintPoints (juce::Graphics& g)
{
    for (size_t i = 0; i < measured.freq.size(); ++i)
    {
        const juce::Point<float> p (xToPx (measured.freq[i]), yToPx (measured.error[i]));
        const bool hi = (int) i == highlighted;
        const float r = hi ? 6.5f : 4.5f;

        if (hi)
        {
            g.setColour (theme::measured.withAlpha (0.25f));
            g.fillEllipse (juce::Rectangle<float> (r * 4.0f, r * 4.0f).withCentre (p));
        }

        g.setColour (theme::panel);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f + 3.0f, r * 2.0f + 3.0f).withCentre (p));
        g.setColour (theme::measured);
        g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (p));
    }
}

void CurvePlot::paintLegend (juce::Graphics& g, juce::Rectangle<float> area)
{
    auto row = juce::Rectangle<float> (area.getX(), 12.0f, area.getWidth(), 22.0f);
    g.setFont (theme::font (13.0f));

    auto item = [&] (const juce::String& text, bool isLine, juce::Colour c)
    {
        const float textW = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), text);
        auto r = row.removeFromRight (textW + 34.0f);
        row.removeFromRight (14.0f);
        auto swatch = r.removeFromLeft (26.0f).withSizeKeepingCentre (22.0f, 10.0f);
        g.setColour (c);
        if (isLine)
            g.fillRoundedRectangle (swatch.withSizeKeepingCentre (22.0f, 3.0f), 1.5f);
        else
            g.fillEllipse (swatch.withSizeKeepingCentre (9.0f, 9.0f));
        g.setColour (theme::textDim);
        g.drawText (text, r, juce::Justification::centredLeft);
    };

    item ("Measured", false, theme::measured);
    item ("Interpolated (PCHIP)", true, theme::accent);
}

void CurvePlot::paintHover (juce::Graphics& g, juce::Rectangle<float> area)
{
    const int near = nearestPoint (mousePos, 10.0f);

    double f, e;
    juce::String extra;
    if (near >= 0)
    {
        f = measured.freq[(size_t) near];
        e = measured.error[(size_t) near];
        const int n = measured.count[(size_t) near];
        extra = "measured point #" + juce::String (near + 1) + (n > 1 ? "  (avg of " + juce::String (n) + ")" : "");
    }
    else
    {
        f = pxToFreq (juce::jlimit (area.getX(), area.getRight(), mousePos.x));
        e = errorAt (f);
        const bool held = f < measured.freq.front() || f > measured.freq.back();
        extra = held ? "held constant" : "interpolated";
    }

    const float x = xToPx (f), y = yToPx (e);

    g.setColour (theme::text.withAlpha (0.25f));
    g.drawVerticalLine ((int) std::round (x), area.getY(), area.getBottom());

    g.setColour (near >= 0 ? theme::measured : theme::accent);
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }));
    g.setColour (theme::panel);
    g.drawEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y }), 1.5f);

    // read-out bubble
    const auto line1 = freqReadout (f) + "    " + dbText (e);
    const float w = 190.0f, h = 44.0f;
    auto box = juce::Rectangle<float> (x + 14.0f, y - h - 10.0f, w, h);
    if (box.getRight() > area.getRight()) box.setX (x - 14.0f - w);
    if (box.getY() < area.getY())          box.setY (y + 12.0f);

    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (box.translated (0.0f, 2.0f), 8.0f);
    g.setColour (theme::panelRaised);
    g.fillRoundedRectangle (box, 8.0f);
    g.setColour (theme::border.brighter (0.2f));
    g.drawRoundedRectangle (box, 8.0f, 1.0f);

    auto inner = box.reduced (10.0f, 5.0f);
    g.setColour (theme::text);
    g.setFont (theme::font (14.0f, true));
    g.drawText (line1, inner.removeFromTop (19.0f), juce::Justification::centredLeft);
    g.setColour (theme::textDim);
    g.setFont (theme::font (12.0f));
    g.drawText (extra, inner, juce::Justification::centredLeft);
}

//==============================================================================
void CurvePlot::mouseMove (const juce::MouseEvent& e)
{
    hovering = true;
    mousePos = e.position;
    if (hasData)
        repaint();
}

void CurvePlot::mouseExit (const juce::MouseEvent&)
{
    hovering = false;
    if (hasData)
        repaint();
}

void CurvePlot::mouseDown (const juce::MouseEvent& e)
{
    if (! hasData)
        return;

    const int idx = nearestPoint (e.position, 10.0f);
    if (idx >= 0 && onPointClicked)
        onPointClicked (idx);
}
