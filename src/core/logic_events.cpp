// The event lists, and the names of gINPUT_EVENT_NAMES / gOUTPUT_EVENTS_NAMES.
#include "core/logic_events.h"

namespace tl::core::logic {
namespace {

const char* const kInputNames[] = {
    "Show",                            // 0x00
    "Hide",                            // 0x01
    "Enable",                          // 0x02
    "Disable",                         // 0x03
    "Enable and Show",                 // 0x04
    "Disable and Hide",                // 0x05
    "Reset",                           // 0x06
    "Add",                             // 0x07
    "Subtract",                        // 0x08
    "Play",                            // 0x09
    "Stop",                            // 0x0a
    "Pause",                           // 0x0b
    "Resume",                          // 0x0c
    "Play Level Music",                // 0x0d
    "Increment",                       // 0x0e
    "Activate",                        // 0x0f
    "Start Particle",                  // 0x10
    "Stop Particle",                   // 0x11
    "Force Stop Particle",             // 0x12
    "Pause Particle",                  // 0x13
    "Resume Particle",                 // 0x14
    "Play",                            // 0x15
    "Play Backwards",                  // 0x16
    "Stop",                            // 0x17
    "Stop to End",                     // 0x18
    "Pause",                           // 0x19
    "Reset",                           // 0x1a
    "Reset To End",                    // 0x1b
    "Fast Forward To End",             // 0x1c
    "Rewind to Start",                 // 0x1d
    "Set State One",                   // 0x1e
    "Set State Two",                   // 0x1f
    "Spawn Units",                     // 0x20
    "Destroy Spawned Units",           // 0x21
    "Hide And Disable Spawned Units",  // 0x22
    "Increment Level Delta",           // 0x23
    "Decrement Level Delta",           // 0x24
    "Activate Warper",                 // 0x25
    "Activate Teleport",               // 0x26
    "Roll",                            // 0x27
    "Input 1",                         // 0x28
    "Input 2",                         // 0x29
    "Input 3",                         // 0x2a
    "Input 4",                         // 0x2b
    "Input 5",                         // 0x2c
    "Input 6",                         // 0x2d
    "Trigger",                         // 0x2e
    "Toggle",                          // 0x2f
    "Interact",                        // 0x30
    "Make Invulnerable",               // 0x31
    "Make Vulnerable",                 // 0x32
    "Start Camera",                    // 0x33
    "Camera Off",                      // 0x34
    "Force Accept",                    // 0x35
    "Force Not Accepted",              // 0x36
    "Force Complete",                  // 0x37
    "Force Not Complete",              // 0x38
    "Start Skill",                     // 0x39
    "Stop Skill",                      // 0x3a
    "Learn Skill",                     // 0x3b
    "Unlearn Skill",                   // 0x3c
    "Alert Monster",                   // 0x3d
    "Enable Targeting",                // 0x3e
    "Disable Targeting",               // 0x3f
    "Enable Targeting & Alert",        // 0x40
    "Hunt",                            // 0x41
    "Play",                            // 0x42
    "Play Looping",                    // 0x43
    "Stop",                            // 0x44
    "Stop and Idle",                   // 0x45
    "Cannot be Targeted",              // 0x46
    "Can be Targeted",                 // 0x47
    "Add as Pet",                      // 0x48
    "Remove as Pet",                   // 0x49
    "Kill Monster",                    // 0x4a
    "Warp Pets to Player",             // 0x4b
    "Heal Player",                     // 0x4c
    "Collidable",                      // 0x4d
    "Not Collidable",                  // 0x4e
    "Take Money",                      // 0x4f
    "Show Tip",                        // 0x50
    "Clear History",                   // 0x51
    "Stop Skills",                     // 0x52
    "Kill Pets",                       // 0x53
    "Stop",                            // 0x54
    "Start",                           // 0x55
    "Pause",                           // 0x56
    "Input1",                          // 0x57
    "Input2",                          // 0x58
    "Input3",                          // 0x59
    "Input4",                          // 0x5a
    "Input5",                          // 0x5b
};

const char* const kOutputNames[] = {
    "Triggered",                    // 0x00
    "Triggered First Time",         // 0x01
    "Deactivated",                  // 0x02
    "Deactivated First Time",       // 0x03
    "On Visible",                   // 0x04
    "On Invisible",                 // 0x05
    "Enabled",                      // 0x06
    "Disabled",                     // 0x07
    "Activated",                    // 0x08
    "Reset",                        // 0x09
    "Initialized",                  // 0x0a
    "Playing",                      // 0x0b
    "Stopped",                      // 0x0c
    "Sound Ended",                  // 0x0d
    "Paused",                       // 0x0e
    "Resumed",                      // 0x0f
    "Incremented",                  // 0x10
    "First Increment",              // 0x11
    "Second Increment",             // 0x12
    "Third Increment",              // 0x13
    "Fourth Increment",             // 0x14
    "Fifth Increment",              // 0x15
    "Increment Greater Then Five",  // 0x16
    "Monsters Spawned",             // 0x17
    "Monster Killed",               // 0x18
    "All Monsters Dead",            // 0x19
    "All Units Spawned",            // 0x1a
    "Item Picked Up",               // 0x1b
    "All Items Picked Up",          // 0x1c
    "Item Interacted",              // 0x1d
    "All Items Interacted With",    // 0x1e
    "Particle Started",             // 0x1f
    "Particle Stopped",             // 0x20
    "Particle Paused",              // 0x21
    "Particle Resumed",             // 0x22
    "Stopped",                      // 0x23
    "Started",                      // 0x24
    "Paused",                       // 0x25
    "Reset to Beginning",           // 0x26
    "Reset to End",                 // 0x27
    "Looped",                       // 0x28
    "Started Backwards",            // 0x29
    "Started Forwards",             // 0x2a
    "Stopped Backwards",            // 0x2b
    "Stopped Forwards",             // 0x2c
    "Finished",                     // 0x2d
    "State One",                    // 0x2e
    "State Two",                    // 0x2f
    "Activation Failed",            // 0x30
    "One",                          // 0x31
    "Two",                          // 0x32
    "Three",                        // 0x33
    "Four",                         // 0x34
    "Five",                         // 0x35
    "FAILED",                       // 0x36
    "SUCCESS",                      // 0x37
    "Interacted with Unit",         // 0x38
    "HP 90 PCT",                    // 0x39
    "HP 80 PCT",                    // 0x3a
    "HP 70 PCT",                    // 0x3b
    "HP 60 PCT",                    // 0x3c
    "HP 50 PCT",                    // 0x3d
    "HP 40 PCT",                    // 0x3e
    "HP 30 PCT",                    // 0x3f
    "HP 20 PCT",                    // 0x40
    "HP 10 PCT",                    // 0x41
    "Monster Alerted",              // 0x42
    "Player HP Below 90 PCT",       // 0x43
    "Player HP Below 80 PCT",       // 0x44
    "Player HP Below 70 PCT",       // 0x45
    "Player HP Below 60 PCT",       // 0x46
    "Player HP Below 50 PCT",       // 0x47
    "Player HP Below 40 PCT",       // 0x48
    "Player HP Below 30 PCT",       // 0x49
    "Player HP Below 20 PCT",       // 0x4a
    "Player HP Below 10 PCT",       // 0x4b
    "Player HP Above 90 PCT",       // 0x4c
    "Player HP Above 80 PCT",       // 0x4d
    "Player HP Above 70 PCT",       // 0x4e
    "Player HP Above 60 PCT",       // 0x4f
    "Player HP Above 50 PCT",       // 0x50
    "Player HP Above 40 PCT",       // 0x51
    "Player HP Above 30 PCT",       // 0x52
    "Player HP Above 20 PCT",       // 0x53
    "Player HP Above 10 PCT",       // 0x54
    "Accepted",                     // 0x55
    "Declined",                     // 0x56
    "Camera Moving",                // 0x57
    "Camera Stopped",               // 0x58
    "Camera Pausing",               // 0x59
    "Camera Control Restored",      // 0x5a
    "Interacting",                  // 0x5b
    "Interacted",                   // 0x5c
    "Interacted Accepted",          // 0x5d
    "Interacted Declined",          // 0x5e
    "Interacted Closed",            // 0x5f
    "Invulnerable",                 // 0x60
    "Vulnerable",                   // 0x61
    "Quest Active",                 // 0x62
    "Quest Not Active",             // 0x63
    "Quest Complete",               // 0x64
    "Quest Not Complete",           // 0x65
    "Quest Abandoned",              // 0x66
    "Skill Started",                // 0x67
    "Skill Stopped",                // 0x68
    "Skill Learned",                // 0x69
    "Skill Unlearned",              // 0x6a
    "Item Dropped",                 // 0x6b
    "Item Equipped",                // 0x6c
    "Item Unequipped",              // 0x6d
    "End of Path Reached",          // 0x6e
    "Clicked",                      // 0x6f
    "Animation Stopped",            // 0x70
    "Animation Playing",            // 0x71
    "Skip Cutscene",                // 0x72
    "Level Activated",              // 0x73
    "Insufficient funds",           // 0x74
    "Money Taken",                  // 0x75
    "Stop",                         // 0x76
    "Start",                        // 0x77
    "Pause",                        // 0x78
    "Output 1",                     // 0x79
    "Output 2",                     // 0x7a
    "Output 3",                     // 0x7b
    "Output 4",                     // 0x7c
    "Output 5",                     // 0x7d
};

const uint32_t kGroupIn[] = {0x00, 0x01};
const uint32_t kGroupOut[] = {0x04, 0x05};
const uint32_t kMonsterIn[] = {0x30, 0x31, 0x32, 0x3d, 0x3e, 0x3f, 0x40, 0x02, 0x03, 0x41,
                               0x00, 0x01, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x52, 0x53};
const uint32_t kMonsterOut[] = {0x5b, 0x60, 0x61, 0x39, 0x3a, 0x3b, 0x3c, 0x3d,
                                0x3e, 0x3f, 0x40, 0x41, 0x18, 0x42, 0x5c, 0x5d,
                                0x5e, 0x5f, 0x06, 0x07, 0x6e, 0x05, 0x04};
const uint32_t kUnitTriggerIn[] = {0x02, 0x03, 0x4d, 0x4e, 0x06, 0x06, 0x2e, 0x1e, 0x1f};
const uint32_t kUnitTriggerOut[] = {0x06, 0x07, 0x09, 0x00, 0x2e, 0x2f, 0x30};
const uint32_t kUnitSpawnerIn[] = {0x20, 0x0a, 0x21, 0x22, 0x23, 0x24};
const uint32_t kUnitSpawnerOut[] = {0x1a, 0x18, 0x19, 0x1b, 0x1c, 0x1d, 0x1e};
const uint32_t kLayoutLinkIn[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05};
const uint32_t kLayoutLinkOut[] = {0x04, 0x05, 0x06, 0x07};
const uint32_t kLayoutLinkParticleIn[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x10, 0x11, 0x12};
const uint32_t kLayoutLinkParticleOut[] = {0x04, 0x05, 0x06, 0x07, 0x1f, 0x20};
const uint32_t kLayoutLinkTimelineIn[] = {0x00, 0x01, 0x02, 0x03, 0x04,
                                          0x05, 0x15, 0x16, 0x17, 0x19};
const uint32_t kLayoutLinkTimelineOut[] = {0x04, 0x05, 0x06, 0x07, 0x24, 0x2a, 0x29,
                                           0x23, 0x2c, 0x2b, 0x28, 0x25, 0x26, 0x27};
const uint32_t kGenericModelIn[] = {0x00, 0x01};
const uint32_t kGenericModelOut[] = {0x04, 0x05};
const uint32_t kRoomPieceIn[] = {0x00, 0x01};
const uint32_t kRoomPieceOut[] = {0x04, 0x05};
const uint32_t kLightIn[] = {0x00, 0x01};
const uint32_t kLightOut[] = {0x04, 0x05};
const uint32_t kSoundIn[] = {0x09, 0x0a, 0x0b, 0x0c};
const uint32_t kSoundOut[] = {0x0b, 0x0c, 0x0d, 0x0e, 0x0f};
const uint32_t kMusicIn[] = {0x09, 0x0a, 0x0d};
const uint32_t kMusicOut[] = {0x0b, 0x0c, 0x0d};
const uint32_t kParticleIn[] = {0x10, 0x11};
const uint32_t kParticleOut[] = {kNone};
const uint32_t kAlignIn[] = {0x02, 0x03};
const uint32_t kAlignOut[] = {kNone};
const uint32_t kTimelineIn[] = {0x02, 0x03, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d};
const uint32_t kTimelineOut[] = {0x06, 0x07, 0x24, 0x2a, 0x29, 0x23, 0x2c,
                                 0x2b, 0x28, 0x25, 0x26, 0x27, 0x2d};
const uint32_t kLogicGroupIn[] = {0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b};
const uint32_t kLogicGroupOut[] = {0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x73};
const uint32_t kPlayerSphereTriggerIn[] = {0x02, 0x03, 0x06};
const uint32_t kPlayerSphereTriggerOut[] = {0x06, 0x07, 0x09, 0x00, 0x01, 0x02, 0x03};
const uint32_t kPlayerBoxTriggerIn[] = {0x02, 0x03, 0x06};
const uint32_t kPlayerBoxTriggerOut[] = {0x06, 0x07, 0x09, 0x00, 0x01, 0x02, 0x03};
const uint32_t kCounterIn[] = {0x07, 0x08, 0x02, 0x03, 0x06};
const uint32_t kCounterOut[] = {0x06, 0x07, 0x09, 0x08};
const uint32_t kRandomChoiceIn[] = {0x27};
const uint32_t kRandomChoiceOut[] = {0x31, 0x32, 0x33, 0x34, 0x35};
const uint32_t kWarperIn[] = {0x00, 0x01, 0x25, 0x02, 0x03};
const uint32_t kWarperOut[] = {0x04, 0x05};
const uint32_t kTeleportIn[] = {0x26, 0x02, 0x03};
const uint32_t kTeleportOut[] = {0x08};
const uint32_t kTimerIn[] = {0x02, 0x03, 0x06};
const uint32_t kTimerOut[] = {0x06, 0x07, 0x08};
const uint32_t kOutputIncrementorIn[] = {0x0e, 0x02, 0x03};
const uint32_t kOutputIncrementorOut[] = {0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x06, 0x07};
const uint32_t kCameraShakeIn[] = {0x0f};
const uint32_t kCameraShakeOut[] = {kNone};
const uint32_t kPropertyNodeIn[] = {0x00, 0x01, 0x02, 0x03};
const uint32_t kPropertyNodeOut[] = {0x04, 0x05, 0x06, 0x07};
const uint32_t kPuzzleInputIn[] = {0x02, 0x03, 0x06, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d};
const uint32_t kPuzzleInputOut[] = {0x09, 0x37, 0x36};
const uint32_t kCameraControllerIn[] = {0x33, 0x34};
const uint32_t kCameraControllerOut[] = {0x57, 0x58, 0x59, 0x5a};
const uint32_t kGameStateControllerIn[] = {0x02, 0x03, 0x4b, 0x4c, 0x50};
const uint32_t kGameStateControllerOut[] = {0x06, 0x07, 0x43, 0x44, 0x45, 0x46,
                                            0x47, 0x48, 0x49, 0x4a, 0x4b};
const uint32_t kQuestControllerIn[] = {0x30, 0x35, 0x36, 0x37, 0x38};
const uint32_t kQuestControllerOut[] = {0x5b, 0x5c, 0x5d, 0x5e, 0x62, 0x63, 0x64, 0x65, 0x66};
const uint32_t kSkillControllerIn[] = {0x39, 0x3a, 0x3b, 0x3c};
const uint32_t kSkillControllerOut[] = {0x67, 0x68, 0x69, 0x6a};
const uint32_t kPathingIn[] = {0x02, 0x03};
const uint32_t kPathingOut[] = {0x06, 0x07, 0x6e};
const uint32_t kAnimationControllerIn[] = {0x42, 0x43, 0x44, 0x45};
const uint32_t kAnimationControllerOut[] = {0x70, 0x71};
const uint32_t kSkipCutsceneIn[] = {0x02, 0x03};
const uint32_t kSkipCutsceneOut[] = {0x06, 0x07, 0x72};
const uint32_t kDamageShapeIn[] = {0x02, 0x03};
const uint32_t kDamageShapeOut[] = {kNone};
const uint32_t kCinematicIn[] = {0x09};
const uint32_t kCinematicOut[] = {0x5c};
const uint32_t kWaypointActivatorIn[] = {0x00, 0x01, 0x0f};
// `CMoneyTakerDescriptor::InputLogicEvent` hears `Take Money`;
// `CMoneyTaker::takeMoney` answers with one of the two.
const uint32_t kMoneyTakerIn[] = {0x4f};
const uint32_t kMoneyTakerOut[] = {0x74, 0x75};
const uint32_t kWaypointActivatorOut[] = {0x04, 0x05};
// `CDungeonObjectDescriptor::CDungeonObjectDescriptor`.
const uint32_t kDungeonObjectIn[] = {0x00, 0x01, 0x51};
const uint32_t kDungeonObjectOut[] = {0x04, 0x05};

struct Entry {
    uint32_t node_type;
    Vocabulary vocabulary;
};

// By the name `CLayoutScene::createDescriptors` registers
// each descriptor under. The three layout links are one CLayoutDescriptor
// built per link type.
const Entry kTable[] = {
    {0, {"Group", kGroupIn, 2, kGroupOut, 2}},
    {1, {"Monster", kMonsterIn, 19, kMonsterOut, 23}},
    {3, {"Unit Trigger", kUnitTriggerIn, 9, kUnitTriggerOut, 7}},
    {4, {"Unit Spawner", kUnitSpawnerIn, 6, kUnitSpawnerOut, 7}},
    {5, {"Layout Link", kLayoutLinkIn, 6, kLayoutLinkOut, 4}},
    {6, {"Layout Link Particle", kLayoutLinkParticleIn, 9, kLayoutLinkParticleOut, 6}},
    {7, {"Layout Link Timeline", kLayoutLinkTimelineIn, 10, kLayoutLinkTimelineOut, 14}},
    {9, {"Generic Model", kGenericModelIn, 2, kGenericModelOut, 2}},
    {10, {"Room Piece", kRoomPieceIn, 2, kRoomPieceOut, 2}},
    {11, {"Light", kLightIn, 2, kLightOut, 2}},
    {12, {"Sound", kSoundIn, 4, kSoundOut, 5}},
    {13, {"Music", kMusicIn, 3, kMusicOut, 3}},
    {17, {"Particle", kParticleIn, 2, kParticleOut, 0}},
    {19, {"Align", kAlignIn, 2, kAlignOut, 0}},
    {34, {"Timeline", kTimelineIn, 11, kTimelineOut, 13}},
    {35, {"Logic Group", kLogicGroupIn, 8, kLogicGroupOut, 9}},
    {36, {"Player Sphere Trigger", kPlayerSphereTriggerIn, 3, kPlayerSphereTriggerOut, 7}},
    {37, {"Player Box Trigger", kPlayerBoxTriggerIn, 3, kPlayerBoxTriggerOut, 7}},
    {38, {"Counter", kCounterIn, 5, kCounterOut, 4}},
    {39, {"Random Choice", kRandomChoiceIn, 1, kRandomChoiceOut, 5}},
    {40, {"Warper", kWarperIn, 5, kWarperOut, 2}},
    {41, {"Teleport", kTeleportIn, 3, kTeleportOut, 1}},
    {42, {"Timer", kTimerIn, 3, kTimerOut, 3}},
    {43, {"Output Incrementor", kOutputIncrementorIn, 3, kOutputIncrementorOut, 9}},
    {44, {"Camera Shake", kCameraShakeIn, 1, kCameraShakeOut, 0}},
    {45, {"Property Node", kPropertyNodeIn, 4, kPropertyNodeOut, 4}},
    {46, {"Puzzle Input", kPuzzleInputIn, 9, kPuzzleInputOut, 3}},
    {47, {"Camera Controller", kCameraControllerIn, 2, kCameraControllerOut, 4}},
    {48, {"Game State Controller", kGameStateControllerIn, 5, kGameStateControllerOut, 11}},
    {49, {"Quest Controller", kQuestControllerIn, 5, kQuestControllerOut, 9}},
    {50, {"Skill Controller", kSkillControllerIn, 4, kSkillControllerOut, 4}},
    {51, {"Pathing", kPathingIn, 2, kPathingOut, 3}},
    {53, {"Animation Controller", kAnimationControllerIn, 4, kAnimationControllerOut, 2}},
    {54, {"Skip Cutscene", kSkipCutsceneIn, 2, kSkipCutsceneOut, 3}},
    {55, {"Damage Shape", kDamageShapeIn, 2, kDamageShapeOut, 0}},
    {56, {"Cinematic", kCinematicIn, 1, kCinematicOut, 1}},
    {57, {"Money Taker", kMoneyTakerIn, 1, kMoneyTakerOut, 2}},
    {58, {"Dungeon Object", kDungeonObjectIn, 3, kDungeonObjectOut, 2}},
    {59, {"Waypoint Activator", kWaypointActivatorIn, 3, kWaypointActivatorOut, 2}},
};

const Vocabulary kUnknown{};

}  // namespace

// PORT: a layout input's name, for the trace and the text layouts.
const char* input_name(uint32_t id) {
    return id < sizeof(kInputNames) / sizeof(kInputNames[0]) ? kInputNames[id] : "?";
}

// PORT: a layout output's name, for the trace and the text layouts.
const char* output_name(uint32_t id) {
    return id < sizeof(kOutputNames) / sizeof(kOutputNames[0]) ? kOutputNames[id] : "?";
}

// PORT: a node kind's input by name (text layouts).
uint32_t Vocabulary::input_by_name(const std::string& name) const {
    for (size_t i = 0; i < input_count; ++i) {
        if (name == input_name(inputs[i])) return inputs[i];
    }
    return kNone;
}

// PORT: a node kind's output by name (text layouts).
uint32_t Vocabulary::output_by_name(const std::string& name) const {
    for (size_t i = 0; i < output_count; ++i) {
        if (name == output_name(outputs[i])) return outputs[i];
    }
    return kNone;
}

// PORT: an accessor of a node kind's inputs.
uint32_t Vocabulary::input_at(size_t index) const {
    return index < input_count ? inputs[index] : kNone;
}

// PORT: an accessor of a node kind's outputs.
uint32_t Vocabulary::output_at(size_t index) const {
    return index < output_count ? outputs[index] : kNone;
}

// PORT: a node kind's inputs and outputs.
const Vocabulary& vocabulary(uint32_t node_type) {
    for (const Entry& entry : kTable) {
        if (entry.node_type == node_type) return entry.vocabulary;
    }
    return kUnknown;
}

}  // namespace tl::core::logic
