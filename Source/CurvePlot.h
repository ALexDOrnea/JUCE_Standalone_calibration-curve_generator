#pragma once

#include <JuceHeader.h>
#include <memory>
#include "CalibrationCore.h"

/** Semilog-x plot of the interpolated calibration curve and the measured points
    (the matplotlib figure of the Python script), with a hover read-out. */
class CurvePlot : public juce::Component
{
public:
    CurvePlot();

    void setResult (const calib::GenerationResult& result, const calib::CurveSettings& settings);
    void clear();

    void setHighlightedPoint (int index);          // -1 for none
    void setDropHighlight (bool shouldHighlight);

    std::function<void (int)> onPointClicked;      // index into measured points

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotArea() const;
    float xToPx (double f) const;
    float yToPx (double e) const;
    double pxToFreq (float x) const;
    double errorAt (double f) const;               // true PCHIP value, held constant outside
    int nearestPoint (juce::Point<float> p, float maxDistance) const;

    void paintEmptyState (juce::Graphics&);
    void paintGrid (juce::Graphics&, juce::Rectangle<float> area);
    void paintHeldRegions (juce::Graphics&, juce::Rectangle<float> area);
    void paintCurve (juce::Graphics&, juce::Rectangle<float> area);
    void paintPoints (juce::Graphics&);
    void paintLegend (juce::Graphics&, juce::Rectangle<float> area);
    void paintHover (juce::Graphics&, juce::Rectangle<float> area);

    bool hasData = false;
    calib::ErrorPoints measured;
    calib::Curve curve;
    std::unique_ptr<calib::Pchip> interp;
    double xMin = 20.0, xMax = 20000.0;
    double yMin = -1.0, yMax = 1.0, yStep = 0.5;
    bool heldBelow = false, heldAbove = false;

    bool hovering = false;
    juce::Point<float> mousePos;
    int highlighted = -1;
    bool dropHighlight = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CurvePlot)
};
