// The interface text's scale: 1, unless iOS's Text Size setting changes it.
#pragma once

namespace tl::render {

float ui_text_scale();
void set_ui_text_scale(float scale);
/// `GameSettings::text_size` (0 small, 1 medium, 2 large) as a text scale.
float ui_text_size_scale(int text_size);

}  // namespace tl::render
