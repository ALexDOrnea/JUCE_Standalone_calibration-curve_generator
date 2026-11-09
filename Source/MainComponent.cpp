#include "MainComponent.h"

namespace
{
    enum Columns { colIndex = 1, colFreq, colError, colRows };

    juce::String fromStd (const std::string& s) { return juce::String::fromUTF8 (s.c_str(), (int) s.size()); }

    juce::String signedDb (double e, int decimals)
    {
        return (e >= 0.0 ? "+" : "") + juce::String (e, decimals);
    }

    void styleEditor (juce::TextEditor& ed, const juce::String& text)
    {
        ed.setText (text, false);
        ed.setFont (theme::font (14.5f));
        ed.setJustification (juce::Justification::centredLeft);
        ed.setIndents (8, 0);
        ed.setInputRestrictions (9, "0123456789.");
        ed.setSelectAllWhenFocused (true);
    }
}

//==============================================================================
MainComponent::MainComponent()
{
    setLookAndFeel (&lookAndFeel);
    setWantsKeyboardFocus (true);

    addAndMakeVisible (loadButton);
    addAndMakeVisible (exportButton);
    loadButton.setColour (juce::TextButton::buttonColourId, theme::accentDark);
    loadButton.setTooltip ("Open a measurement CSV (Ctrl+O)");
    exportButton.setTooltip ("Save the calibration curve as a .txt file (Ctrl+S)");
    exportButton.setEnabled (false);
    loadButton.onClick   = [this] { browseForCsv(); };
    exportButton.onClick = [this] { exportCalibration(); };

    for (auto* l : { &fMinLabel, &fMaxLabel, &pointsLabel })
    {
        l->setFont (theme::font (12.5f));
        l->setColour (juce::Label::textColourId, theme::textDim);
        l->setBorderSize ({ 0, 2, 0, 0 });
        addAndMakeVisible (*l);
    }

    styleEditor (fMinEditor,   calib::formatG (settings.fMin));
    styleEditor (fMaxEditor,   calib::formatG (settings.fMax));
    styleEditor (pointsEditor, juce::String (settings.numPoints));
    pointsEditor.setInputRestrictions (6, "0123456789");
    fMinEditor.setTooltip ("Lowest frequency of the output grid");
    fMaxEditor.setTooltip ("Highest frequency of the output grid");
    pointsEditor.setTooltip ("Number of logarithmically spaced output lines (300 = ~100 per decade)");

    for (auto* ed : { &fMinEditor, &fMaxEditor, &pointsEditor })
    {
        ed->onTextChange = [this] { regenerate(); };
        ed->onReturnKey  = [this] { grabKeyboardFocus(); };
        addAndMakeVisible (*ed);
    }

    resetButton.setTooltip ("Restore 20 Hz - 20 kHz, 300 points");
    resetButton.onClick = [this]
    {
        const calib::CurveSettings defaults;
        fMinEditor.setText (calib::formatG (defaults.fMin), false);
        fMaxEditor.setText (calib::formatG (defaults.fMax), false);
        pointsEditor.setText (juce::String (defaults.numPoints), false);
        regenerate();
    };
    addAndMakeVisible (resetButton);

    auto& header = table.getHeader();
    header.addColumn ("#",              colIndex, 38, 30, 60, juce::TableHeaderComponent::notSortable);
    header.addColumn ("Frequency [Hz]", colFreq, 112, 80, 200, juce::TableHeaderComponent::notSortable);
    header.addColumn ("Error [dB]",     colError, 92, 70, 200, juce::TableHeaderComponent::notSortable);
    header.addColumn ("Rows",           colRows,  56, 40, 100, juce::TableHeaderComponent::notSortable);
    header.setStretchToFitActive (true);
    header.setPopupMenuActive (false);
    table.setHeaderHeight (26);
    table.setRowHeight (24);
    addAndMakeVisible (table);

    logBox.setMultiLine (true, true);
    logBox.setReadOnly (true);
    logBox.setCaretVisible (false);
    logBox.setScrollbarsShown (true);
    logBox.setFont (theme::mono (12.5f));
    logBox.setColour (juce::TextEditor::backgroundColourId, theme::panel);
    logBox.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    logBox.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (logBox);

    plot.onPointClicked = [this] (int idx) { table.selectRow (idx); };
    addAndMakeVisible (plot);

    log ("Ready. Load a CSV: frequency, script value, reference value.");
    setSize (1280, 800);
}

MainComponent::~MainComponent()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void MainComponent::browseForCsv()
{
    const auto start = currentFile.existsAsFile() ? currentFile.getParentDirectory()
                                                  : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Open measurement CSV", start, "*.csv;*.txt;*.tsv;*");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f != juce::File())
                                  loadFile (f);
                          });
}

void MainComponent::loadFile (const juce::File& file)
{
    juce::MemoryBlock data;
    if (! file.existsAsFile() || ! file.loadFileAsData (data))
    {
        log ("Cannot read file " + file.getFullPathName(), Level::error);
        setStatus ("Cannot read file " + file.getFileName(), Level::error);
        return;
    }

    currentFile = file;
    measurements = calib::parseMeasurements (std::string (static_cast<const char*> (data.getData()), data.getSize()));
    hasTable = true;
    table.deselectAllRows();

    log ("Loaded " + file.getFileName());
    for (const auto& m : measurements.messages)
        log (fromStd (m), Level::warning);

    regenerate();

    if (result.ok)
    {
        const auto& f = result.measured.freq;
        log (juce::String ((int) f.size()) + " values read: " + calib::formatG (f.front()) + "Hz -- "
               + calib::formatG (f.back()) + "Hz", Level::success);
        if (result.heldConstantBelow || result.heldConstantAbove)
            log ("Attention: outside " + juce::String (calib::formatG (f.front())) + "--"
                   + calib::formatG (f.back()) + "Hz the error is held constant", Level::warning);
    }
    else
    {
        log (fromStd (result.errorMessage), Level::error);
    }
}

bool MainComponent::readSettings (calib::CurveSettings& out, juce::String& problem) const
{
    const auto fMinText = fMinEditor.getText().trim();
    const auto fMaxText = fMaxEditor.getText().trim();
    const auto nText    = pointsEditor.getText().trim();

    if (fMinText.isEmpty() || fMaxText.isEmpty() || nText.isEmpty())
    {
        problem = "Fill in f min, f max and the number of points";
        return false;
    }

    out.fMin = fMinText.getDoubleValue();
    out.fMax = fMaxText.getDoubleValue();
    out.numPoints = nText.getIntValue();

    if (out.fMin <= 0.0)          { problem = "f min must be greater than 0"; return false; }
    if (out.fMax <= out.fMin)     { problem = "f max must be greater than f min"; return false; }
    if (out.numPoints < 2)        { problem = "At least 2 output points are needed"; return false; }
    return true;
}

void MainComponent::regenerate()
{
    auto fail = [this] (const juce::String& message)
    {
        result = {};
        plot.clear();
        exportButton.setEnabled (false);
        table.updateContent();
        setStatus (message, Level::error);
    };

    calib::CurveSettings s;
    juce::String problem;
    const bool settingsOk = readSettings (s, problem);

    for (auto* ed : { &fMinEditor, &fMaxEditor, &pointsEditor })
        ed->setColour (juce::TextEditor::outlineColourId, settingsOk ? theme::border : theme::error.withAlpha (0.7f));
    for (auto* ed : { &fMinEditor, &fMaxEditor, &pointsEditor })
        ed->repaint();

    if (! hasTable)
    {
        if (! settingsOk)
            setStatus (problem, Level::error);
        else
            setStatus ("Ready", Level::info);
        return;
    }

    if (! settingsOk)
        return fail (problem);

    settings = s;
    result = calib::generate (measurements, settings);

    const auto rows = (int) measurements.freq.size();
    sourceTitle = currentFile.getFileName();
    sourceDetail = juce::String (rows) + " rows";
    if (! measurements.messages.empty())
        sourceDetail << ", " << (int) measurements.messages.size() << " ignored";

    if (! result.ok)
    {
        sourceDetail << "  |  " << (int) result.measured.freq.size() << " frequencies";
        repaint (sourceCard);
        return fail (fromStd (result.errorMessage));
    }

    const auto& f = result.measured.freq;
    sourceDetail << "  |  " << (int) f.size() << " frequencies, "
                 << calib::formatG (f.front()) << " Hz - " << calib::formatG (f.back()) << " Hz";

    table.updateContent();
    table.repaint();
    plot.setResult (result, settings);
    exportButton.setEnabled (true);

    juce::String status;
    status << (int) result.curve.freq.size() << " lines, " << calib::formatG (settings.fMin) << " - "
           << calib::formatG (settings.fMax) << " Hz, logarithmic grid";
    if (result.heldConstantBelow || result.heldConstantAbove)
        setStatus (status + "   -   error held constant outside " + calib::formatG (f.front()) + " - "
                     + calib::formatG (f.back()) + " Hz", Level::warning);
    else
        setStatus (status, Level::success);

    repaint (sourceCard);
}

void MainComponent::exportCalibration()
{
    if (! result.ok)
        return;

    const auto dir = currentFile.existsAsFile() ? currentFile.getParentDirectory()
                                                : juce::File::getSpecialLocation (juce::File::userDocumentsDirectory);

    chooser = std::make_unique<juce::FileChooser> ("Save calibration file",
                                                   dir.getChildFile ("generated_calibration.txt"), "*.txt");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode
                            | juce::FileBrowserComponent::canSelectFiles
                            | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
                          {
                              auto f = fc.getResult();
                              if (f == juce::File())
                                  return;

                              if (! f.getFileName().containsChar ('.'))
                                  f = f.withFileExtension ("txt");

                              const auto text = fromStd (calib::formatCalibration (result.curve));
                              if (f.replaceWithText (text, false, false, "\n"))
                              {
                                  juce::String msg;
                                  msg << "Saved: " << f.getFullPathName() << " (" << (int) result.curve.freq.size()
                                      << " lines, " << calib::formatG (settings.fMin) << "-"
                                      << calib::formatG (settings.fMax) << " Hz, logarithmic grid)";
                                  log (msg, Level::success);
                                  setStatus ("Saved " + f.getFileName(), Level::success);
                              }
                              else
                              {
                                  log ("Cannot write " + f.getFullPathName(), Level::error);
                                  setStatus ("Cannot write " + f.getFileName(), Level::error);
                              }
                          });
}

//==============================================================================
juce::Colour MainComponent::colourFor (Level level)
{
    switch (level)
    {
        case Level::success: return theme::success;
        case Level::warning: return theme::warning;
        case Level::error:   return theme::error;
        case Level::info:    break;
    }
    return theme::textDim;
}

void MainComponent::log (const juce::String& text, Level level)
{
    const auto time = juce::Time::getCurrentTime().formatted ("%H:%M:%S  ");
    logBox.moveCaretToEnd();
    logBox.setColour (juce::TextEditor::textColourId, theme::textDim.withAlpha (0.6f));
    logBox.insertTextAtCaret (time);
    logBox.setColour (juce::TextEditor::textColourId, level == Level::info ? theme::text : colourFor (level));
    logBox.insertTextAtCaret (text + "\n");
}

void MainComponent::setStatus (const juce::String& text, Level level)
{
    statusText = text;
    statusLevel = level;
    repaint (statusArea);
}

//==============================================================================
void MainComponent::paintCard (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title) const
{
    auto rf = r.toFloat().reduced (0.5f);
    g.setColour (theme::panel);
    g.fillRoundedRectangle (rf, 12.0f);
    g.setColour (theme::border);
    g.drawRoundedRectangle (rf, 12.0f, 1.0f);

    g.setColour (theme::textDim);
    g.setFont (theme::font (11.5f, true));
    g.drawText (title.toUpperCase(), r.reduced (14, 0).removeFromTop (30), juce::Justification::centredLeft);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (theme::background);

    // header
    g.setGradientFill (juce::ColourGradient::vertical (theme::header.brighter (0.04f), (float) headerArea.getY(),
                                                       theme::header, (float) headerArea.getBottom()));
    g.fillRect (headerArea);
    g.setColour (theme::border);
    g.drawHorizontalLine (headerArea.getBottom() - 1, 0.0f, (float) getWidth());

    {
        auto h = headerArea.reduced (20, 0);
        auto logo = h.removeFromLeft (36).withSizeKeepingCentre (36, 36).toFloat();
        g.setGradientFill (juce::ColourGradient (theme::accent, logo.getTopLeft(), theme::accentDark, logo.getBottomRight(), false));
        g.fillRoundedRectangle (logo, 9.0f);

        juce::Path p;   // small calibration-curve glyph
        auto li = logo.reduced (8.0f);
        p.startNewSubPath (li.getX(), li.getCentreY() + 4.0f);
        p.cubicTo (li.getX() + li.getWidth() * 0.3f, li.getY(),
                   li.getX() + li.getWidth() * 0.6f, li.getBottom(),
                   li.getRight(), li.getCentreY() - 4.0f);
        g.setColour (juce::Colours::white);
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        h.removeFromLeft (12);
        auto titleArea = h.removeFromLeft (500);
        g.setColour (theme::text);
        g.setFont (theme::font (19.0f, true));
        g.drawText ("Calibration Curve Generator", titleArea.removeFromTop (titleArea.getHeight() / 2 + 2),
                    juce::Justification::bottomLeft);
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.5f));
        g.drawText ("Measured error (script - reference)  ->  PCHIP interpolation on a log-frequency grid",
                    titleArea.withTrimmedTop (2), juce::Justification::topLeft);
    }

    // cards
    paintCard (g, sourceCard, "Source");
    {
        auto c = sourceCard.reduced (14, 0).withTrimmedTop (28).withTrimmedBottom (10);
        g.setColour (hasTable ? theme::text : theme::textDim);
        g.setFont (theme::font (15.5f, true));
        g.drawFittedText (sourceTitle, c.removeFromTop (22), juce::Justification::centredLeft, 1);
        g.setColour (theme::textDim);
        g.setFont (theme::font (12.5f));
        g.drawFittedText (sourceDetail, c, juce::Justification::topLeft, 2);
    }
    paintCard (g, settingsCard, "Output grid");
    paintCard (g, pointsCard, "Measured error");
    paintCard (g, logCard, "Log");

    // status bar
    g.setColour (theme::header);
    g.fillRect (statusArea);
    g.setColour (theme::border);
    g.drawHorizontalLine (statusArea.getY(), 0.0f, (float) getWidth());

    auto s = statusArea.reduced (16, 0);
    auto dot = s.removeFromLeft (10).withSizeKeepingCentre (8, 8).toFloat();
    g.setColour (colourFor (statusLevel));
    g.fillEllipse (dot);
    s.removeFromLeft (8);

    g.setFont (theme::font (12.5f));
    g.setColour (theme::textDim);
    const juce::String hint = "Ctrl+O open   |   Ctrl+S export   |   v" JUCE_APPLICATION_VERSION_STRING;
    g.drawText (hint, s, juce::Justification::centredRight);
    g.setColour (statusLevel == Level::info ? theme::textDim : theme::text);
    g.drawText (statusText, s.withTrimmedRight (300), juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();

    headerArea = r.removeFromTop (64);
    statusArea = r.removeFromBottom (28);

    {
        auto h = headerArea.reduced (20, 14);
        exportButton.setBounds (h.removeFromRight (190));
        h.removeFromRight (10);
        loadButton.setBounds (h.removeFromRight (130));
    }

    r.reduce (16, 16);
    auto left = r.removeFromLeft (juce::jlimit (300, 380, getWidth() / 4));
    r.removeFromLeft (16);
    plot.setBounds (r);

    sourceCard = left.removeFromTop (92);
    left.removeFromTop (12);
    settingsCard = left.removeFromTop (100);
    left.removeFromTop (12);
    logCard = left.removeFromBottom (juce::jlimit (110, 220, left.getHeight() / 3));
    left.removeFromBottom (12);
    pointsCard = left;

    {
        auto c = settingsCard.reduced (14, 0);
        auto titleRow = c.removeFromTop (30);
        resetButton.setBounds (titleRow.removeFromRight (78).withSizeKeepingCentre (78, 22).translated (0, 2));

        c.removeFromBottom (14);
        auto editors = c.removeFromBottom (30);
        auto labels = c.removeFromBottom (20);

        const int gap = 8;
        const int w = (editors.getWidth() - 2 * gap) / 3;
        fMinLabel.setBounds (labels.removeFromLeft (w));       labels.removeFromLeft (gap);
        fMaxLabel.setBounds (labels.removeFromLeft (w));       labels.removeFromLeft (gap);
        pointsLabel.setBounds (labels);
        fMinEditor.setBounds (editors.removeFromLeft (w));     editors.removeFromLeft (gap);
        fMaxEditor.setBounds (editors.removeFromLeft (w));     editors.removeFromLeft (gap);
        pointsEditor.setBounds (editors);
    }

    table.setBounds (pointsCard.reduced (8, 0).withTrimmedTop (30).withTrimmedBottom (8));
    logBox.setBounds (logCard.reduced (8, 0).withTrimmedTop (28).withTrimmedBottom (8));
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress ('o', juce::ModifierKeys::commandModifier, 0))
    {
        browseForCsv();
        return true;
    }
    if (key == juce::KeyPress ('s', juce::ModifierKeys::commandModifier, 0))
    {
        exportCalibration();
        return true;
    }
    return false;
}

//==============================================================================
bool MainComponent::isInterestedInFileDrag (const juce::StringArray& files)
{
    return files.size() == 1 && juce::File (files[0]).existsAsFile();
}

void MainComponent::fileDragEnter (const juce::StringArray&, int, int)  { plot.setDropHighlight (true); }
void MainComponent::fileDragExit (const juce::StringArray&)             { plot.setDropHighlight (false); }

void MainComponent::filesDropped (const juce::StringArray& files, int, int)
{
    plot.setDropHighlight (false);
    if (! files.isEmpty())
        loadFile (juce::File (files[0]));
}

//==============================================================================
int MainComponent::getNumRows()
{
    return (int) result.measured.freq.size();
}

void MainComponent::paintRowBackground (juce::Graphics& g, int row, int, int, bool selected)
{
    if (selected)
        g.fillAll (theme::accentDark.withAlpha (0.55f));
    else if (row % 2 == 1)
        g.fillAll (theme::panelRaised.withAlpha (0.5f));
}

void MainComponent::paintCell (juce::Graphics& g, int row, int col, int w, int h, bool selected)
{
    if (row < 0 || row >= getNumRows())
        return;

    const auto i = (size_t) row;
    juce::String text;
    auto colour = theme::text;
    auto just = juce::Justification::centredRight;

    switch (col)
    {
        case colIndex: text = juce::String (row + 1); colour = theme::textDim; just = juce::Justification::centredLeft; break;
        case colFreq:  text = calib::formatG (result.measured.freq[i]); break;
        case colError:
        {
            const double e = result.measured.error[i];
            text = signedDb (e, 2);
            colour = selected ? theme::text : (std::abs (e) < 0.005 ? theme::textDim : theme::accent.brighter (0.2f));
            break;
        }
        case colRows:  text = juce::String (result.measured.count[i]); colour = theme::textDim; break;
        default: break;
    }

    g.setColour (colour);
    g.setFont (theme::font (13.5f));
    g.drawText (text, juce::Rectangle<int> (w, h).reduced (8, 0), just);
}

void MainComponent::selectedRowsChanged (int lastRowSelected)
{
    plot.setHighlightedPoint (lastRowSelected);
}
