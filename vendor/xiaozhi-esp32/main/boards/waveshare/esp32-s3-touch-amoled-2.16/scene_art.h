#pragma once
#include <lvgl.h>
#include "scene_art_pack.h"

namespace desk_gap {
bool InitializeSceneArt();
const lv_image_dsc_t* SceneBackground();
const lv_image_dsc_t* SceneImage(SceneArtKind kind, unsigned frame, bool mirror = false);
}  // namespace desk_gap
