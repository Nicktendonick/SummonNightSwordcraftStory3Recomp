#pragma once
#include "recomp_graphics_preset.h"
namespace swordcraft3 {
inline constexpr RecompGraphicsPreset graphics_presets[] = {
    {"Original Pixels", "Hard-edged pixels with no screen texture. The simplest look and a light option for slower PCs.", 0, 0, 35},
    {"Clean & Crisp", "Recommended starting point. Keeps pixel edges clearer when the window is resized, without adding a grid or scanlines.", 2, 0, 35},
    {"Soft & Smooth", "Gently rounds pixel edges with Smooth 2x. Game text can look softer; this asks more of your PC.", 3, 0, 35},
    {"Handheld Grid", "Original pixels with a light LCD grid at 25%. A handheld-inspired texture, not an exact recreation of a physical GBA screen.", 0, 1, 25},
    {"Retro TV", "Soft filtering with CRT scanlines and shading at 35%. A stylized TV look, not how the original handheld displayed the game.", 1, 2, 35},
};
inline constexpr int graphics_preset_count = sizeof(graphics_presets) / sizeof(graphics_presets[0]);
}
