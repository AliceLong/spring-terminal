#pragma once
#include <lvgl.h>
#include "game/spring_collection.h"
namespace spring_collection {
constexpr unsigned kPixelBytes = 8 * 32 * 32 * 3;
const lv_image_dsc_t* Image(Kind kind, unsigned frame);
}  // namespace spring_collection
