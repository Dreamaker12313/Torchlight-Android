// What the characters say, and how long they say it for. A quest's `[DIALOG]`
// block holds its NPCs' lines, one section per state: `[INTRO]`, `[RETURN]`,
// `[COMPLETE]`, `[PASSIVE]` (`[DETAILS]` is the journal entry).
//
// Two kinds of line, told apart by `FLOATYTEXT` (`CGameUI::forceInteractiveDialog`):
// a floaty one floats over the speaker's head and fades, and the `Interacted`
// the level script waits on fires at once; any other opens `CQuestDialogMenu`
// (portrait, camera on the speaker, pathing stopped) and `Interacted` fires only
// when the player closes it (`LevelScript::open_dialog()`).
//
// No OGRE and no file I/O: the renderer calls add().
#pragma once

#include "core/quest.h"

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace tl::core {

/// Strip the inline directives (`<LOOKAT=NAME>`, the only kind the data has)
/// out of a line of dialogue. Returns the text without them; `look_at`
/// receives the unit a `LOOKAT` named.
std::string strip_dialog_directives(const std::string& line, std::string* look_at = nullptr);

/// One authored line. `CQuestDialog::CQuestDialog`
/// and `parseDialogTag`'s fallbacks give the defaults below.
struct DialogLine {
    /// `INTRO`, `RETURN`, `COMPLETE` or `PASSIVE`, as the file spells it.
    std::string section;

    /// `MAKE_PET_ON_ACCEPT` (`parseDialogTag`):
    /// accepting the offer makes the speaker the player's pet
    /// (`giveOrRemoveItems` -> `CCharacter::addPet`).
    bool make_pet_on_accept = false;

    /// `REMOVE_AS_PET_ON_COMPLETE` and `DESTROY_PET` (default true,
    /// `parseDialogTag`): what `CQuestDialog::cleanUp`
    /// does to the pets the line's `UNITNAME` names -
    /// leave the player, then be hidden and deleted.
    bool remove_as_pet_on_complete = true;
    bool destroy_pet = true;

    /// `UNITNAME`, a unit's `NAME`, matched without case.
    std::string unit;

    /// One `[GIVEITEM]` or `[TAKEITEM]` group: a `CQuestDialogItem` as
    /// `CQuestDialogItem::parseDialogItemGroup`
    /// reads it, from `CQuestDialogItem::CQuestDialogItem`'s
    /// defaults.
    struct Item {
        /// `NAME`, upper-cased: the unit `getDataGroupByObjectName` finds for
        /// it (X360 `+0x8`), unless it is `GOLD`.
        std::string name;
        int count = 1;      ///< X360 `+0x14` `COUNT`: default 1, and 1 for anything below 1
        bool gold = false;  ///< X360 `+0x11`: `NAME` was `GOLD`
    };
    /// `GIVEITEM` then `TAKEITEM`: what `CQuestDialog::giveOrRemoveItems`
    /// hands over and takes back, once.
    std::vector<Item> give_items;
    std::vector<Item> take_items;

    /// Authored texts in file order; one is picked by `get_dialog()`
    /// (`CQuestDialog::getDialog`).
    std::vector<std::string> texts;

    /// The `SOUND` definition's name (not a filename).
    std::string sound;

    /// How long a boxed line is on screen, in seconds: the recording's length,
    /// floored by box_time(). A floaty line runs `kFloatySeconds`, its recording
    /// playing on until the next dialogue starts (`CQuestDialogMenu::setNPC`
    /// calls `stopDialogSound()`); a boxed line's stops when its window closes
    /// (`CQuestDialogMenu::setOpen(false)`, `update`).
    float seconds = 0.0f;

    /// `FLOATYTEXT`: spoken over the character's head rather than in a box.
    bool floaty = false;

    /// `ICONABOVEHEAD`: the quest marker over the speaker while this is its line
    /// (`CQuestManager::calculateNPCIcon`). Defaults to true
    /// (`CQuestDialog::parseDialogTag`'s fallback).
    bool icon_above_head = true;

    /// `THEMEOVERRIDE`, upper-cased: the unit theme `calculateNPCIcon` gives
    /// the speaker in place of the quest marker.
    std::string theme_override;

    /// `LOOK_AT_PLAYER`, default true. Not the inline `<LOOKAT=NAME>`
    /// (strip_dialog_directives()).
    bool look_at_player = true;

    /// `CAMERAMODX` / `Y` / `Z`, default 0: while a boxed line is open the camera
    /// frames the *speaker*, offset by this (`CGameClient::updateIngame` adds
    /// `CGameUI::getQuestDialogCameraOffset()` to `getQuestDialogNPC()`'s position).
    float camera_mod_x = 0.0f;
    float camera_mod_y = 0.0f;
    float camera_mod_z = 0.0f;
};

/// How long a floaty line stays over its speaker's head, in seconds:
/// `CQuestDialogMenu::setNPC` makes the text through
/// `CGameUI::getTextEventObject(pos, text, 1.25f, 255.0f, white, white, true)`
/// with a fade rate of 0.5, and `CTextEvent::update` takes `dt * rate * 200`
/// off the alpha: `255 / (200 * 0.5)`.
inline constexpr float kFloatySeconds = 2.55f;

/// How far above the speaker the floaty text is drawn: `CQuestDialogMenu::setNPC`
/// adds .rodata to Y alone.
inline constexpr float kFloatyHeight = 1.5f;

/// Every quest's dialogue, by quest name.
class DialogBook {
public:
    /// `quest` is matched without case, the way the data spells it in both.
    void add(const std::string& quest, DialogLine line);

    /// The line a `Quest Controller` should open, or null: the section by the
    /// quest's state, then the line by `unit`; a unit named with no line in the
    /// quest gets none.
    ///
    /// INVENTED: for a level script with no quest log (the tests); the game asks
    /// `CQuest::getQuestDialog`
    /// (`QuestLog::dialog_for`) through `LevelScript::set_quest_dialog`.
    const DialogLine* find(const std::string& quest, const std::string& unit,
                           QuestStatus status) const;

    /// The line a unit says when the player walks up and clicks them, or null.
    /// `CGameUI::updateIngameUI` finds the quest through
    /// `CQuestManager::getQuestForNPC(unit)` (every quest
    /// `CQuest::getQuestIsValidForNPC` accepts) and picks at random among
    /// several; this takes the first in file order, so a run is repeatable. The
    /// section is chosen as find() does.
    const DialogLine* find_for_unit(const std::string& unit, const QuestState& quests,
                                    std::string* quest_name = nullptr) const;

    size_t line_count() const;
    size_t quest_count() const { return by_quest_.size(); }

    /// How long a boxed line stays up, in seconds: the longer of the recording
    /// and the box's text reveal, `max(characters * 0.015, 5.0)`
    /// (`CQuestDialogMenu::setNPC`, typed out by `updateTextDisplay`). The
    /// original never times out: the player closes the box.
    static float box_time(const std::string& text, float sound_seconds);

private:
    std::map<std::string, std::vector<DialogLine>> by_quest_;
};

}  // namespace tl::core
