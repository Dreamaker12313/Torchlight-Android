#include "core/sound_bank.h"

namespace tl::core {

// `KSOUNDBANK_STRINGS` at `.bss, in the static initialiser's order (0x818xxx),
// misspellings and all.
const std::array<const char*, kSoundSlots> kSoundSlotKeys = {{
    "STEP_SOUND",
    "STRIKE_SOUND",
    "PHYSICALHIT_SOUND",
    "ICEHIT_SOUND",
    "FIREHIT_SOUND",
    "ELECTRICHIT_SOUND",
    "POISONHIT_SOUND",
    "SHIELDBLOCK",
    "BLOCK",
    "CRITICAL_SOUND",
    "ATTACK_SOUND",
    "IDLE_SOUND",
    "DEATH_SOUND",
    "ROAR_SOUND",
    "FLEE_SOUND",
    "MISS_SOUND",
    "FALL_SOUND",
    "LAND_SOUND",
    "TAKE_SOUND",
    "SPAWN_SOUND",
    "USE_SOUND",
    "EQUIP_SOUND",
    "OPENMENU_SOUND",
    "BUY_SOUND",
    "ERROR_SOUND",
    "LEVELUP_SOUND",
    "EFFORT_SOUND",
    "SPENDPOINT_SOUND",
    "PORTALENTER_SOUND",
    "POERTALEXIT_SOUND",
    "SKILLASSIGN_SOUND",
    "QUESTRECEIVED_SOUND",
    "QUESTCOMPLETED_SOUND",
    "LOWMANA_SOUND",
    "MISSILEREFLECT_SOUND",
    "BADEFFECT_SOUND",
    "GOODEFFECT_SOUND",
    "NPCDIALOG_SOUND",
    "NPCGREET",
    "NPCBYE",
    "NPCPURCHASE",
    "NPCSELL",
    "VOICEPETINVENTORYFULL_SOUND",
    "VOICEPETDEPARTED_SOUND",
    "VOICEPETRETURNED_SOUND",
    "VOICEPETFLED_SOUND",
    "VOICEPETLEVELUP_SOUND",
    "VOICEINVENTORYFULL_SOUND",
    "VOICEINSUFFICIENTGOLD_SOUND",
    "VOICEIMPOSSIBLE_SOUND",
    "VOICECANTCAST_SOUND",
    "VOICECANTCASTHERE_SOUND",
    "VOICEHEALTHLOW_SOUND",
    "VOICEMANALOW_SOUND",
    "VOICETRAPSPRUNG_SOUND",
    "VOICEREFRESHED_SOUND",
    "VOICEPOWERFUL_SOUND",
    "VOICEHEALTHY_SOUND",
    "VOICEILL_SOUND",
    "VOICEPOISONED_SOUND",
    "VOICELEVELUP_SOUND",
    "VOICEFAMEUP_SOUND",
    "VOICESKILLUP_SOUND",
    "VOICEFORTUNEGOOD_SOUND",
    "VOICEFORTUNEBAD_SOUND",
    "VOICESPELLLEARNED_SOUND",
    "",  // slot 66 - the menu-close sound, which the data cannot reach
}};

// PORT: a diagnostic count of a unit's sound slots.
size_t SoundBank::filled() const {
    size_t n = 0;
    for (const std::string& name : names_) {
        if (!name.empty()) ++n;
    }
    return n;
}

bool VoiceQueue::queue(Entry entry, bool channel_playing) {
    // the slot playing, while its channel sounds.
    if (playing_slot_ == entry.slot && channel_playing) return false;
    // a slot already queued.
    for (const Entry& queued : queue_) {
        if (queued.slot == entry.slot) return false;
    }
    queue_.push_back(std::move(entry));
    return true;
}

bool VoiceQueue::update(float delta, bool channel_playing, Entry& out) {
    if (queue_.empty()) {
        // a channel that has gone silent is forgotten.
        if (!channel_playing) {
            channel_.clear();
            waited_ = 0.0f;
        }
        return false;
    }
    // a channel still sounding holds the queue.
    if (!channel_.empty() && channel_playing) return false;
    waited_ = waited_ + delta;
    if (waited_ < queue_.front().delay) return false;
    waited_ = 0.0f;
    out = queue_.front();
    playing_slot_ = out.slot;
    queue_.erase(queue_.begin());
    return true;
}

// PORT: the voice queue emptied.
void VoiceQueue::clear() {
    queue_.clear();
    channel_.clear();
    waited_ = 0.0f;
}

}  // namespace tl::core
