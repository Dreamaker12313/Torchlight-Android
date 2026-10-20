// Generated from the Xbox image's descriptor
// constructors; do not edit by hand. Included by formats/layout_schema.h only.
// clang-format off

// "Align": `CAlignDescriptor::CAlignDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CAlignDescriptorParam1`.
inline constexpr Property kProps0[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TO VELOCITY", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"STRETCH", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn0[] = {2, 3};
inline constexpr uint32_t kOut0[] = {0};

// "Animation Controller": `CAnimationPlayerDescriptor::CAnimationPlayerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CAnimationPlayerDescriptorParam1`.
inline constexpr Property kProps1[] = {
    {"START ON LOAD", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"BLEND TIME", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"BLEND OUT TIME", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"DURATION OVERRIDE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"PLAY IDLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ANIMATION", "ANIMATION", VariableType::String, 0x0u, false},
    {"GROUP", "UNIT", VariableType::String, 0x4u, true},
    {"UNIT", "UNIT", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn1[] = {66, 67, 68, 69};
inline constexpr uint32_t kOut1[] = {112, 113};

// "Box Collision": `CBoxColliderDescriptor::CBoxColliderDescriptor`, then `CColliderDescriptor::CColliderDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CBoxColliderDescriptorParam1`.
inline constexpr Property kProps2[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TYPE", "COLLISION", VariableType::UInt, 0x4u, true},
    {"INTERSECTION TYPE", "COLLISION", VariableType::UInt, 0x4u, true},
    {"FRICTION", "COLLISION", VariableType::Float, 0x0u, false},
    {"BOUNCYNESS", "COLLISION", VariableType::Float, 0x0u, false},
    {"WIDTH", "DIMINSIONS", VariableType::Float, 0x0u, false},
    {"HEIGHT", "DIMINSIONS", VariableType::Float, 0x0u, false},
    {"DEPTH", "DIMINSIONS", VariableType::Float, 0x0u, false},
    {"SHOW BOX", "VISUAL BOX", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn2[] = {2, 3};
inline constexpr uint32_t kOut2[] = {0};

// "Button": `CEditorButtonDescriptor::CEditorButtonDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CEditorButtonDescriptorParam1`.
inline constexpr Property kProps3[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"PCT X", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"PCT Y", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"PCT WIDTH", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"PCT HEIGHT", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"X", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"Y", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"WIDTH", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"HEIGHT", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"X OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"Y OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"PCT X OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"PCT Y OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"NORMAL", "IMAGE FILES", VariableType::String, 0x0u, false},
    {"ROLLOVER", "IMAGE FILES", VariableType::String, 0x0u, false},
    {"CLICKED", "IMAGE FILES", VariableType::String, 0x0u, false},
    {"DISABLED", "IMAGE FILES", VariableType::String, 0x0u, false},
};
inline constexpr uint32_t kIn3[] = {0, 1, 2, 3};
inline constexpr uint32_t kOut3[] = {4, 5, 6, 7, 111};

// "Camera Controller": `CCameraControllerDescriptor::CCameraControllerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CCameraControllerDescriptorParam1`.
inline constexpr Property kProps4[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"PAN TIME", "CAMERA CONTROL", VariableType::Float, 0x0u, false},
    {"EASE IN DISTANCE PCT", "CAMERA CONTROL", VariableType::Float, 0x0u, false},
    {"EASE IN PCT", "CAMERA CONTROL", VariableType::Float, 0x0u, false},
    {"TIME", "PAUSE TIMER", VariableType::Float, 0x0u, false},
    {"RESETS AT END", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FOLLOW UNIT", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TYPE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"GROUP", "INTERACT WITH", VariableType::String, 0x4u, true},
    {"UNIT", "INTERACT WITH", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn4[] = {51, 52};
inline constexpr uint32_t kOut4[] = {87, 88, 89, 90};

// "Camera Shake": `CCameraShakeDescriptor::CCameraShakeDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CCameraShakeDescriptorParam1`.
inline constexpr Property kProps5[] = {
    {"DIRECTION", "PROPERTIES", VariableType::Vector3, 0x0u, false},
    {"DURATION", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"MAGNITUDE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"ORIENTATION TYPE", "PROPERTIES", VariableType::String, 0x4u, true},
    {"CAMERA SHAKE NAME", "PROPERTIES", VariableType::String, 0x4u, true},
    {"FADES", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn5[] = {15};
inline constexpr uint32_t kOut5[] = {0};

// "Cinematic": `CCinematicDescriptor::CCinematicDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CCinematicDescriptorParam1`.
inline constexpr Property kProps6[] = {
    {"CINEMATIC", "CINEMATIC", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn6[] = {9};
inline constexpr uint32_t kOut6[] = {92};

// "Color": `CColorAffectorDescriptor::CColorAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps7[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"COLOR OVER TIME", "PROPERTIES", VariableType::Float, 0x20u, false},
};
inline constexpr uint32_t kIn7[] = {2, 3};
inline constexpr uint32_t kOut7[] = {0};

// "Counter": `CCounterDescriptor::CCounterDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CCounterDescriptorParam1`.
inline constexpr Property kProps8[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"EQUALS VALUE", "PROPERTIES", VariableType::Int, 0x0u, false},
    {"STARTING VALUE", "PROPERTIES", VariableType::Int, 0x0u, false},
    {"LOGIC", "PROPERTIES", VariableType::UInt, 0x4u, true},
};
inline constexpr uint32_t kIn8[] = {7, 8, 2, 3, 6};
inline constexpr uint32_t kOut8[] = {6, 7, 9, 8};

// "Damage Shape": `CDamageShapeDescriptor::CDamageShapeDescriptor`, then `CShapeDescriptor::CShapeDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CDamageShapeDescriptorParam1`.
inline constexpr Property kProps9[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"SHAPE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"RELEASE ORDER", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"DIRECTION", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"SIZE", "BOX SIZE", VariableType::Vector3, 0x0u, false},
    {"USE MAX ONLY", "RADIUS", VariableType::Bool, 0x0u, false},
    {"MIN RADIUS", "RADIUS", VariableType::Float, 0x10u, false},
    {"MAX RADIUS", "RADIUS", VariableType::Float, 0x10u, false},
    {"ANGLE", "ANGLE", VariableType::Float, 0x10u, false},
    {"ANGLE OFFSET", "ANGLE", VariableType::Float, 0x10u, false},
    {"DAMAGE TYPE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"UNITTYPE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"ALIGNMENT", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"UPDATE IN EDITOR", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DOT", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TARGET CORPSE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MAX UNITS TO HIT", "PROPERTIES", VariableType::Int, 0x0u, false},
    {"MIN", "DAMAGE", VariableType::Float, 0x0u, false},
    {"MAX", "DAMAGE", VariableType::Float, 0x0u, false},
    {"LOOPS", "TIMER", VariableType::Bool, 0x0u, false},
    {"LOOP COUNT", "TIMER", VariableType::Int, 0x0u, false},
    {"TIMER", "TIMER", VariableType::Float, 0x0u, false},
    {"PAUSE TIMER", "TIMER", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn9[] = {2, 3};
inline constexpr uint32_t kOut9[] = {0};

// "Dungeon Object": `CDungeonObjectDescriptor::CDungeonObjectDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps10[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"DUNGEON NAME", "PROPERTIES", VariableType::String, 0x0u, false},
};
inline constexpr uint32_t kIn10[] = {0, 1, 81};
inline constexpr uint32_t kOut10[] = {4, 5};

// "Emitter": `CParticleEmitterWrapperDescriptor::CParticleEmitterWrapperDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CParticleEmitterWrapperDescriptorParam1`.
inline constexpr Property kProps11[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"TYPE OF EMITTER", "TYPE", VariableType::UInt, 0x4u, true},
    {"DIRECTION", "MOVEMENT", VariableType::Vector3, 0x0u, false},
    {"FIXED", "DIMENSIONS", VariableType::Bool, 0x0u, false},
    {"SCALE ON LAUNCH", "DIMENSIONS", VariableType::Float, 0x10u, false},
    {"WIDTH", "DIMENSIONS", VariableType::Float, 0x10u, false},
    {"HEIGHT", "DIMENSIONS", VariableType::Float, 0x10u, false},
    {"DEPTH", "DIMENSIONS", VariableType::Float, 0x10u, false},
    {"ANGLE", "MOVEMENT", VariableType::Float, 0x10u, false},
    {"VELOCITY", "MOVEMENT", VariableType::Float, 0x10u, false},
    {"MASS", "PROPERTIES", VariableType::Float, 0x10u, false},
    {"KEEP LOCAL", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"NO EXPIRATION", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DELAY FIRST", "EMISSION", VariableType::Bool, 0x0u, false},
    {"DELAY", "EMISSION", VariableType::Float, 0x10u, false},
    {"PARTICLE LIFE", "EMISSION", VariableType::Float, 0x10u, false},
    {"EMIT DURATION", "EMISSION", VariableType::Float, 0x10u, false},
    {"EMIT RATE", "EMISSION", VariableType::Float, 0x10u, false},
    {"NUM LOOPS", "EMISSION", VariableType::Int, 0x0u, false},
    {"RELEASE OVER DISTANCE", "EMISSION", VariableType::Bool, 0x0u, false},
    {"CHANCE TO EMIT", "EMISSION", VariableType::Float, 0x10u, false},
    {"MIN INCREMENT", "LINE EMITTER", VariableType::Float, 0x0u, false},
    {"MAX INCREMENT", "LINE EMITTER", VariableType::Float, 0x0u, false},
    {"MAX DEVIATION", "LINE EMITTER", VariableType::Float, 0x0u, false},
    {"END POSITION", "LINE EMITTER", VariableType::Vector3, 0x0u, false},
    {"WIDTH", "BOX EMITTER", VariableType::Float, 0x0u, false},
    {"HEIGHT", "BOX EMITTER", VariableType::Float, 0x0u, false},
    {"DEPTH", "BOX EMITTER", VariableType::Float, 0x0u, false},
    {"RADIUS", "SPHERE EMITTER", VariableType::Float, 0x0u, false},
    {"MIN RADIUS", "CIRCLE EMITTER", VariableType::Float, 0x10u, false},
    {"MAX RADIUS", "CIRCLE EMITTER", VariableType::Float, 0x10u, false},
    {"STEP", "CIRCLE EMITTER", VariableType::Float, 0x10u, false},
    {"FIXED", "CIRCLE EMITTER", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn11[] = {2, 3};
inline constexpr uint32_t kOut11[] = {0};

// "Game State Controller": `CGameStateControllerDescriptor::CGameStateControllerDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CGameStateControllerDescriptorParam1`.
inline constexpr Property kProps12[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"INVULNERABLE", "PLAYER", VariableType::Bool, 0x0u, false},
    {"STATE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"TIP", "HELP", VariableType::UInt, 0x4u, true},
};
inline constexpr uint32_t kIn12[] = {2, 3, 75, 76, 80};
inline constexpr uint32_t kOut12[] = {6, 7, 67, 68, 69, 70, 71, 72, 73, 74, 75};

// "Generic Model": `CGenericModelDescriptor::CGenericModelDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CGenericModelDescriptorParam1`.
inline constexpr Property kProps13[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"FILE", "RESOURCES", VariableType::String, 0x1000u, false},
    {"SCALE X", "MUTATORS", VariableType::Float, 0x0u, false},
    {"SCALE Y", "MUTATORS", VariableType::Float, 0x0u, false},
    {"SCALE Z", "MUTATORS", VariableType::Float, 0x0u, false},
    {"SPEED", "ANIMATION", VariableType::Float, 0x0u, false},
    {"LOOPS", "ANIMATION", VariableType::Bool, 0x0u, false},
    {"ANIMATION", "ANIMATION", VariableType::UInt, 0x4u, true},
    {"LIGHT MAP", "RENDERING", VariableType::Bool, 0x0u, false},
    {"TEXTURE OVERRIDE", "TEXTURE", VariableType::String, 0x2000u, false},
    {"POLY COUNT", "INFO", VariableType::Int, 0x1u, false},
};
inline constexpr uint32_t kIn13[] = {0, 1};
inline constexpr uint32_t kOut13[] = {4, 5};

// "Geometry Rotator": `CGeometryRotateDescriptor::CGeometryRotateDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CGeometryRotateDescriptorParam1`.
inline constexpr Property kProps14[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ROTATION SPEED", "ROTATION", VariableType::Float, 0x10u, false},
    {"USE OWN ROTATION", "ROTATION", VariableType::Bool, 0x0u, false},
    {"AXIS OF ROTATION", "ROTATION", VariableType::Vector3, 0x0u, false},
};
inline constexpr uint32_t kIn14[] = {2, 3};
inline constexpr uint32_t kOut14[] = {0};

// "Gravity Well": `CGravityAffectorDescriptor::CGravityAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps15[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GRAVITY", "PROPERTIES", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn15[] = {2, 3};
inline constexpr uint32_t kOut15[] = {0};

// "Group": `CRandomGroupDescriptor::CRandomGroupDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CRandomGroupDescriptorParam1`.
inline constexpr Property kProps16[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DYNAMIC", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"RANDOMIZATION", "WEIGHT OR RAND AS CHILD", VariableType::UInt, 0x0u, false},
    {"NUMBER", "CHILD RANDOMIZATION", VariableType::UInt, 0x0u, false},
    {"CHOICE", "CHILD RANDOMIZATION", VariableType::UInt, 0x4u, true},
    {"DIFFICULTY MODE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"DUNGEON", "PROPERTIES", VariableType::String, 0x4u, true},
    {"CLASS", "PROPERTIES", VariableType::String, 0x4u, true},
    {"ACTIVE OR COMPLETE", "QUEST", VariableType::String, 0x4u, true},
    {"COMPLETE", "QUEST", VariableType::String, 0x4u, true},
    {"NOT COMPLETE", "QUEST", VariableType::String, 0x4u, true},
    {"ACTIVE", "QUEST", VariableType::String, 0x4u, true},
    {"NOT ACTIVE OR COMPLETE", "QUEST", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn16[] = {0, 1};
inline constexpr uint32_t kOut16[] = {4, 5};

// "Image": `CEditorImageDescriptor::CEditorImageDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CEditorImageDescriptorParam1`.
inline constexpr Property kProps17[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"PCT X", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"PCT Y", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"PCT WIDTH", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"PCT HEIGHT", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"X", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"Y", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"WIDTH", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"HEIGHT", "SIZE AND SHAPE", VariableType::Float, 0x0u, false},
    {"X OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"Y OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"PCT X OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"PCT Y OFFSET", "OFFSETS", VariableType::Float, 0x0u, false},
    {"FILE", "IMAGE FILE", VariableType::String, 0x0u, false},
};
inline constexpr uint32_t kIn17[] = {0, 1};
inline constexpr uint32_t kOut17[] = {4, 5};

// "Item": `CItemDescriptor::CItemDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CItemDescriptorParam1`.
inline constexpr Property kProps18[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ITEM", "PROPERTIES", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn18[] = {0};
inline constexpr uint32_t kOut18[] = {27};

// "Jet": `CJetAffectorDescriptor::CJetAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps19[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ACCELERATION", "PROPERTIES", VariableType::Float, 0x10u, false},
};
inline constexpr uint32_t kIn19[] = {2, 3};
inline constexpr uint32_t kOut19[] = {0};

// "Layout Link": `CLayoutDescriptor::CLayoutDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLayoutDescriptorKLAYOUT_TYPE_NORMALParam2`.
inline constexpr Property kProps20[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"LAYOUT FILE", "RESOURCES", VariableType::String, 0x800u, false},
};
inline constexpr uint32_t kIn20[] = {0, 1, 2, 3, 4, 5};
inline constexpr uint32_t kOut20[] = {4, 5, 6, 7};

// "Layout Link Particle": `CLayoutDescriptor::CLayoutDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLayoutDescriptorKLAYOUT_TYPE_PARTICLEParam2`.
inline constexpr Property kProps21[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"LAYOUT FILE", "RESOURCES", VariableType::String, 0x200u, false},
    {"START ON LOAD", "", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn21[] = {0, 1, 2, 3, 4, 5, 16, 17, 18};
inline constexpr uint32_t kOut21[] = {4, 5, 6, 7, 31, 32};

// "Layout Link Timeline": `CLayoutDescriptor::CLayoutDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLayoutDescriptorKLAYOUT_TYPE_TIMELINEParam2`.
inline constexpr Property kProps22[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"LAYOUT FILE", "RESOURCES", VariableType::String, 0x800u, false},
    {"START ON LOAD", "", VariableType::Bool, 0x0u, false},
    {"DURATION MODIFY", "", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn22[] = {0, 1, 2, 3, 4, 5, 21, 22, 23, 25};
inline constexpr uint32_t kOut22[] = {4, 5, 6, 7, 36, 42, 41, 35, 44, 43, 40, 37, 38, 39};

// "Layout Link Trigger": `CLayoutDescriptor::CLayoutDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLayoutDescriptorKLAYOUT_TYPE_UNIT_TRIGGERParam2`.
inline constexpr Property kProps23[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn23[] = {0, 1, 2, 3, 4, 5, 77, 78, 6, 6, 46, 30, 31};
inline constexpr uint32_t kOut23[] = {4, 5, 6, 7, 9, 0, 46, 47, 48};

// "Light": `CLightDescriptor::CLightDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLightDescriptorParam1`.
inline constexpr Property kProps24[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FILE", "PROPERTIES", VariableType::String, 0x4u, true},
    {"BRIGHTNESS", "BRIGHTNESS", VariableType::UInt, 0x0u, false},
    {"SCALE X", "SCALE", VariableType::Float, 0x0u, false},
    {"SCALE Z", "SCALE", VariableType::Float, 0x0u, false},
    {"ANGLE", "ROTATION ANGLE", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn24[] = {0, 1};
inline constexpr uint32_t kOut24[] = {4, 5};

// "Line": `CLineAffectorDescriptor::CLineAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLineAffectorDescriptorParam1`.
inline constexpr Property kProps25[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DEVIATION", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"TIME STEP", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"DRIFT", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"END POSITION", "PROPERTIES", VariableType::Vector3, 0x0u, false},
};
inline constexpr uint32_t kIn25[] = {2, 3};
inline constexpr uint32_t kOut25[] = {0};

// "Linear Force": `CLinearForceAffectorDescriptor::CLinearForceAffectorDescriptor` (inline in the console's thunk), then `CForceAffectorDescriptor::CForceAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLinearForceAffectorDescriptorParam1`.
inline constexpr Property kProps26[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FORCE APPLICATION", "FORCES", VariableType::UInt, 0x4u, true},
    {"FORCES", "FORCES", VariableType::Vector3, 0x0u, false},
};
inline constexpr uint32_t kIn26[] = {2, 3};
inline constexpr uint32_t kOut26[] = {0};

// "Logic Group": `CLogicGroupDescriptor::CLogicGroupDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLogicGroupDescriptorParam1`.
inline constexpr Property kProps27[] = {
    {"", "", VariableType::Unknown, 0u, false},
};
inline constexpr uint32_t kIn27[] = {84, 85, 86, 87, 88, 89, 90, 91};
inline constexpr uint32_t kOut27[] = {118, 119, 120, 121, 122, 123, 124, 125, 115};

// "Missile": `CMissileDescriptor::CMissileDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CMissileDescriptorParam1`.
inline constexpr Property kProps28[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"RELEASE", "PARTICLES", VariableType::String, 0x200u, false},
    {"ACTIVE", "PARTICLES", VariableType::String, 0x200u, false},
    {"HIT", "PARTICLES", VariableType::String, 0x200u, false},
    {"DIE", "PARTICLES", VariableType::String, 0x200u, false},
    {"NUM RICOCHETS", "PROPERTIES", VariableType::UInt, 0x0u, false},
    {"MAX DISTANCE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"RADIUS", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"AOE RAIDUS", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"MAX VELOCITY", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"FRICTION", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"FORCE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"FULL VELOCITY", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MISSILE NAME", "PROPERTIES", VariableType::String, 0x0u, false},
    {"HOMING SPEED", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"TARGETING ANGLE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"REPEAT RATE", "EDITOR_ONLY", VariableType::Float, 0x100u, false},
    {"COLLISION RADIUS", "EDITOR_ONLY", VariableType::Bool, 0x100u, false},
    {"AOE RADIUS", "EDITOR_ONLY", VariableType::Bool, 0x100u, false},
    {"TRACK GROUND", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TARGET POSITION", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"COLLIDES WITH OBJECTS", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ARCH HEIGHT", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"HOME AFTER X SECONDS", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"PIERCING", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MAX TURN RATE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"TURN ACCELERATION", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"TURN RANDOMIZATION", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"RANDOMIZATION RATE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"AOE DAMAGE SCALE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"DAMAGE SOAK SCALE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"VERTICAL AIMING", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MIN PERCENT DAMAGE", "DAMAGE", VariableType::Float, 0x0u, false},
    {"MAX PERCENT DAMAGE", "DAMAGE", VariableType::Float, 0x0u, false},
    {"FORCE", "DAMAGE", VariableType::Float, 0x0u, false},
    {"DAMAGE TYPE", "DAMAGE", VariableType::UInt, 0x4u, true},
};
inline constexpr uint32_t kIn28[] = {0};
inline constexpr uint32_t kOut28[] = {0};

// "Money Taker": `CMoneyTakerDescriptor::CMoneyTakerDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps29[] = {
    {"AMOUNT", "MONEY TAKER", VariableType::Int, 0x0u, false},
};
inline constexpr uint32_t kIn29[] = {79};
inline constexpr uint32_t kOut29[] = {117, 116};

// "Monster": `CMonsterDescriptor::CMonsterDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CMonsterDescriptorParam1`.
inline constexpr Property kProps30[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"MONSTER", "PROPERTIES", VariableType::String, 0x4u, true},
    {"FILE", "MESH", VariableType::String, 0x1u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"NO TARGET", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FIRE ALL HP EVENTS", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"INVINCIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"PATH", "PROPERTIES", VariableType::String, 0x4u, true},
    {"PATH LOOPS", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TARGETABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"SCALE", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"CANNOT WANDER", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"NO GOLD", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"NO TREASURE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ANIMATION", "ANIMATION", VariableType::UInt, 0x4u, true},
    {"THEME", "PROPERTIES", VariableType::UInt, 0x104u, true},
    {"SPEED", "ANIMATION", VariableType::Float, 0x0u, false},
    {"LOOPS", "ANIMATION", VariableType::Bool, 0x0u, false},
    {"CHEST MESH", "WARDROBE", VariableType::String, 0x1100u, false},
    {"BOOTS MESH", "WARDROBE", VariableType::String, 0x1100u, false},
    {"GLOVES MESH", "WARDROBE", VariableType::String, 0x1100u, false},
    {"HELM MESH", "WARDROBE", VariableType::String, 0x1100u, false},
    {"SHOULDER MESH", "WARDROBE", VariableType::String, 0x1100u, false},
    {"CHEST TEXTURE", "WARDROBE", VariableType::String, 0x2100u, false},
    {"BOOTS TEXTURE", "WARDROBE", VariableType::String, 0x2100u, false},
    {"GLOVES TEXTURE", "WARDROBE", VariableType::String, 0x2100u, false},
};
inline constexpr uint32_t kIn30[] = {48, 49, 50, 61, 62, 63, 64, 2, 3, 65, 0, 1, 70, 71, 72, 73, 74, 82, 83};
inline constexpr uint32_t kOut30[] = {91, 96, 97, 57, 58, 59, 60, 61, 62, 63, 64, 65, 24, 66, 92, 93, 94, 95, 6, 7, 110, 5, 4};

// "Music": `CMusicObjectDescriptor::CMusicObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CMusicObjectDescriptorParam1`.
inline constexpr Property kProps31[] = {
    {"FILE NAME", "MUSIC", VariableType::String, 0x2u, false},
    {"LOOPS", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn31[] = {9, 10, 13};
inline constexpr uint32_t kOut31[] = {11, 12, 13};

// "Output Incrementor": `COutputIncrementorDescriptor::COutputIncrementorDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_COutputIncrementorDescriptorParam1`.
inline constexpr Property kProps32[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MAX OUTPUT", "INCREMENTOR", VariableType::Int, 0x0u, false},
    {"LOOP COUNT", "INCREMENTOR", VariableType::Int, 0x0u, false},
    {"LOOPS FOREVER", "INCREMENTOR", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn32[] = {14, 2, 3};
inline constexpr uint32_t kOut32[] = {16, 17, 18, 19, 20, 21, 22, 6, 7};

// "Particle": `CParticleTechWrapperDescriptor::CParticleTechWrapperDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CParticleTechWrapperDescriptorParam1`.
inline constexpr Property kProps33[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RENDER TYPE", "RENDERING", VariableType::UInt, 0x4u, true},
    {"VISUAL SCALE", "WORLD SCALES", VariableType::Float, 0x0u, false},
    {"TIME SCALE", "WORLD SCALES", VariableType::Float, 0x0u, false},
    {"VELOCITY SCALE", "WORLD SCALES", VariableType::Float, 0x0u, false},
    {"LIFE TIME", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"WORLD ORIENTATION", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"NO CULLING", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"IS LIGHT", "TECHNIQUE PROPERTIES", VariableType::Bool, 0x0u, false},
    {"VISIBLE", "TECHNIQUE PROPERTIES", VariableType::Bool, 0x0u, false},
    {"SORTS", "TECHNIQUE PROPERTIES", VariableType::Bool, 0x0u, false},
    {"SORT INDEX", "TECHNIQUE PROPERTIES", VariableType::UInt, 0x4u, true},
    {"DEPTH BIAS", "TECHNIQUE PROPERTIES", VariableType::Float, 0x0u, false},
    {"VELOCITY", "MAX VALUES ALLOWED", VariableType::Float, 0x0u, false},
    {"VISUAL QUOTA", "MAX VALUES ALLOWED", VariableType::UInt, 0x0u, false},
    {"TEXTURE", "RESOURCES and RENDERING", VariableType::String, 0x400u, false},
    {"RENDER STYLE", "RESOURCES and RENDERING", VariableType::UInt, 0x4u, true},
    {"MESH", "RESOURCES and RENDERING", VariableType::String, 0x1000u, false},
    {"DEPTH CHECK", "RESOURCES and RENDERING", VariableType::Bool, 0x0u, false},
    {"ROTATION TYPE", "BILLBOARD PROPERTIES", VariableType::UInt, 0x4u, true},
    {"ORIGIN", "BILLBOARD PROPERTIES", VariableType::UInt, 0x4u, true},
    {"LENGTH", "RIBBON PROPERTIES", VariableType::Float, 0x0u, false},
    {"WIDTH", "RIBBON PROPERTIES", VariableType::Float, 0x0u, false},
    {"SEGMENTS", "RIBBON PROPERTIES", VariableType::UInt, 0x0u, false},
    {"FADE ENDS", "RIBBON PROPERTIES", VariableType::Bool, 0x0u, false},
    {"LOCAL TRAIL", "RIBBON PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FRAMES WIDE", "UV ANIMATION", VariableType::Int, 0x0u, false},
    {"FRAMES HIGH", "UV ANIMATION", VariableType::Int, 0x0u, false},
    {"U RATE", "UV ANIMATION", VariableType::Float, 0x0u, false},
    {"V RATE", "UV ANIMATION", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn33[] = {16, 17};
inline constexpr uint32_t kOut33[] = {0};

// "Path Node": `CPathNodeDescriptor::CPathNodeDescriptor` (inline in the console's thunk), then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CPathNodeDescriptorParam1`.
inline constexpr Property kProps34[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
};
inline constexpr uint32_t kIn34[] = {0, 1};
inline constexpr uint32_t kOut34[] = {4, 5};

// "Pathing": `CPathControllerDescriptor::CPathControllerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CPathControllerDescriptorParam1`.
inline constexpr Property kProps35[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"POINTS", "PATHING POINTS", VariableType::Float, 0x8000u, false},
    {"NUMBER OF POINTS", "PATHING POINTS", VariableType::UInt, 0x0u, false},
    {"SHOW", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ENABLE ON START", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"PATH NAME", "PROPERTIES", VariableType::String, 0x0u, false},
    {"RUN", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"PATH TO PLAYER", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GROUP", "UNIT PATHING", VariableType::String, 0x4u, true},
    {"UNIT", "UNIT PATHING", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn35[] = {2, 3};
inline constexpr uint32_t kOut35[] = {6, 7, 110};

// "Plane Collision": `CPlaneColliderDescriptor::CPlaneColliderDescriptor`, then `CColliderDescriptor::CColliderDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CPlaneColliderDescriptorParam1`.
inline constexpr Property kProps36[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TYPE", "COLLISION", VariableType::UInt, 0x4u, true},
    {"INTERSECTION TYPE", "COLLISION", VariableType::UInt, 0x4u, true},
    {"FRICTION", "COLLISION", VariableType::Float, 0x0u, false},
    {"BOUNCYNESS", "COLLISION", VariableType::Float, 0x0u, false},
    {"NORMAL", "NORMAL", VariableType::Vector3, 0x0u, false},
    {"SHOW PLANE", "VISUAL PLANE", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn36[] = {2, 3};
inline constexpr uint32_t kOut36[] = {0};

// "Player Box Trigger": `CTriggerBoxDescriptor::CTriggerBoxDescriptor`, then `CTriggerDescriptor::CTriggerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps37[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DIMENSIONS", "PROPERTIES", VariableType::Vector3, 0x0u, false},
};
inline constexpr uint32_t kIn37[] = {2, 3, 6};
inline constexpr uint32_t kOut37[] = {6, 7, 9, 0, 1, 2, 3};

// "Player Sphere Trigger": `CTriggerSphereDescriptor::CTriggerSphereDescriptor`, then `CTriggerDescriptor::CTriggerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps38[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"RADIUS", "PROPERTIES", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn38[] = {2, 3, 6};
inline constexpr uint32_t kOut38[] = {6, 7, 9, 0, 1, 2, 3};

// "Property Node": `CPropertyNodeDescriptor::CPropertyNodeDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CPropertyNodeDescriptorParam1`.
inline constexpr Property kProps39[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"TYPE", "PROPERTY", VariableType::UInt, 0x44u, true},
    {"RADIUS", "PROPERTY", VariableType::Float, 0x0u, false},
    {"BOX WIDTH", "PROPERTY", VariableType::Float, 0x0u, false},
    {"BOX LENGTH", "PROPERTY", VariableType::Float, 0x0u, false},
    {"ENABLED", "PROPERTY", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn39[] = {0, 1, 2, 3};
inline constexpr uint32_t kOut39[] = {4, 5, 6, 7};

// "Puzzle Input": `CPuzzleRandomizerDescriptor::CPuzzleRandomizerDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CPuzzleRandomizerDescriptorParam1`.
inline constexpr Property kProps40[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"NUMBER OF INPUTS", "PROPERTIES", VariableType::Int, 0x0u, false},
};
inline constexpr uint32_t kIn40[] = {2, 3, 6, 40, 41, 42, 43, 44, 45};
inline constexpr uint32_t kOut40[] = {9, 55, 54};

// "Quest Controller": `CQuestControllerDescriptor::CQuestControllerDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CQuestControllerDescriptorParam1`.
inline constexpr Property kProps41[] = {
    {"DISABLE PLAYER", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"BROADCAST ON LOAD", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GROUP", "INTERACT WITH", VariableType::String, 0x4u, true},
    {"UNIT", "INTERACT WITH", VariableType::String, 0x4u, true},
    {"QUEST", "QUEST", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn41[] = {48, 53, 54, 55, 56};
inline constexpr uint32_t kOut41[] = {91, 92, 93, 94, 98, 99, 100, 101, 102};

// "Random Choice": `CRandomChoiceDescriptor::CRandomChoiceDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CRandomChoiceDescriptorParam1`.
inline constexpr Property kProps42[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TYPE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"COUNT", "WEIGHTED", VariableType::UInt, 0x0u, false},
    {"ONE", "CHOICES", VariableType::Int, 0x0u, false},
    {"TWO", "CHOICES", VariableType::Int, 0x0u, false},
    {"THREE", "CHOICES", VariableType::Int, 0x0u, false},
    {"FOUR", "CHOICES", VariableType::Int, 0x0u, false},
    {"FIVE", "CHOICES", VariableType::Int, 0x0u, false},
};
inline constexpr uint32_t kIn42[] = {39};
inline constexpr uint32_t kOut42[] = {49, 50, 51, 52, 53};

// "Room Piece": `CRoomPieceDescriptor::CRoomPieceDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CRoomPieceDescriptorParam1`.
inline constexpr Property kProps43[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"COLLISION ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"BAKE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"VISIBLE ON MAP", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GUID", "GUID", VariableType::String, 0x1u, false},
    {"PIECE", "ROOM PIECE", VariableType::String, 0x101u, false},
    {"VISUAL", "VISUAL PIECE INDEX", VariableType::UInt, 0x4004u, true},
    {"SCALE", "SCALE", VariableType::Float, 0x0u, false},
    {"X", "SCALE", VariableType::Float, 0x0u, false},
    {"Y", "SCALE", VariableType::Float, 0x0u, false},
    {"Z", "SCALE", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn43[] = {0, 1};
inline constexpr uint32_t kOut43[] = {4, 5};

// "Scale": `CScaleAffectorDescriptor::CScaleAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CScaleAffectorDescriptorParam1`.
inline constexpr Property kProps44[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FIXED", "SCALES", VariableType::Bool, 0x0u, false},
    {"X", "SCALES", VariableType::Float, 0x10u, false},
    {"Y", "SCALES", VariableType::Float, 0x10u, false},
    {"Z", "SCALES", VariableType::Float, 0x10u, false},
};
inline constexpr uint32_t kIn44[] = {2, 3};
inline constexpr uint32_t kOut44[] = {0};

// "Sine Force": `CSineForceAffectorDescriptor::CSineForceAffectorDescriptor`, then `CForceAffectorDescriptor::CForceAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CSineForceAffectorDescriptorParam1`.
inline constexpr Property kProps45[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FORCE APPLICATION", "FORCES", VariableType::UInt, 0x4u, true},
    {"FORCES", "FORCES", VariableType::Vector3, 0x0u, false},
    {"MIN", "SINE WAVE FREQUENCY", VariableType::Float, 0x0u, false},
    {"MAX", "SINE WAVE FREQUENCY", VariableType::Float, 0x0u, false},
};
inline constexpr uint32_t kIn45[] = {2, 3};
inline constexpr uint32_t kOut45[] = {0};

// "Skill Controller": `CSkillControllerDescriptor::CSkillControllerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CSkillControllerDescriptorParam1`.
inline constexpr Property kProps46[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"LEARN ON START", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"UNLEARN ON STOP", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"FORCE STOP", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TARGET PLAYER", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"USE UNIT TARGET", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"SKILL LEVEL", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GROUP", "INTERACT WITH", VariableType::String, 0x4u, true},
    {"UNIT", "INTERACT WITH", VariableType::String, 0x4u, true},
    {"SKILL", "SKILL", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn46[] = {57, 58, 59, 60};
inline constexpr uint32_t kOut46[] = {103, 104, 105, 106};

// "Skip Cutscene": `CSkipCutsceneDescriptor::CSkipCutsceneDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CSkipCutsceneDescriptorParam1`.
inline constexpr Property kProps47[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DISABLE SKILLS", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn47[] = {2, 3};
inline constexpr uint32_t kOut47[] = {6, 7, 114};

// "Sound": `CSoundObjectDescriptor::CSoundObjectDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CSoundObjectDescriptorParam1`.
inline constexpr Property kProps48[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"START ON ACTIVATE", "SOUND", VariableType::Bool, 0x0u, false},
    {"ENVIRONMENTAL", "SOUND", VariableType::Bool, 0x0u, false},
    {"VOLUME", "SOUND", VariableType::Float, 0x0u, false},
    {"RADIUS", "SOUND", VariableType::Float, 0x0u, false},
    {"CATEGORY", "FILE", VariableType::UInt, 0x104u, true},
    {"SOUND GROUP", "FILE", VariableType::UInt, 0x104u, true},
    {"GUID", "GUID", VariableType::String, 0x80u, false},
};
inline constexpr uint32_t kIn48[] = {9, 10, 11, 12};
inline constexpr uint32_t kOut48[] = {11, 12, 13, 14, 15};

// "Sphere Collision": `CSphereColliderDescriptor::CSphereColliderDescriptor`, then `CColliderDescriptor::CColliderDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CSphereColliderDescriptorParam1`.
inline constexpr Property kProps49[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TYPE", "COLLISION", VariableType::UInt, 0x4u, true},
    {"INTERSECTION TYPE", "COLLISION", VariableType::UInt, 0x4u, true},
    {"FRICTION", "COLLISION", VariableType::Float, 0x0u, false},
    {"BOUNCYNESS", "COLLISION", VariableType::Float, 0x0u, false},
    {"RADIUS", "SIZE", VariableType::Float, 0x0u, false},
    {"SHOW SPHERE", "VISUAL SPHERE", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn49[] = {2, 3};
inline constexpr uint32_t kOut49[] = {0};

// "Teleport": `CTeleportDescriptor::CTeleportDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CTeleportDescriptorParam1`.
inline constexpr Property kProps50[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn50[] = {38, 2, 3};
inline constexpr uint32_t kOut50[] = {8};

// "Texture Animation": `CTextureAnimateDescriptor::CTextureAnimateDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CTextureAnimateDescriptorParam1`.
inline constexpr Property kProps51[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ANIMATION SPEED", "ANIMATION", VariableType::Float, 0x10u, false},
    {"USE OWN ANIMATION", "ANIMATION", VariableType::Bool, 0x0u, false},
    {"USE RANDOM STARTING FRAME", "ANIMATION", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn51[] = {2, 3};
inline constexpr uint32_t kOut51[] = {0};

// "Texture Rotate": `CTextureRotateDescriptor::CTextureRotateDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CTextureRotateDescriptorParam1`.
inline constexpr Property kProps52[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"ROTATION SPEED", "ROTATION", VariableType::Float, 0x10u, false},
    {"STARTING ROTATION", "ROTATION", VariableType::Float, 0x10u, false},
    {"USE OWN ROTATION", "ROTATION", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn52[] = {2, 3};
inline constexpr uint32_t kOut52[] = {0};

// "Timeline": `CTimelineDescriptor::CTimelineDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CTimelineDescriptorParam1`.
inline constexpr Property kProps53[] = {
    {"DURATION", "TIMELINE", VariableType::Float, 0x0u, false},
    {"STEP", "TIMELINE", VariableType::Float, 0x0u, false},
    {"LOOPS", "TIMELINE", VariableType::Bool, 0x0u, false},
    {"RESET PROPERTIES", "LOAD", VariableType::Bool, 0x0u, false},
    {"START ON LOAD", "LOAD", VariableType::Bool, 0x0u, false},
    {"DEFAULT INTERPOLATION TYPE", "TIMELINE", VariableType::UInt, 0x4u, true},
    {"LAYOUT CONTROL", "MODIFICATION", VariableType::Bool, 0x0u, false},
    {"TYPE", "MODIFICATION", VariableType::UInt, 0x4u, true},
};
inline constexpr uint32_t kIn53[] = {2, 3, 21, 22, 23, 24, 25, 26, 27, 28, 29};
inline constexpr uint32_t kOut53[] = {6, 7, 36, 42, 41, 35, 44, 43, 40, 37, 38, 39, 45};

// "Timer": `CLogicTimerDescriptor::CLogicTimerDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CLogicTimerDescriptorParam1`.
inline constexpr Property kProps54[] = {
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TIME", "TIMER", VariableType::Float, 0x0u, false},
    {"MAXTIME", "TIMER", VariableType::Float, 0x0u, false},
    {"LOOP COUNT", "TIMER", VariableType::Int, 0x0u, false},
    {"LOOPS FOREVER", "TIMER", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn54[] = {2, 3, 6};
inline constexpr uint32_t kOut54[] = {6, 7, 8};

// "Unit Spawner": `CUnitSpawnerDescriptor::CUnitSpawnerDescriptor`, then `CShapeDescriptor::CShapeDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CUnitSpawnerDescriptorParam1`.
inline constexpr Property kProps55[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"SHAPE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"RELEASE ORDER", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"DIRECTION", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"SIZE", "BOX SIZE", VariableType::Vector3, 0x0u, false},
    {"USE MAX ONLY", "RADIUS", VariableType::Bool, 0x0u, false},
    {"MIN RADIUS", "RADIUS", VariableType::Float, 0x10u, false},
    {"MAX RADIUS", "RADIUS", VariableType::Float, 0x10u, false},
    {"ANGLE", "ANGLE", VariableType::Float, 0x10u, false},
    {"ANGLE OFFSET", "ANGLE", VariableType::Float, 0x10u, false},
    {"COUNT", "PROPERTIES", VariableType::UInt, 0x0u, false},
    {"MAX ALLOWED", "PROPERTIES", VariableType::UInt, 0x0u, false},
    {"UPDATE IN EDITOR", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"SPAWN ON CREATE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"TARGET PLAYER", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DURATION", "PROPERTIES", VariableType::Float, 0x0u, false},
    {"PULSE RATE", "PROPERTIES", VariableType::UInt, 0x0u, false},
    {"SPAWN IN CENTER", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"REQUIRES LOS", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MUST SPAWN", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"PLAY SPAWN ANIM", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"SNAP TO GROUND", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GIVE XP", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GIVE LOOT", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"UNIT LEVEL DELTA", "PROPERTIES", VariableType::Int, 0x0u, false},
    {"DESTROY BODY", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"GROUP", "RESOURCES", VariableType::String, 0x4u, true},
    {"RESOURCE", "RESOURCES", VariableType::String, 0x4u, true},
};
inline constexpr uint32_t kIn55[] = {32, 10, 33, 34, 35, 36};
inline constexpr uint32_t kOut55[] = {26, 24, 25, 27, 28, 29, 30};

// "Unit Trigger": `CUnitTriggerDescriptor::CUnitTriggerDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CUnitTriggerDescriptorParam1`.
inline constexpr Property kProps56[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"MODEL", "PROPERTIES", VariableType::String, 0x4u, true},
    {"LOOP STYLE", "PROPERTIES", VariableType::UInt, 0x4u, true},
    {"REQUIRED QUEST", "REQUIREMENTS", VariableType::String, 0x4u, true},
    {"REQUIRED ITEM", "REQUIREMENTS", VariableType::String, 0x4u, true},
    {"NUMBER REQUIRED", "REQUIREMENTS", VariableType::UInt, 0x0u, false},
    {"TAKE ITEMS", "REQUIREMENTS", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn56[] = {2, 3, 77, 78, 6, 6, 46, 30, 31};
inline constexpr uint32_t kOut56[] = {6, 7, 9, 0, 46, 47, 48};

// "Vortext": `CVortexAffectorDescriptor::CVortexAffectorDescriptor`, then `CAffectorDescriptor::CAffectorDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CVortexAffectorDescriptorParam1`.
inline constexpr Property kProps57[] = {
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"ENABLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"DIRECTION", "ROTATION", VariableType::Vector3, 0x0u, false},
    {"RATE", "ROTATION", VariableType::Float, 0x10u, false},
};
inline constexpr uint32_t kIn57[] = {2, 3};
inline constexpr uint32_t kOut57[] = {0};

// "Warper": `CWarperDescriptor::CWarperDescriptor`, then `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`; registered by `Set_CWarperDescriptorParam1`.
inline constexpr Property kProps58[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"ENABLED", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"LEVEL DELTA", "PROPERTIES", VariableType::Int, 0x0u, false},
    {"LEVEL ABSOLUTE", "PROPERTIES", VariableType::Int, 0x0u, false},
    {"DUNGEON NAME", "PROPERTIES", VariableType::String, 0x0u, false},
    {"WARP NAME", "PROPERTIES", VariableType::String, 0x0u, false},
    {"WAYPOINT", "PROPERTIES", VariableType::Bool, 0x0u, false},
};
inline constexpr uint32_t kIn58[] = {0, 1, 37, 2, 3};
inline constexpr uint32_t kOut58[] = {4, 5};

// "Waypoint Activator": `CPositionableObjectDescriptor::CPositionableObjectDescriptor`, then `CBaseObjectDescriptor::CBaseObjectDescriptor`, then `CDescriptor::CDescriptor`.
inline constexpr Property kProps59[] = {
    {"VISIBLE", "PROPERTIES", VariableType::Bool, 0x0u, false},
    {"POSITION", "POSITION", VariableType::Vector3, 0x0u, false},
    {"FORWARD", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"RIGHT", "ORIENTATION", VariableType::Vector3, 0x0u, false},
    {"UP", "ORIENTATION", VariableType::Vector3, 0x0u, false},
};
inline constexpr uint32_t kIn59[] = {0, 1, 15};
inline constexpr uint32_t kOut59[] = {4, 5};

inline constexpr Descriptor kDescriptors[] = {
    {"Align", "CAlignDescriptor", kProps0, 4u, kIn0, 2u, kOut0, 0u},
    {"Animation Controller", "CAnimationPlayerDescriptor", kProps1, 8u, kIn1, 4u, kOut1, 2u},
    {"Box Collision", "CBoxColliderDescriptor", kProps2, 10u, kIn2, 2u, kOut2, 0u},
    {"Button", "CEditorButtonDescriptor", kProps3, 18u, kIn3, 4u, kOut3, 5u},
    {"Camera Controller", "CCameraControllerDescriptor", kProps4, 10u, kIn4, 2u, kOut4, 4u},
    {"Camera Shake", "CCameraShakeDescriptor", kProps5, 6u, kIn5, 1u, kOut5, 0u},
    {"Cinematic", "CCinematicDescriptor", kProps6, 1u, kIn6, 1u, kOut6, 1u},
    {"Color", "CColorAffectorDescriptor", kProps7, 3u, kIn7, 2u, kOut7, 0u},
    {"Counter", "CCounterDescriptor", kProps8, 4u, kIn8, 5u, kOut8, 4u},
    {"Damage Shape", "CDamageShapeDescriptor", kProps9, 24u, kIn9, 2u, kOut9, 0u},
    {"Dungeon Object", "CDungeonObjectDescriptor", kProps10, 6u, kIn10, 3u, kOut10, 2u},
    {"Emitter", "CParticleEmitterWrapperDescriptor", kProps11, 34u, kIn11, 2u, kOut11, 0u},
    {"Game State Controller", "CGameStateControllerDescriptor", kProps12, 4u, kIn12, 5u, kOut12, 11u},
    {"Generic Model", "CGenericModelDescriptor", kProps13, 15u, kIn13, 2u, kOut13, 2u},
    {"Geometry Rotator", "CGeometryRotateDescriptor", kProps14, 5u, kIn14, 2u, kOut14, 0u},
    {"Gravity Well", "CGravityAffectorDescriptor", kProps15, 3u, kIn15, 2u, kOut15, 0u},
    {"Group", "CRandomGroupDescriptor", kProps16, 14u, kIn16, 2u, kOut16, 2u},
    {"Image", "CEditorImageDescriptor", kProps17, 14u, kIn17, 2u, kOut17, 2u},
    {"Item", "CItemDescriptor", kProps18, 6u, kIn18, 0u, kOut18, 1u},
    {"Jet", "CJetAffectorDescriptor", kProps19, 3u, kIn19, 2u, kOut19, 0u},
    {"Layout Link", "CLayoutDescriptor", kProps20, 7u, kIn20, 6u, kOut20, 4u},
    {"Layout Link Particle", "CLayoutDescriptor", kProps21, 8u, kIn21, 9u, kOut21, 6u},
    {"Layout Link Timeline", "CLayoutDescriptor", kProps22, 9u, kIn22, 10u, kOut22, 14u},
    {"Layout Link Trigger", "CLayoutDescriptor", kProps23, 6u, kIn23, 13u, kOut23, 9u},
    {"Light", "CLightDescriptor", kProps24, 7u, kIn24, 2u, kOut24, 2u},
    {"Line", "CLineAffectorDescriptor", kProps25, 6u, kIn25, 2u, kOut25, 0u},
    {"Linear Force", "CForceAffectorDescriptor", kProps26, 4u, kIn26, 2u, kOut26, 0u},
    {"Logic Group", "CLogicGroupDescriptor", kProps27, 0u, kIn27, 8u, kOut27, 9u},
    {"Missile", "CMissileDescriptor", kProps28, 36u, kIn28, 0u, kOut28, 0u},
    {"Money Taker", "CMoneyTakerDescriptor", kProps29, 1u, kIn29, 1u, kOut29, 2u},
    {"Monster", "CMonsterDescriptor", kProps30, 30u, kIn30, 19u, kOut30, 23u},
    {"Music", "CMusicObjectDescriptor", kProps31, 2u, kIn31, 3u, kOut31, 3u},
    {"Output Incrementor", "COutputIncrementorDescriptor", kProps32, 4u, kIn32, 3u, kOut32, 9u},
    {"Particle", "CParticleTechWrapperDescriptor", kProps33, 33u, kIn33, 2u, kOut33, 0u},
    {"Path Node", "CPositionableObjectDescriptor", kProps34, 2u, kIn34, 2u, kOut34, 2u},
    {"Pathing", "CPathControllerDescriptor", kProps35, 11u, kIn35, 2u, kOut35, 3u},
    {"Plane Collision", "CPlaneColliderDescriptor", kProps36, 8u, kIn36, 2u, kOut36, 0u},
    {"Player Box Trigger", "CTriggerBoxDescriptor", kProps37, 3u, kIn37, 3u, kOut37, 7u},
    {"Player Sphere Trigger", "CTriggerSphereDescriptor", kProps38, 3u, kIn38, 3u, kOut38, 7u},
    {"Property Node", "CPropertyNodeDescriptor", kProps39, 10u, kIn39, 4u, kOut39, 4u},
    {"Puzzle Input", "CPuzzleRandomizerDescriptor", kProps40, 2u, kIn40, 9u, kOut40, 3u},
    {"Quest Controller", "CQuestControllerDescriptor", kProps41, 5u, kIn41, 5u, kOut41, 9u},
    {"Random Choice", "CRandomChoiceDescriptor", kProps42, 8u, kIn42, 1u, kOut42, 5u},
    {"Room Piece", "CRoomPieceDescriptor", kProps43, 15u, kIn43, 2u, kOut43, 2u},
    {"Scale", "CScaleAffectorDescriptor", kProps44, 6u, kIn44, 2u, kOut44, 0u},
    {"Sine Force", "CSineForceAffectorDescriptor", kProps45, 6u, kIn45, 2u, kOut45, 0u},
    {"Skill Controller", "CSkillControllerDescriptor", kProps46, 10u, kIn46, 4u, kOut46, 4u},
    {"Skip Cutscene", "CSkipCutsceneDescriptor", kProps47, 2u, kIn47, 2u, kOut47, 3u},
    {"Sound", "CSoundObjectDescriptor", kProps48, 8u, kIn48, 4u, kOut48, 5u},
    {"Sphere Collision", "CSphereColliderDescriptor", kProps49, 8u, kIn49, 2u, kOut49, 0u},
    {"Teleport", "CTeleportDescriptor", kProps50, 5u, kIn50, 3u, kOut50, 1u},
    {"Texture Animation", "CTextureAnimateDescriptor", kProps51, 5u, kIn51, 2u, kOut51, 0u},
    {"Texture Rotate", "CTextureRotateDescriptor", kProps52, 5u, kIn52, 2u, kOut52, 0u},
    {"Timeline", "CTimelineDescriptor", kProps53, 8u, kIn53, 11u, kOut53, 13u},
    {"Timer", "CLogicTimerDescriptor", kProps54, 5u, kIn54, 3u, kOut54, 3u},
    {"Unit Spawner", "CUnitSpawnerDescriptor", kProps55, 31u, kIn55, 6u, kOut55, 7u},
    {"Unit Trigger", "CUnitTriggerDescriptor", kProps56, 11u, kIn56, 9u, kOut56, 7u},
    {"Vortext", "CVortexAffectorDescriptor", kProps57, 4u, kIn57, 2u, kOut57, 0u},
    {"Warper", "CWarperDescriptor", kProps58, 11u, kIn58, 5u, kOut58, 2u},
    {"Waypoint Activator", "CPositionableObjectDescriptor", kProps59, 5u, kIn59, 3u, kOut59, 2u},
};
// clang-format on
