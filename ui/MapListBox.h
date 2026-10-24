#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>

namespace swingers
{
// A ComboBox listing drum maps.
// - beforePopup runs right before the popup shows its items, so the owner can reload the library (another
//   instance may have saved maps) and refill the list first.
// - onPick reports every item chosen in the popup, also the one already selected (juce::ComboBox notifies only
//   when the selection changes, so re-picking the shown map would do nothing). A pick updates the selection
//   without onChange, so it is reported once; onChange still reports keyboard changes. Without onPick the popup
//   behaves like a plain ComboBox.
class MapListBox : public juce::ComboBox
{
public:
    std::function<void()> beforePopup;
    std::function<void (int itemId)> onPick;

    void showPopup() override
    {
        if (beforePopup)
            beforePopup();

        // juce::ComboBox::showPopup, with our own result callback.
        auto menu = *getRootMenu();
        if (menu.getNumItems() > 0)
        {
            const auto selectedId = getSelectedId();
            for (juce::PopupMenu::MenuItemIterator iterator (menu, true); iterator.next();)
                if (auto& item = iterator.getItem(); item.itemID != 0)
                    item.isTicked = item.itemID == selectedId;
        }
        else
        {
            menu.addItem (1, getTextWhenNoChoicesAvailable(), false, false);
        }

        menu.setLookAndFeel (&getLookAndFeel());
        menu.showMenuAsync (juce::PopupMenu::Options()
                                .withTargetComponent (this)
                                .withItemThatMustBeVisible (getSelectedId())
                                .withInitiallySelectedItem (getSelectedId())
                                .withMinimumWidth (getWidth())
                                .withMaximumNumColumns (1)
                                .withStandardItemHeight (getHeight()),
                            [safe = juce::Component::SafePointer<MapListBox> (this)] (int result) {
                                if (safe != nullptr)
                                    safe->pickFromPopup (result);
                            });
    }

    // What the popup does with the chosen item (0 = dismissed). Public so tests run the same path.
    void pickFromPopup (int itemId)
    {
        hidePopup();
        if (itemId == 0)
            return;

        if (! onPick)
        {
            setSelectedId (itemId);
            return;
        }
        setSelectedId (itemId, juce::dontSendNotification);
        onPick (itemId);
    }
};
} // namespace swingers
