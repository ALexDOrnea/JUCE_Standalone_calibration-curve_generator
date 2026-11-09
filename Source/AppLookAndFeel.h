#pragma once

#include <JuceHeader.h>

namespace theme
{
    const juce::Colour background   { 0xff0e1117 };
    const juce::Colour header       { 0xff141923 };
    const juce::Colour panel        { 0xff171c26 };
    const juce::Colour panelRaised  { 0xff1e2430 };
    const juce::Colour border       { 0xff2a3140 };
    const juce::Colour grid         { 0xff232a37 };
    const juce::Colour text         { 0xffe6e9ef };
    const juce::Colour textDim      { 0xff8b94a7 };
    const juce::Colour accent       { 0xff4fc3f7 };   // interpolated curve
    const juce::Colour accentDark   { 0xff1f6f96 };
    const juce::Colour measured     { 0xffffb74d };   // measured points
    const juce::Colour warning      { 0xffffa726 };
    const juce::Colour error        { 0xffef5350 };
    const juce::Colour success      { 0xff66bb6a };

    inline juce::Font font (float height, bool bold = false)
    {
        return juce::Font (juce::FontOptions (height, bold ? juce::Font::bold : juce::Font::plain));
    }

    inline juce::Font mono (float height)
    {
        return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), height, juce::Font::plain));
    }
}

//==============================================================================
class AppLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AppLookAndFeel()
    {
        setColourScheme ({ theme::background, theme::panel, theme::panelRaised,
                           theme::border, theme::text, theme::accent,
                           theme::background, theme::accentDark, theme::text });

        setColour (juce::ResizableWindow::backgroundColourId, theme::background);
        setColour (juce::DocumentWindow::textColourId, theme::text);

        setColour (juce::TextButton::buttonColourId, theme::panelRaised);
        setColour (juce::TextButton::buttonOnColourId, theme::accentDark);
        setColour (juce::TextButton::textColourOffId, theme::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);

        setColour (juce::TextEditor::backgroundColourId, theme::background);
        setColour (juce::TextEditor::textColourId, theme::text);
        setColour (juce::TextEditor::outlineColourId, theme::border);
        setColour (juce::TextEditor::focusedOutlineColourId, theme::accent);
        setColour (juce::TextEditor::highlightColourId, theme::accentDark);
        setColour (juce::CaretComponent::caretColourId, theme::accent);

        setColour (juce::Label::textColourId, theme::text);

        setColour (juce::ListBox::backgroundColourId, theme::panel);
        setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TableHeaderComponent::backgroundColourId, theme::panelRaised);
        setColour (juce::TableHeaderComponent::textColourId, theme::textDim);
        setColour (juce::TableHeaderComponent::outlineColourId, theme::border);
        setColour (juce::TableHeaderComponent::highlightColourId, theme::border);

        setColour (juce::ScrollBar::thumbColourId, theme::border.brighter (0.2f));
        setColour (juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);

        setColour (juce::TooltipWindow::backgroundColourId, theme::panelRaised);
        setColour (juce::TooltipWindow::textColourId, theme::text);
        setColour (juce::TooltipWindow::outlineColourId, theme::border);

        setColour (juce::AlertWindow::backgroundColourId, theme::panel);
        setColour (juce::AlertWindow::textColourId, theme::text);
        setColour (juce::AlertWindow::outlineColourId, theme::border);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& backgroundColour,
                               bool isHighlighted, bool isDown) override
    {
        auto r = button.getLocalBounds().toFloat().reduced (0.5f);
        auto base = backgroundColour;

        if (! button.isEnabled())   base = base.withMultipliedAlpha (0.4f);
        else if (isDown)            base = base.darker (0.15f);
        else if (isHighlighted)     base = base.brighter (0.12f);

        g.setColour (base);
        g.fillRoundedRectangle (r, 7.0f);

        g.setColour (button.isEnabled() ? theme::border.brighter (isHighlighted ? 0.35f : 0.1f)
                                        : theme::border.withMultipliedAlpha (0.5f));
        g.drawRoundedRectangle (r, 7.0f, 1.0f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
    {
        return theme::font (juce::jmin (15.0f, (float) buttonHeight * 0.45f), true);
    }

    void fillTextEditorBackground (juce::Graphics& g, int w, int h, juce::TextEditor& ed) override
    {
        g.setColour (ed.findColour (juce::TextEditor::backgroundColourId));
        g.fillRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h), 6.0f);
    }

    void drawTextEditorOutline (juce::Graphics& g, int w, int h, juce::TextEditor& ed) override
    {
        if (! ed.isEnabled())
            return;

        const bool focused = ed.hasKeyboardFocus (true) && ! ed.isReadOnly();
        g.setColour (ed.findColour (focused ? juce::TextEditor::focusedOutlineColourId
                                            : juce::TextEditor::outlineColourId));
        g.drawRoundedRectangle (juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f),
                                6.0f, focused ? 1.6f : 1.0f);
    }
};
