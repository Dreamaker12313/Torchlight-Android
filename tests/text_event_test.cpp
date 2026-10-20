// Floating text and particles: `CGameUI::getTextEventObject`, `CTextEvent::update`,
// `CCharacter::loadModel`, `playHitParticle`,
// `playStrikeParticle`, `die`.
#include "core/text_event.h"
#include "core/character_particles.h"
#include "test_support.h"

#include <cmath>
#include <string>

namespace {

using tl::core::CombatText;
using tl::core::TextEvent;

bool near(float a, float b) { return std::fabs(a - b) < 1.0e-4f; }

}  // namespace

/// 255 of alpha at 200 a second: 1.275 s, solid until the alpha is under a
/// quarter of 255.
TL_TEST(a_text_event_fades_over_its_last_quarter) {
    TextEvent event =
        tl::core::make_text_event(1.0f, 2.0f, 3.0f, "Blocked!", 1.0f, 1.0f, 0.67f, 0.16f, true);
    TL_CHECK(near(event.opacity, 1.0f));
    TL_CHECK(tl::core::update_text_event(event, 0.5f));  // alpha 155
    TL_CHECK(near(event.alpha, 155.0f));
    TL_CHECK(near(event.opacity, 1.0f));
    TL_CHECK(tl::core::update_text_event(event, 0.5f));  // alpha 55: 220 of 255
    TL_CHECK(near(event.opacity, 220.0f / 255.0f));
    TL_CHECK(tl::core::update_text_event(event, 0.25f));  // alpha 5
    TL_CHECK(near(event.opacity, 20.0f / 255.0f));
    TL_CHECK(!tl::core::update_text_event(event, 0.05f));  // gone
    TL_CHECK(near(event.alpha, 0.0f));
}

/// The position moves by the speed as it stood, and only then does the speed
/// take the delta.
TL_TEST(a_text_event_rises_faster_as_it_goes) {
    TextEvent event =
        tl::core::make_text_event(0.0f, 2.0f, 0.0f, "x", 1.0f, 1.0f, 1.0f, 1.0f, true);
    tl::core::update_text_event(event, 0.1f);
    TL_CHECK(near(event.y, 2.01f));
    TL_CHECK(near(event.rise, 0.2f));
    tl::core::update_text_event(event, 0.1f);
    TL_CHECK(near(event.y, 2.03f));
    TL_CHECK(near(event.rise, 0.3f));
}

/// The opacity is rewritten only when the alpha's integer part moves.
TL_TEST(a_text_event_keeps_its_opacity_inside_one_alpha_step) {
    TextEvent event =
        tl::core::make_text_event(0.0f, 0.0f, 0.0f, "x", 1.0f, 1.0f, 1.0f, 1.0f, true);
    event.alpha = 40.5f;
    event.opacity = 40.5f / 255.0f;
    const float before = event.opacity;
    TL_CHECK(tl::core::update_text_event(event, 0.001f));  // 40.3: still 40
    TL_CHECK(near(event.opacity, before));
    TL_CHECK(tl::core::update_text_event(event, 0.005f));  // 39.3
    TL_CHECK(near(event.opacity, 39.3f * 4.0f / 255.0f));
}

/// A still event (`moving` false) neither fades nor rises, and half speed -
/// what `setNPC` writes for a line of dialogue - doubles the life.
TL_TEST(a_text_event_obeys_its_flags_and_speed) {
    TextEvent still =
        tl::core::make_text_event(0.0f, 1.0f, 0.0f, "x", 1.25f, 1.0f, 1.0f, 1.0f, false);
    TL_CHECK(tl::core::update_text_event(still, 10.0f));
    TL_CHECK(near(still.alpha, 255.0f));
    TL_CHECK(near(still.y, 1.0f));

    TextEvent slow =
        tl::core::make_text_event(0.0f, 0.0f, 0.0f, "x", 1.25f, 1.0f, 1.0f, 1.0f, true);
    slow.speed = 0.5f;
    TL_CHECK(tl::core::update_text_event(slow, 2.5f));
    TL_CHECK(!tl::core::update_text_event(slow, 0.1f));
}

TL_TEST(combat_texts_are_written_as_their_callers_write_them) {
    TL_CHECK(tl::core::combat_text(CombatText::GlancingBlow) == "Glancing Blow!");
    TL_CHECK(tl::core::combat_text(CombatText::GlancingBlowOnPlayer) == "Glancing Blow!");
    TL_CHECK(tl::core::combat_text(CombatText::CriticalStrike) == "Critical Strike!");
    TL_CHECK(tl::core::combat_text(CombatText::Blocked) == "Blocked!");
    TL_CHECK(tl::core::combat_text(CombatText::Absorbed) == "Absorbed!");
    TL_CHECK(tl::core::combat_text(CombatText::Gold, 37) == "37 Gold");
    TL_CHECK(tl::core::combat_text(CombatText::Fame, 150) == "150 Fame");

    // A monster's glancing blow is yellow, the player's own side's red.
    TL_CHECK(near(tl::core::combat_text_style(CombatText::GlancingBlow).green, 0.77f));
    TL_CHECK(near(tl::core::combat_text_style(CombatText::GlancingBlowOnPlayer).green, 0.2f));
    TL_CHECK(near(tl::core::combat_text_style(CombatText::CriticalStrike).red, 0.196f));

    const TextEvent crit =
        tl::core::make_combat_text(CombatText::CriticalStrike, 4.0f, 1.0f, -2.0f);
    TL_CHECK(near(crit.y, 2.2f));
    TL_CHECK(near(crit.scale, 1.0f));
    TL_CHECK(near(crit.alpha, 255.0f));
    TL_CHECK(crit.fades && crit.rises);
    const TextEvent fame = tl::core::make_combat_text(CombatText::Fame, 0.0f, 1.0f, 0.0f, 20);
    TL_CHECK(near(fame.y, 2.0f));

    TL_CHECK(tl::core::text_events_shown(true));
    TL_CHECK(!tl::core::text_events_shown(false));
}

/// `CCharacter::displayDamageAbsorbed`: blows summed, rounded up, shown at most
/// every half second (the first at once).
TL_TEST(the_damage_over_a_character_is_summed_and_shown_twice_a_second) {
    tl::core::DamageDisplay shown;
    int amount = 0;
    // Nothing pending: nothing shown, and the timer runs under 0.
    TL_CHECK(!tl::core::display_damage(shown, true, 0.25f, amount));
    TL_CHECK(near(shown.timer, -0.25f));
    shown.pending += 3.2f;
    TL_CHECK(tl::core::display_damage(shown, true, 0.016f, amount));
    TL_CHECK_EQ(amount, 4);
    TL_CHECK(near(shown.timer, 0.5f));
    TL_CHECK_EQ(shown.pending, 0.0f);
    // Two blows inside the half second wait for it and come out as one.
    shown.pending += 10.0f;
    TL_CHECK(!tl::core::display_damage(shown, true, 0.25f, amount));
    shown.pending += 2.5f;
    TL_CHECK(!tl::core::display_damage(shown, true, 0.25f, amount));  // exactly 0: not under it
    TL_CHECK(tl::core::display_damage(shown, true, 0.016f, amount));
    TL_CHECK_EQ(amount, 13);
}

/// `DISPLAYDMG` off (`+0x774`): the function returns before the timer moves.
/// `forceDisplayOfDamageAbsorbed` is the same call with 10000.
TL_TEST(a_unit_without_displaydmg_shows_nothing_and_a_death_does_not_wait) {
    tl::core::DamageDisplay hidden;
    hidden.pending = 7.0f;
    hidden.timer = 0.4f;
    int amount = 0;
    TL_CHECK(!tl::core::display_damage(hidden, false, 1.0f, amount));
    TL_CHECK(near(hidden.timer, 0.4f));
    TL_CHECK_EQ(hidden.pending, 7.0f);

    tl::core::DamageDisplay shown;
    shown.pending = 41.0f;
    shown.timer = 0.5f;
    TL_CHECK(tl::core::display_damage(shown, true, tl::core::kForcedDamageDisplay, amount));
    TL_CHECK_EQ(amount, 41);
}

/// The number: `UNIT_Y` over the character, orange (1, 0.77, 0) over a
/// monster and red (1, 0.2, 0.2) over the player's side.
TL_TEST(a_damage_number_is_orange_over_a_monster_and_red_over_the_players_side) {
    const TextEvent monster = tl::core::make_damage_text(1.0f, 2.0f, 3.0f, 17, false);
    TL_CHECK_EQ(monster.text, std::string("17"));
    TL_CHECK(near(monster.y, 3.0f));
    TL_CHECK(near(monster.red, 1.0f) && near(monster.green, 0.77f) && near(monster.blue, 0.0f));
    TL_CHECK(near(monster.scale, 1.0f) && near(monster.alpha, 255.0f));
    const TextEvent mine = tl::core::make_damage_text(1.0f, 2.0f, 3.0f, 5, true);
    TL_CHECK(near(mine.red, 1.0f) && near(mine.green, 0.2f) && near(mine.blue, 0.2f));
}

/// Five engine names and the unit's own; `PARTICLE_DEATH_CENSOR` lands in the
/// explode slot and slot 4 is never filled.
TL_TEST(a_characters_particle_list_is_filled_as_load_model_fills_it) {
    namespace slot = tl::core::character_particle;
    tl::core::UnitParticles unit;
    unit.get_hit = "BLOODHIT";
    unit.death = "SKELETONDEATH";
    unit.explode = "DEATHEXPLODE";
    auto names = tl::core::character_particle_names(unit);
    TL_CHECK(names[slot::kHit] == "HIT");
    TL_CHECK(names[slot::kCritical] == "CRITICAL");
    TL_CHECK(names[slot::kBlock] == "BLOCK");
    TL_CHECK(names[slot::kTransform] == "TRANSFORM");
    TL_CHECK(names[slot::kIceExplode] == "ICEEXPLODE");
    TL_CHECK(names[slot::kGetHit] == "BLOODHIT");
    TL_CHECK(names[slot::kGetHitCensor].empty());
    TL_CHECK(names[slot::kDeath] == "SKELETONDEATH");
    TL_CHECK(names[slot::kDeathCensor].empty());
    TL_CHECK(names[slot::kExplode] == "DEATHEXPLODE");
    TL_CHECK(names[slot::kSpawn].empty());

    unit.death_censor = "DUSTDEATH";
    names = tl::core::character_particle_names(unit);
    TL_CHECK(names[slot::kDeathCensor].empty());
    TL_CHECK(names[slot::kExplodeCensor] == "DUSTDEATH");
    unit.explode_censor = "DUSTEXPLODE";
    names = tl::core::character_particle_names(unit);
    TL_CHECK(names[slot::kExplodeCensor] == "DUSTEXPLODE");
}

TL_TEST(show_blood_picks_the_particle_of_a_blow_and_of_a_death) {
    namespace slot = tl::core::character_particle;
    TL_CHECK(tl::core::hit_particle_slot(true) == slot::kGetHit);
    TL_CHECK(tl::core::hit_particle_slot(false) == slot::kGetHitCensor);
    TL_CHECK(tl::core::death_particle_slot(true) == slot::kDeath);
    TL_CHECK(tl::core::death_particle_slot(false) == slot::kDeathCensor);
    TL_CHECK(tl::core::strike_particle_slot(false, false) == slot::kHit);
    TL_CHECK(tl::core::strike_particle_slot(true, false) == slot::kCritical);
    TL_CHECK(tl::core::strike_particle_slot(true, true) == slot::kBlock);
}

/// The struck unit's radius towards the attacker along the ground, 1.25 up.
TL_TEST(a_blows_particle_stands_on_the_side_it_came_from) {
    tl::core::ParticlePoint at =
        tl::core::blow_particle_position({3.0f, 9.0f, 4.0f}, {0.0f, 1.0f, 0.0f}, 0.5f);
    TL_CHECK(near(at.x, 0.3f));
    TL_CHECK(near(at.y, 2.25f));
    TL_CHECK(near(at.z, 0.4f));
    // The attacker on top of its target: no direction, the position and 1.25.
    at = tl::core::blow_particle_position({1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f}, 0.5f);
    TL_CHECK(near(at.x, 1.0f));
    TL_CHECK(near(at.y, 1.25f));
    TL_CHECK(near(at.z, 1.0f));
}
