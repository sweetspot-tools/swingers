#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace swingers
{
// A ComboBox listing drum maps that calls beforePopup right before it shows its items, so the owner can reload
// the library (another instance may have saved maps) and refill the list first.
class MapListBox : public juce::ComboBox
{
public:
    std::function<void()> beforePopup;

    void showPopup() override
    {
        if (beforePopup)
            beforePopup();
        juce::ComboBox::showPopup();
    }
};
} // namespace swingers
