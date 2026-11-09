#pragma once

#include <JuceHeader.h>
#include "AppLookAndFeel.h"
#include "CalibrationCore.h"
#include "CurvePlot.h"

class MainComponent : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::TableListBoxModel
{
public:
    MainComponent();
    ~MainComponent() override;

    void loadFile (const juce::File& file);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    // FileDragAndDropTarget
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray& files, int, int) override;

private:
    // TableListBoxModel
    int getNumRows() override;
    void paintRowBackground (juce::Graphics&, int row, int w, int h, bool selected) override;
    void paintCell (juce::Graphics&, int row, int col, int w, int h, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    enum class Level { info, success, warning, error };

    void browseForCsv();
    void exportCalibration();
    void regenerate();
    bool readSettings (calib::CurveSettings& out, juce::String& problem) const;
    void log (const juce::String& text, Level level = Level::info);
    void setStatus (const juce::String& text, Level level);
    static juce::Colour colourFor (Level level);

    void paintCard (juce::Graphics&, juce::Rectangle<int> r, const juce::String& title) const;

    AppLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltipWindow { this, 600 };

    juce::TextButton loadButton   { "Load CSV..." };
    juce::TextButton exportButton { "Export calibration..." };

    juce::Label fMinLabel   { {}, "f min [Hz]" };
    juce::Label fMaxLabel   { {}, "f max [Hz]" };
    juce::Label pointsLabel { {}, "Points" };
    juce::TextEditor fMinEditor, fMaxEditor, pointsEditor;
    juce::TextButton resetButton { "Defaults" };

    juce::TableListBox table { "points", this };
    juce::TextEditor logBox;
    CurvePlot plot;

    // state
    juce::File currentFile;
    bool hasTable = false;
    calib::MeasurementTable measurements;
    calib::CurveSettings settings;
    calib::GenerationResult result;

    juce::String statusText { "Ready" };
    Level statusLevel = Level::info;
    juce::String sourceTitle { "No file loaded" }, sourceDetail { "Load or drop a CSV with frequency, script and reference columns" };

    std::unique_ptr<juce::FileChooser> chooser;

    // layout cache for paint()
    juce::Rectangle<int> headerArea, sourceCard, settingsCard, pointsCard, logCard, statusArea;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
