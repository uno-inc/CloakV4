/*
Cloak V4
Copyright (C) 2025 Maintainer_(Ivan Shkatov) <ivanskatov672@gmail.com>
Copyright (C) 2025 Prounce <prouncedev@gmail.com>
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

/***************************************************************************************************
 *                                          NewMenu.cpp:                                           *
 *                                                                                                 *
 *          The menu supports various interactive elements, such as categories, sliders,           * 
 *      selection boxes, and dropdowns, using core::rect<s32> for mouse events and collisions      *
 *                                                                                                 *
 *                                  Core functionality includes:                                   *
 *                                                                                                 *
 * - Dynamic creation and cleanup of GUI elements via `create()` and `close()` methods.            *
 * - Drawing methods (`drawCategory`, `drawSelectionBox`, etc.) for rendering menu components.     *
 * - Event handling through `OnEvent()` for togglable cheats, sliders, and selection boxes.        *
 * - Helper methods for geometric calculations and slider value mapping.                           *
 *                                                                                                 *
 *                            This code is cursed as fuck, but it works.                           *
 *                       When you work on this file, increment the counter:                        *
 *                                   Total hours spent here: 195                                   *
 *                                                                                                 *
 *                        This makes me want to jump off a bridge -plus22                          *
 ***************************************************************************************************/

#include "NewMenu.h"
#include "porting.h"
#include "filesys.h"
#include <chrono>
#include "client/color_theme.h"

std::chrono::high_resolution_clock::time_point NewMenu::lastTime = std::chrono::high_resolution_clock::now();

float NewMenu::getDeltaTime() {
    auto currentTime = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> deltaTime = currentTime - lastTime;
    lastTime = currentTime;

    return deltaTime.count();
}

NewMenu::NewMenu(gui::IGUIEnvironment* env, 
    gui::IGUIElement* parent, 
    s32 id, IMenuManager* menumgr, 
    Client* client)
    : IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, 
    core::rect<s32>(0, 0, 0, 0)), 
    m_menumgr(menumgr), isDragging(false), draggedRectIndex(-1),
    m_client(client)
{    
    infostream << "[NEWMENU] Successfully created" << std::endl;
    this->env = env;
}

NewMenu::~NewMenu()
{
    for (size_t i = 0; i < cheatSettingTextFields.size(); ++i) {
        for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
            for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
                delete cheatSettingTextFields[i][c][s];
            }
        }
    }

    for (auto element : hudElements) {
        delete element;
    }
    hudElements.clear();
}

s32 NewMenu::roundToGrid(s32 num) {
    return std::round(num / (category_height / 2)) * (category_height / 2);
}

float slowTransitionHue(float h) {
    if (h >= 2.0f && h < 3.0f) {
        return 2.0f + (h - 2.0f) / 2.0f;
    } else if (h >= 3.0f) {
        return 2.5f + (h - 3.0f);
    } else {
        return h;
    }
}

video::SColor getNextRainbowColor() {
    static float m_rainbow_offset = 0.0f;
    static auto last_update = std::chrono::steady_clock::now();

    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<float> elapsed = now - last_update;

    if (elapsed.count() >= 0.05f) {
        m_rainbow_offset += 0.06f;
        if (m_rainbow_offset >= 6.0f)
            m_rainbow_offset -= 6.0f;
        last_update = now;
    }

    float h = m_rainbow_offset;
    h = slowTransitionHue(h);

    float x = (1 - fabs(fmod(h, 2.0f) - 1.0f)) * 255.0f;

    switch ((int)h) {
        case 0: return video::SColor(255, 255, (u8)x, 0);
        case 1: return video::SColor(255, (u8)x, 255, 0);
        case 2: return video::SColor(255, 0, 255, (u8)x);
        case 3: return video::SColor(255, 0, (u8)x, 255);
        case 4: return video::SColor(255, (u8)x, 0, 255);
        case 5: return video::SColor(255, 255, 0, (u8)x);
        default: return video::SColor(255, 255, 0, 0);
    }
}


void NewMenu::setColorsFromTheme(const ColorTheme theme)
{
    settingBackgroundColor = theme.background_top;
    settingBarColor = theme.primary;
    sliderColor = theme.text;
    sliderBarColor = theme.text;
    sliderColorActive = theme.text_muted;
    sliderBarColorActive = theme.primary_muted;
    option_color = theme.background_top;
}

void NewMenu::setWidthFromMultiplier(const s32 multiplier)
{
    const auto screenSize = Environment->getVideoDriver()->getScreenSize();

    category_height = std::max(1, s32(screenSize.Height * 0.035 * (multiplier / 10.0f)));

    // Make rows tall enough for the real font, so text fits on phones.
    if (g_fontengine) {
        gui::IGUIFont *fit_font = g_fontengine->getFont(g_fontengine->getFontSize(FM_HD) * 0.8, FM_HD);
        if (fit_font) {
            const s32 font_h = s32(fit_font->getDimension(L"Ag").Height);
            category_height = std::max(category_height, s32(font_h * 1.25));
        }
    }

    // Preserve the original panel proportions unless all three columns
    // would exceed the available screen width.
    const s32 gap = std::max(4, category_height / 4);
    const s32 maxPanelWidth = std::max<s32>(1, (static_cast<s32>(screenSize.Width) - 4 * gap) / 3);
    category_width = std::max(1, std::min(category_height * 5, maxPanelWidth));

    setting_bar_width = std::max(1, s32(category_height * 0.1));
    setting_bar_padding = std::max(1, s32(category_height * 0.1));
    slider_height_padding = std::max(1, s32(category_height * 0.4));
    slider_width_padding = std::max(1, s32(category_height * 0.3));
    selection_box_padding = std::max(1, s32(category_height * 0.1));

    if (m_initialized) {
        GET_SCRIPT_POINTER
        for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {
            respaceMenu(i);
        }
    }
}

void NewMenu::adjustCategoryPositions() {
    if (!m_initialized) return;

    video::IVideoDriver* driver = Environment->getVideoDriver();
    auto newSize = driver->getScreenSize();

    if (newSize == lastScreenSize)
        return;

    // Recalculate panel dimensions after orientation or display-size changes.
    setWidthFromMultiplier(g_settings->getS32("WidthMultiplier"));

    float scaleX = float(newSize.Width) / lastScreenSize.Width;
    float scaleY = float(newSize.Height) / lastScreenSize.Height;

    for (size_t i = 0; i < category_positions.size(); ++i) {
        category_positions[i].X = std::round(category_positions[i].X * scaleX);
        category_positions[i].Y = std::round(category_positions[i].Y * scaleY);
    }

    for (size_t i = 0; i < category_positions.size(); ++i) {
        moveMenu(i, category_positions[i]);
    }

    lastScreenSize = newSize;
}

void NewMenu::create()
{
    GET_SCRIPT_POINTER

    if (script->m_cheat_categories.empty()) {
        std::cout << "No categories available." << std::endl;
        return;
    }

    if (!m_initialized) {
        for (auto element : hudElements) {
            delete element;
        }
        hudElements.clear();

        themes_path = porting::path_user + DIR_DELIM + "themes";
        theme_manager = ThemeManager();
        theme_manager.LoadThemes(themes_path);

        s32 multiplier = g_settings->getS32("WidthMultiplier");

        setWidthFromMultiplier(multiplier);

        if (g_settings->exists("ColorTheme")) {
            current_theme_name = g_settings->get("ColorTheme");
        } else {
            current_theme_name = "Modern";
            g_settings->set("ColorTheme", current_theme_name);
        }

        current_theme = theme_manager.GetThemeByName(current_theme_name);
        setColorsFromTheme(current_theme);

        video::IVideoDriver* driver = Environment->getVideoDriver();
        lastScreenSize = driver->getScreenSize();

        // INITIALIZE CHEAT HUD ELEMENTS
        if (g_settings->exists("HudElement_Position1_target")) {
            v2f position = g_settings->getV2F("HudElement_Position1_target");
            v2f position2 = g_settings->getV2F("HudElement_Position2_target");
            if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                hudElements.push_back(new TargetHUD(core::rect<s32>(roundToGrid(position.X), roundToGrid(position.Y), roundToGrid(position2.X), roundToGrid(position2.Y))));
            } else {
                hudElements.push_back(new TargetHUD(core::rect<s32>(position.X, position.Y, position2.X, position2.Y)));
            }
        } else {
            hudElements.push_back(new TargetHUD(core::rect<s32>(400, 400, 600, 500)));
        }

        hudElements[0]->elementName = "target";

        if (g_settings->exists("HudElement_Position1_coords")) {
            v2f position = g_settings->getV2F("HudElement_Position1_coords");
            v2f position2 = g_settings->getV2F("HudElement_Position2_coords");
            if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                hudElements.push_back(new coordsHUD(core::rect<s32>(roundToGrid(position.X), roundToGrid(position.Y), roundToGrid(position2.X), roundToGrid(position2.Y))));
            } else {
                hudElements.push_back(new coordsHUD(core::rect<s32>(position.X, position.Y, position2.X, position2.Y)));
            }
        } else {
            hudElements.push_back(new coordsHUD(core::rect<s32>(400, 400, 600, 500)));
        }

        hudElements[1]->elementName = "coords";
        editHUDbuttonBounds = core::rect<s32>(
            roundToGrid(lastScreenSize.Width - ((category_height/2) * 9)),
            roundToGrid(lastScreenSize.Height - ((category_height/2) * 3)),
            roundToGrid(lastScreenSize.Width - (category_height/2)) + 1,
            roundToGrid(lastScreenSize.Height - (category_height/2)) + 1
        );

        // Ensure "Client" category exists
        ScriptApiCheatsCategory *client_category = nullptr;

        for (auto *category : script->m_cheat_categories) {
            if (category && category->m_name == "Client") {
                client_category = category;
                break;
            }
        }

        if (!client_category) {
            client_category = new ScriptApiCheatsCategory("Client");
            script->m_cheat_categories.push_back(client_category);
        }

        // Ensure "Appearance" cheat exists in Client category
        ScriptApiCheatsCheat *appearance_cheat = nullptr;
        ScriptApiCheatsCheat *grid = nullptr;
        ScriptApiCheatsCheat *hints_cheat = nullptr;

        for (auto *cheat : client_category->m_cheats) {
            if (cheat && cheat->m_name == "Appearance") {
                appearance_cheat = cheat;
                break;
            }
        }

        for (auto *cheat : client_category->m_cheats) {
            if (cheat && cheat->m_name == "MenuGrid") {
                grid = cheat;
                break;
            }
        }

        for (auto *cheat : client_category->m_cheats) {
            if (cheat && cheat->m_name == "Hints") {
                hints_cheat = cheat;
                break;
            }
        }

        if (!appearance_cheat) {
            appearance_cheat = new ScriptApiCheatsCheat("Appearance", "appearance", "");
            client_category->m_cheats.push_back(appearance_cheat);
        }

        if (!grid) {
            grid = new ScriptApiCheatsCheat("MenuGrid", "use_menu_grid", "");
            client_category->m_cheats.push_back(grid);
        }

        if (!hints_cheat) {
            hints_cheat = new ScriptApiCheatsCheat("Hints", "use_hints", "");
            client_category->m_cheats.push_back(hints_cheat);
        }

        // --- Ensure "Theme" setting exists or update it ---
        ScriptApiCheatsCheatSetting* theme_setting = nullptr;

        for (auto *setting : appearance_cheat->m_cheat_settings) {
            if (setting && setting->m_name == "Theme") {
                theme_setting = setting;
                break;
            }
        }

        if (!theme_setting) {
            theme_setting = new ScriptApiCheatsCheatSetting("Theme", "ColorTheme");
            theme_setting->m_type = "selectionbox";
            appearance_cheat->m_cheat_settings.push_back(theme_setting);
        } else {
            // Clear old options if already present
            theme_setting->m_options.clear();
        }

        std::vector<std::string> theme_names = theme_manager.GetThemes();
        for (const std::string &name : theme_names) {
            theme_setting->m_options.push_back(new std::string(name));
        }

        // --- Ensure "Width Scale" setting exists or update it ---
        ScriptApiCheatsCheatSetting* width_multiplier_setting = nullptr;

        for (auto *setting : appearance_cheat->m_cheat_settings) {
            if (setting && setting->m_name == "Menu size") {
                width_multiplier_setting = setting;
                break;
            }
        }

        if (!width_multiplier_setting) {
            width_multiplier_setting = new ScriptApiCheatsCheatSetting("Menu size", "WidthMultiplier");
            width_multiplier_setting->m_type = "selectionbox";
            appearance_cheat->m_cheat_settings.push_back(width_multiplier_setting);
        } else {
            width_multiplier_setting->m_options.clear();
        }

        for (int i = 8; i <= 13; ++i) {
            width_multiplier_setting->m_options.push_back(new std::string(std::to_string(i)));
        }

        // START RESIZING ALL ARRAYS
        category_positions.resize(script->m_cheat_categories.size(), core::position2d<s32>(0, 0));
        categoryRects.resize(script->m_cheat_categories.size(), core::rect<s32>(0,0,0,0));
        dropdownRects.resize(script->m_cheat_categories.size(), core::rect<s32>(0,0,0,0));
        textRects.resize(script->m_cheat_categories.size(), core::rect<s32>(0,0,0,0));
        selectionBoxRects.resize(script->m_cheat_categories.size());
        selectedCategory.resize(script->m_cheat_categories.size(), false);
        selectedCheat.resize(script->m_cheat_categories.size());
        dropdownHovered.resize(script->m_cheat_categories.size(), false);
        textHovered.resize(script->m_cheat_categories.size(), false);
        cheatRects.resize(script->m_cheat_categories.size());
        cheatRectAnimationProgress.resize(script->m_cheat_categories.size());
        cheatTextRects.resize(script->m_cheat_categories.size());
        cheatDropdownRects.resize(script->m_cheat_categories.size());
        cheatDropdownHovered.resize(script->m_cheat_categories.size());
        cheat_positions.resize(script->m_cheat_categories.size());
        cheatTextHovered.resize(script->m_cheat_categories.size());
        cheatSettingRects.resize(script->m_cheat_categories.size());
        cheatSliderRects.resize(script->m_cheat_categories.size());
        cheatSliderBarRects.resize(script->m_cheat_categories.size());
        cheatSettingTextRects.resize(script->m_cheat_categories.size());
        cheatSettingTextFields.resize(script->m_cheat_categories.size());
        cheatSettingTextLasts.resize(script->m_cheat_categories.size());
        cheatSettingTextHovered.resize(script->m_cheat_categories.size());
        cheatSliderHovered.resize(script->m_cheat_categories.size());
        selectionBoxHovered.resize(script->m_cheat_categories.size());
        cheat_setting_positions.resize(script->m_cheat_categories.size());
        cheatSettingOptionHovered.resize(script->m_cheat_categories.size());
        cheatSettingOptionRects.resize(script->m_cheat_categories.size());

        for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {
            if (g_settings->exists("Category_Dropdown_" + std::to_string(i)) && g_settings->getBool("save_menu_category_positions")) {
                selectedCategory[i] = g_settings->getBool("Category_Dropdown_" + std::to_string(i));
            }
            
            if (g_settings->exists("Category_Position_" + std::to_string(i)) && g_settings->getBool("save_menu_category_positions")) {
                v2f position = g_settings->getV2F("Category_Position_" + std::to_string(i));
                category_positions[i] = core::position2d<s32>(position.X, position.Y);
            } else {
                category_positions[i] = core::position2d<s32>(category_height / 2, category_height / 2 + ((category_height + category_height / 2) * i));
            }
            categoryRects[i] = core::rect<s32>(category_positions[i].X, category_positions[i].Y, 
                                            category_positions[i].X + category_width, category_positions[i].Y + category_height);

            textRects[i] = core::rect<s32>(category_positions[i].X, category_positions[i].Y, 
                                        category_positions[i].X + (category_width - category_height), category_positions[i].Y + category_height);

            dropdownRects[i] = core::rect<s32>(category_positions[i].X + (category_width - category_height), category_positions[i].Y, 
                                            category_positions[i].X + category_width, category_positions[i].Y + category_height);
                                    
            cheatRects[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::rect<s32>(0,0,0,0));
            cheatRectAnimationProgress[i].resize(script->m_cheat_categories[i]->m_cheats.size(), 1.0f);
            cheatTextRects[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::rect<s32>(0,0,0,0));
            cheatDropdownRects[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::rect<s32>(0,0,0,0));
            cheatDropdownHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size(), false);
            cheatTextHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size(), false);
            selectedCheat[i].resize(script->m_cheat_categories[i]->m_cheats.size(), false);
            cheat_positions[i].resize(script->m_cheat_categories[i]->m_cheats.size(), core::position2d<s32>(0, 0));
            cheatSettingRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSliderRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSliderBarRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            selectionBoxRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextFields[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextLasts[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingTextHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSliderHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            selectionBoxHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheat_setting_positions[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingOptionHovered[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            cheatSettingOptionRects[i].resize(script->m_cheat_categories[i]->m_cheats.size());
            for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {
                if (g_settings->exists("Cheat_Dropdown_" + std::to_string(i) + "_" + std::to_string(c)) && g_settings->getBool("save_menu_category_positions")) {
                    selectedCheat[i][c] = g_settings->getBool("Cheat_Dropdown_" + std::to_string(i) + "_" + std::to_string(c));
                }
                cheatRectAnimationProgress[i][c] = static_cast<float>(!script->m_cheat_categories[i]->m_cheats[c]->is_enabled());
                cheatSettingRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSliderRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                selectionBoxRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSliderBarRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextFields[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextLasts[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingTextHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(), false);
                cheatSliderHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(), false);
                selectionBoxHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(), false);
                cheat_setting_positions[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingOptionHovered[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                cheatSettingOptionRects[i][c].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size());
                for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                    if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "text") {
                        std::wstring wname = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(g_settings->get(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting));
                        cheatSettingTextLasts[i][c][s] = wname;
                        cheatSettingTextFields[i][c][s] = env->addEditBox(wname.c_str(), core::rect<s32>(0, 0, category_width, category_height), true);
                        cheatSettingTextFields[i][c][s]->setDrawBackground(false);
                        cheatSettingTextFields[i][c][s]->setWordWrap(true);
                        cheatSettingTextFields[i][c][s]->setOverrideColor(video::SColor(173, 35, 45, 56));
                        cheatSettingTextFields[i][c][s]->setVisible(false);
                    } else if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_type == "selectionbox") {
                        for (size_t o = 0; o < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(); ++o) {
                            cheatSettingOptionHovered[i][c][s].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(), false);
                            cheatSettingOptionRects[i][c][s].resize(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size());
                        }
                    }
                }
                
                moveMenu(i, category_positions[i]);
            }
        }

        // Restore at most one open category.
        size_t activeCategory = selectedCategory.size();
        for (size_t i = 0; i < selectedCategory.size(); ++i) {
            if (selectedCategory[i])
                activeCategory = i;
        }

        for (size_t i = 0; i < selectedCategory.size(); ++i) {
            selectedCategory[i] = (i == activeCategory);

            // Restore at most one open settings panel per category.
            size_t activeCheat = selectedCheat[i].size();
            for (size_t c = 0; c < selectedCheat[i].size(); ++c) {
                if (selectedCheat[i][c])
                    activeCheat = c;
            }

            for (size_t c = 0; c < selectedCheat[i].size(); ++c)
                selectedCheat[i][c] = (c == activeCheat);

            // Refresh geometry, saved dropdown state and text-field visibility.
            moveMenu(i, category_positions[i]);
        }

        m_initialized = true;
    } else {
        for (size_t i = 0; i < cheatSettingTextFields.size(); ++i) {
            for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
                for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
                    if (cheatSettingTextFields[i][c][s] != nullptr) {
                        cheatSettingTextFields[i][c][s]->setVisible(selectedCategory[i] && selectedCheat[i][c]);
                    }
                }
            }
        }
    }

    core::rect<s32> screenRect(0, 0, 
        Environment->getVideoDriver()->getScreenSize().Width, 
        Environment->getVideoDriver()->getScreenSize().Height);
    setRelativePosition(screenRect);

    IGUIElement::setVisible(true);
    Environment->setFocus(this);
    m_menumgr->createdMenu(this);
    m_is_open = true;
}

void NewMenu::close()
{
    for (size_t i = 0; i < cheatSettingTextFields.size(); ++i) {
        for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
            for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
                if (cheatSettingTextFields[i][c][s] != nullptr) {
                    cheatSettingTextFields[i][c][s]->setVisible(false);
                }
            }
        }
    }
    Environment->removeFocus(this);
    m_menumgr->deletingMenu(this);
    IGUIElement::setVisible(false);
    m_is_open = false;
}

double NewMenu::roundToNearestStep(double number, double m_min, double m_max, double m_steps)
{
    double stepSize = (m_max - m_min) / (m_steps - 1);
    return m_min + stepSize * round((number - m_min) / stepSize);
}

void NewMenu::drawInterpolatedRectangle(video::IVideoDriver* driver, const core::rect<s32>& rect, video::SColor innerColor, video::SColor outerColor, float interpolation)
{
    interpolation = core::clamp(interpolation, 0.0f, 1.0f);

    if (interpolation == 1.0f) {
        driver->draw2DRectangle(outerColor, rect);
        return;
    }

    if (interpolation == 0.0f) {
        driver->draw2DRectangle(innerColor, rect);
        return;
    }

    s32 midX = (rect.UpperLeftCorner.X + rect.LowerRightCorner.X) / 2;
    s32 interpWidth = static_cast<s32>((rect.getWidth() * (1.0f - interpolation)) / 2);

    core::rect<s32> innerRect(midX - interpWidth, rect.UpperLeftCorner.Y, midX + interpWidth, rect.LowerRightCorner.Y);
    driver->draw2DRectangle(innerColor, innerRect);
    
    driver->draw2DRectangle(outerColor, core::rect<s32>(rect.UpperLeftCorner.X, rect.UpperLeftCorner.Y, innerRect.UpperLeftCorner.X, rect.LowerRightCorner.Y));
    driver->draw2DRectangle(outerColor, core::rect<s32>(innerRect.LowerRightCorner.X, rect.UpperLeftCorner.Y, rect.LowerRightCorner.X, rect.LowerRightCorner.Y));
}

void NewMenu::calculateSliderSplit(
    const core::rect<s32>& sliderRect, 
    double value, 
    double minValue, 
    double maxValue, 
    core::rect<s32>& filledRect, 
    core::rect<s32>& remainingRect
) {
    if (minValue >= maxValue) {
        filledRect = sliderRect;
        remainingRect = core::rect<s32>(0, 0, 0, 0);
    }

    value = std::max(minValue, std::min(value, maxValue));

    double ratio = (value - minValue) / (maxValue - minValue);
    int splitX = sliderRect.UpperLeftCorner.X + static_cast<int>(sliderRect.getWidth() * ratio);

    filledRect = core::rect<s32>(
        sliderRect.UpperLeftCorner.X,
        sliderRect.UpperLeftCorner.Y,
        splitX,
        sliderRect.LowerRightCorner.Y
    );

    remainingRect = core::rect<s32>(
        splitX,
        sliderRect.UpperLeftCorner.Y,
        sliderRect.LowerRightCorner.X,
        sliderRect.LowerRightCorner.Y
    );
}

double mapValue(double value, double oldMin, double oldMax, double newMin, double newMax) {
    return newMin + (value - oldMin) * (newMax - newMin) / (oldMax - oldMin);
}

double NewMenu::calculateSliderValueFromPosition(const core::rect<s32>& sliderBarRect, const core::position2d<s32>& pointerPosition, double m_min, double m_max, double m_steps)
{

    s32 clampedX = std::clamp(pointerPosition.X, sliderBarRect.UpperLeftCorner.X, sliderBarRect.LowerRightCorner.X);


    double sliderValue = mapValue(clampedX, sliderBarRect.UpperLeftCorner.X, sliderBarRect.LowerRightCorner.X, m_min, m_max);

    return roundToNearestStep(sliderValue, m_min, m_max, m_steps);
}

s32 NewMenu::respaceMenu(size_t i)
{
    GET_SCRIPT_POINTER_S32

    const s32 gap = std::max(4, category_height / 4);

    const s32 category_x = category_positions[i].X;
    const s32 cheat_x = category_x + category_width + gap;
    const s32 settings_x = cheat_x + category_width + gap;

    const s32 category_y = category_positions[i].Y;

    categoryRects[i] = core::rect<s32>(
        category_x, category_y,
        category_x + category_width, category_y + category_height);

    textRects[i] = core::rect<s32>(
        category_x, category_y,
        category_x + category_width - category_height,
        category_y + category_height);

    dropdownRects[i] = core::rect<s32>(
        category_x + category_width - category_height, category_y,
        category_x + category_width, category_y + category_height);

    s32 last_height = category_y + category_height + 1;

    // AutoHeal slot toggles: square cells that fit the settings column.
    const s32 grid_cols = 5;
    const s32 grid_cell = std::max<s32>(1, category_width / grid_cols);
    const std::string grid_prefix = "auto_heal.slot_";

    for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {
        const s32 cheat_y = category_y +
                            static_cast<s32>(c) * category_height;

        cheat_positions[i][c] = core::position2d<s32>(cheat_x, cheat_y);

        cheatRects[i][c] = core::rect<s32>(
            cheat_x, cheat_y,
            cheat_x + category_width, cheat_y + category_height);

        auto *cheat = script->m_cheat_categories[i]->m_cheats[c];

        if (cheat->has_settings()) {
            cheatTextRects[i][c] = core::rect<s32>(
                cheat_x, cheat_y,
                cheat_x + category_width - category_height,
                cheat_y + category_height);

            cheatDropdownRects[i][c] = core::rect<s32>(
                cheat_x + category_width - category_height, cheat_y,
                cheat_x + category_width, cheat_y + category_height);

            if (selectedCheat[i][c]) {
                s32 setting_y = cheat_y;
                const bool is_autoheal = (cheat->m_name == "AutoHeal");
                std::vector<size_t> slot_indices;

                for (size_t s = 0; s < cheat->m_cheat_settings.size(); ++s) {
                    const std::string &type = cheat->m_cheat_settings[s]->m_type;

                    if (is_autoheal &&
                        cheat->m_cheat_settings[s]->m_setting.compare(0, grid_prefix.size(), grid_prefix) == 0) {
                        slot_indices.push_back(s);
                        continue;
                    }

                    cheat_setting_positions[i][c][s] =
                        core::position2d<s32>(settings_x, setting_y);

                    s32 setting_height = category_height;

                    if (type == "selectionbox" ||
                        type == "slider_int" ||
                        type == "slider_float") {
                        setting_height = category_height * 2;
                    } else if (type == "text") {
                        setting_height = category_height * 4;
                    }

                    cheatSettingRects[i][c][s] = core::rect<s32>(
                        settings_x, setting_y,
                        settings_x + category_width,
                        setting_y + setting_height);

                    if (type == "text" &&
                        cheatSettingTextFields[i][c][s] != nullptr) {
                        cheatSettingTextFields[i][c][s]->setRelativePosition(
                            core::rect<s32>(
                                settings_x + (setting_bar_padding * 2) + setting_bar_width,
                                setting_y + category_height,
                                settings_x + category_width - setting_bar_padding,
                                setting_y + setting_height - setting_bar_padding
                            ));
                    } else if (type != "selectionbox" &&
                               type != "slider_int" &&
                               type != "slider_float") {
                        cheatSettingTextRects[i][c][s] = core::rect<s32>(
                            settings_x + (setting_bar_padding * 2) + setting_bar_width,
                            setting_y,
                            settings_x + category_width,
                            setting_y + category_height);
                    }

                    setting_y += setting_height;
                }

                // Slot grid goes under the other settings. Each slot's cell
                // comes from its own number, not from the list order.
                if (!slot_indices.empty()) {
                    s32 max_index = 0;
                    for (size_t s : slot_indices) {
                        const std::string &sname = cheat->m_cheat_settings[s]->m_setting;
                        s32 index = std::atoi(sname.c_str() + grid_prefix.size()) - 1;
                        if (index < 0)
                            index = 0;
                        max_index = std::max(max_index, index);

                        const s32 x0 = settings_x + (index % grid_cols) * grid_cell;
                        const s32 y0 = setting_y + (index / grid_cols) * grid_cell;

                        cheat_setting_positions[i][c][s] = core::position2d<s32>(x0, y0);
                        cheatSettingRects[i][c][s] =
                            core::rect<s32>(x0, y0, x0 + grid_cell, y0 + grid_cell);
                        cheatSettingTextRects[i][c][s] = cheatSettingRects[i][c][s];
                    }
                    setting_y += ((max_index / grid_cols) + 1) * grid_cell;
                }

                last_height = std::max(last_height, setting_y);
            }
        } else {
            cheatTextRects[i][c] = core::rect<s32>(
                cheat_x, cheat_y,
                cheat_x + category_width, cheat_y + category_height);

            cheatDropdownRects[i][c] = core::rect<s32>(
                cheat_x + category_width, cheat_y,
                cheat_x + category_width, cheat_y);
        }

        last_height = std::max(last_height, cheat_y + category_height);
    }

    return last_height;
}

void NewMenu::moveMenu(size_t i, core::position2d<s32> new_position)
{
    const auto screenSize = Environment->getVideoDriver()->getScreenSize();
    const s32 screenWidth = screenSize.Width;
    const s32 screenHeight = screenSize.Height;

    s32 newX = std::max(0, new_position.X);
    s32 newY = std::max(0, new_position.Y);

    for (size_t c = 0; c < cheatSettingTextFields[i].size(); ++c) {
        g_settings->setBool(
            "Category_Dropdown_" + std::to_string(i),
            selectedCategory[i]);

        for (size_t s = 0; s < cheatSettingTextFields[i][c].size(); ++s) {
            g_settings->setBool(
                "Cheat_Dropdown_" + std::to_string(i) + "_" + std::to_string(c),
                selectedCheat[i][c]);

            if (cheatSettingTextFields[i][c][s] != nullptr) {
                cheatSettingTextFields[i][c][s]->setVisible(
                    selectedCategory[i] && selectedCheat[i][c]);
            }
        }
    }

    if (g_settings->exists("use_menu_grid") &&
        g_settings->getBool("use_menu_grid")) {
        const s32 gridSize = std::max(1, category_height / 2);
        newX = std::round(float(newX) / gridSize) * gridSize;
        newY = std::round(float(newY) / gridSize) * gridSize;
    }

    const s32 gap = std::max(4, category_height / 4);
    const s32 cascadeWidth = 3 * category_width + 2 * gap;
    const s32 maxX = std::max(gap, screenWidth - cascadeWidth - gap);
    newX = std::min(maxX, std::max(gap, newX));

    // Generate the rectangles at the proposed position first.
    category_positions[i] = core::position2d<s32>(newX, newY);
    respaceMenu(i);

    // Measure only content that is currently visible.
    s32 visibleBottom = newY + category_height;

    if (selectedCategory[i]) {
        GET_SCRIPT_POINTER

        const auto &cheats = script->m_cheat_categories[i]->m_cheats;

        for (size_t c = 0; c < cheats.size(); ++c) {
            visibleBottom = std::max(
                visibleBottom,
                cheatRects[i][c].LowerRightCorner.Y);

            if (!selectedCheat[i][c])
                continue;

            for (size_t s = 0; s < cheats[c]->m_cheat_settings.size(); ++s) {
                visibleBottom = std::max(
                    visibleBottom,
                    cheatSettingRects[i][c][s].LowerRightCorner.Y);
            }
        }

        // Include the expanded selection options popup.
        if (isSelecting && selectingCategoryIndex == i &&
            selectingCheatIndex < cheats.size() &&
            selectedCheat[i][selectingCheatIndex]) {
            const auto &options =
                cheats[selectingCheatIndex]
                    ->m_cheat_settings[selectingSettingIndex]->m_options;

            if (!options.empty()) {
                visibleBottom = std::max(
                    visibleBottom,
                    cheatSettingRects[selectingCategoryIndex]
                        [selectingCheatIndex][selectingSettingIndex]
                            .LowerRightCorner.Y +
                    static_cast<s32>(options.size()) * category_height);
            }
        }
    }

    const s32 menuHeight = std::max(0, visibleBottom - newY);
    const s32 maxY = std::max(0, screenHeight - menuHeight);
    newY = std::min(newY, maxY);

    // Rebuild all rectangles at the final clamped position.
    category_positions[i] = core::position2d<s32>(newX, newY);
    respaceMenu(i);

    g_settings->setV2F(
        "Category_Position_" + std::to_string(i),
        v2f(newX, newY));
}

bool NewMenu::OnEvent(const irr::SEvent& event) 
{
    s32 screenWidth = Environment->getVideoDriver()->getScreenSize().Width, screenHeight = Environment->getVideoDriver()->getScreenSize().Height;
    if (!m_is_open) {
        return false;
    }
        // Convert touchscreen input into mouse events so the menu
    // can use its existing click, drag and slider handling.
    if (event.EventType == irr::EET_TOUCH_INPUT_EVENT) {
        irr::SEvent mouse_event = {};
        mouse_event.EventType = irr::EET_MOUSE_INPUT_EVENT;
        mouse_event.MouseInput.X = event.TouchInput.X;
        mouse_event.MouseInput.Y = event.TouchInput.Y;

        switch (event.TouchInput.Event) {
        case irr::ETIE_PRESSED_DOWN:
            mouse_event.MouseInput.Event = irr::EMIE_LMOUSE_PRESSED_DOWN;
            break;
        case irr::ETIE_LEFT_UP:
            mouse_event.MouseInput.Event = irr::EMIE_LMOUSE_LEFT_UP;
            break;
        case irr::ETIE_MOVED:
            mouse_event.MouseInput.Event = irr::EMIE_MOUSE_MOVED;
            break;
        default:
            return false;
        }

        return OnEvent(mouse_event);
    }
    GET_SCRIPT_POINTER_BOOL  
    if (event.EventType == irr::EET_KEY_INPUT_EVENT)
    {
        if (event.KeyInput.Key == KEY_ESCAPE)
        {
            close();
            return true; 
        }
    }
    
    setWidthFromMultiplier(g_settings->getS32("WidthMultiplier"));
    adjustCategoryPositions();
    
    if (event.EventType == irr::EET_MOUSE_INPUT_EVENT && !isEditing) {
        if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
            if (isSelecting) {
                for (size_t o = 0; o < script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_options.size(); ++o) {
                    if (cheatSettingOptionRects[selectingCategoryIndex][selectingCheatIndex][selectingSettingIndex][o].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                        g_settings->set(script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_setting, 
                            script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_options[o]->c_str());
                    }
                    if(script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_name == "Theme") {
                        current_theme_name = g_settings->get("ColorTheme");
                        current_theme = theme_manager.GetThemeByName(current_theme_name);
                        setColorsFromTheme(current_theme);
                    }
                    if(script->m_cheat_categories[selectingCategoryIndex]->m_cheats[selectingCheatIndex]->m_cheat_settings[selectingSettingIndex]->m_setting == "WidthMultiplier") {
                        s32 multiplier = g_settings->getS32("WidthMultiplier");
                        setWidthFromMultiplier(multiplier);
                        for (size_t menuIndex = 0;
                             menuIndex < category_positions.size(); ++menuIndex) {
                            moveMenu(menuIndex, category_positions[menuIndex]);
                        }
                    }
                }

                isSelecting=false;
                return true;
            }

            if (editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                isEditing = !isEditing;
                return true;
            }

            for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {
                if (dropdownRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                    bool opening = !selectedCategory[i];

                    if (opening) {
                        for (size_t j = 0; j < selectedCategory.size(); ++j) {
                            if (j != i && selectedCategory[j]) {
                                selectedCategory[j] = false;

                                for (size_t c = 0; c < cheatSettingTextFields[j].size(); ++c) {
                                    for (size_t s = 0; s < cheatSettingTextFields[j][c].size(); ++s) {
                                        if (cheatSettingTextFields[j][c][s] != nullptr)
                                            cheatSettingTextFields[j][c][s]->setVisible(false);
                                    }
                                }
                            }
                        }
                    }

                    selectedCategory[i] = opening;
                    moveMenu(i, category_positions[i]);
                    return true;
                }
                if (textRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                    isDragging = true;
                    draggedRectIndex = i;
                    offset = core::vector2d<s32>(event.MouseInput.X - textRects[i].UpperLeftCorner.X, event.MouseInput.Y - textRects[i].UpperLeftCorner.Y);
                    return true; 
                }
                if (selectedCategory[i]) {
                    for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {
                        if (cheatTextRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                            if (script->m_cheat_categories[i]->m_cheats[c]->m_name != "Appearance") {
                                script->toggle_cheat(script->m_cheat_categories[i]->m_cheats[c]);
                                return true; 
                            }
                        }

                        if (cheatDropdownRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                            const bool opening = !selectedCheat[i][c];
                            for (size_t other = 0; other < selectedCheat[i].size(); ++other) {
                                selectedCheat[i][other] = (other == c) && opening;
                                for (size_t s = 0; s < cheatSettingTextFields[i][other].size(); ++s) {
                                    if (cheatSettingTextFields[i][other][s] != nullptr) {
                                        cheatSettingTextFields[i][other][s]->setVisible(
                                            selectedCategory[i] && selectedCheat[i][other]);
                                    }
                                }
                            }
                            moveMenu(i, category_positions[i]);
                            return true; 
                        }

                        for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                            if (selectedCheat[i][c]) {
                                if (cheatSettingTextRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                                    if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting == "ReloadThemes") {
                                        theme_manager.LoadThemes(themes_path);
                                        m_initialized = false;
                                        create();
                                    } else {
                                        script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->toggle();
                                        if (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting.compare(0, 15, "auto_heal.slot_") == 0) {
                                            errorstream << "[AutoHeal] tapped slot " << script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_name
                                                << " now " << g_settings->getBool(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_setting) << std::endl;
                                        }
                                    }
                                }
                                if (cheatSliderRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y)) || cheatSliderBarRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                                    draggedSliderCategoryIndex = i;
                                    draggedSliderCheatIndex = c;
                                    draggedSliderSettingIndex = s;
                                    isSliding = true;
                                    ScriptApiCheatsCheatSetting* cheatSetting = script->m_cheat_categories[draggedSliderCategoryIndex]->m_cheats[draggedSliderCheatIndex]->m_cheat_settings[draggedSliderSettingIndex];
                                    cheatSetting->set_value(calculateSliderValueFromPosition(cheatSliderBarRects[draggedSliderCategoryIndex][draggedSliderCheatIndex][draggedSliderSettingIndex], core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y), cheatSetting->m_min, cheatSetting->m_max, cheatSetting->m_steps));
                                }
                                if (selectionBoxRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                                    selectingCategoryIndex = i;
                                    selectingCheatIndex = c;
                                    selectingSettingIndex = s;
                                    isSelecting = true;
                                }
                            }
                        }
                    }
                }
            }
            return false; 
        } else if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
            isDragging = false;
            isSliding = false;
            draggedRectIndex = -1;
            draggedSliderCategoryIndex = 0;
            draggedSliderCheatIndex = 0;
            draggedSliderSettingIndex = 0;
            return true;
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && isDragging && draggedRectIndex != -1) {
            moveMenu(draggedRectIndex, core::vector2d<s32>(event.MouseInput.X - offset.X, event.MouseInput.Y - offset.Y));
            return true;
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED && isSliding) {
            ScriptApiCheatsCheatSetting* cheatSetting = script->m_cheat_categories[draggedSliderCategoryIndex]->m_cheats[draggedSliderCheatIndex]->m_cheat_settings[draggedSliderSettingIndex];
            cheatSetting->set_value(calculateSliderValueFromPosition(cheatSliderBarRects[draggedSliderCategoryIndex][draggedSliderCheatIndex][draggedSliderSettingIndex], core::position2d<s32>(event.MouseInput.X, event.MouseInput.Y), cheatSetting->m_min, cheatSetting->m_max, cheatSetting->m_steps));
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED) {
            isEditingHovered = editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

            for (size_t i = 0; i < script->m_cheat_categories.size(); ++i) {

                dropdownHovered[i] = dropdownRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                textHovered[i] = textRects[i].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                for (size_t c = 0; c < script->m_cheat_categories[i]->m_cheats.size(); ++c) {

                    cheatTextHovered[i][c] = cheatTextRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                    cheatDropdownHovered[i][c] = cheatDropdownRects[i][c].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                    for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings.size(); ++s) {
                        cheatSettingTextHovered[i][c][s] = cheatSettingTextRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                        cheatSliderHovered[i][c][s] = cheatSliderRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                        selectionBoxHovered[i][c][s] = selectionBoxRects[i][c][s].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

                        for (size_t o = 0; o < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(); ++o) {
                            cheatSettingOptionHovered[i][c][s][o] = cheatSettingOptionRects[i][c][s][o].isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                        }
                    }
                }
            }
        }
    } else if (event.EventType == irr::EET_MOUSE_INPUT_EVENT) {
        if (event.MouseInput.Event == irr::EMIE_LMOUSE_PRESSED_DOWN) {
            if (editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                isEditing = !isEditing;
                return true;
            }
            for (size_t e = 0; e < hudElements.size(); ++e) {
                if (hudElements[e]->resizeBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y))) {
                    isResizingHUDElement = true;
                    resizingHUDElementIndex = e;
                    resizingHUDElementOffset = core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y) - hudElements[e]->bounds.LowerRightCorner;
                    return true;
                } else if (hudElements[e]->bounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y)))
                {
                    isDraggingHUDElement = true;
                    draggingHUDElementIndex = e;
                    draggingHUDElementOffset = hudElements[e]->bounds.UpperLeftCorner - core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y);
                    return true;
                }
            }
        } else if (event.MouseInput.Event == irr::EMIE_LMOUSE_LEFT_UP) {
            isResizingHUDElement = false;
            isDraggingHUDElement = false;
        } else if (event.MouseInput.Event == irr::EMIE_MOUSE_MOVED) {
            isEditingHovered = editHUDbuttonBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));

            if (isResizingHUDElement) {
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements[resizingHUDElementIndex]->bounds = core::rect<s32>(
                        roundToGrid(hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.X),
                        roundToGrid(hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.Y),
                        roundToGrid(event.MouseInput.X + resizingHUDElementOffset.X),
                        roundToGrid(event.MouseInput.Y + resizingHUDElementOffset.Y)
                    );
                } else {
                    hudElements[resizingHUDElementIndex]->bounds = core::rect<s32>(
                        hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.X,
                        hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.Y,
                        event.MouseInput.X + resizingHUDElementOffset.X,
                        event.MouseInput.Y + resizingHUDElementOffset.Y
                    );
                }
                hudElements[resizingHUDElementIndex]->resizeBounds = core::rect<s32>(
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.X - 5,
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.Y - 5,
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.X + 5,
                    hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.Y + 5
                );

                g_settings->setV2F("HudElement_Position1_" + hudElements[resizingHUDElementIndex]->elementName, v2f(hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.X, hudElements[resizingHUDElementIndex]->bounds.UpperLeftCorner.Y));
                g_settings->setV2F("HudElement_Position2_" + hudElements[resizingHUDElementIndex]->elementName, v2f(hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.X, hudElements[resizingHUDElementIndex]->bounds.LowerRightCorner.Y));
            } else if (isDraggingHUDElement) {
                s32 rectWidth = hudElements[draggingHUDElementIndex]->bounds.getWidth();
                s32 rectHeight = hudElements[draggingHUDElementIndex]->bounds.getHeight();
                if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid")) {
                    hudElements[draggingHUDElementIndex]->bounds = core::rect<s32>(
                        roundToGrid(event.MouseInput.X + draggingHUDElementOffset.X),
                        roundToGrid(event.MouseInput.Y + draggingHUDElementOffset.Y),
                        roundToGrid(event.MouseInput.X + draggingHUDElementOffset.X + rectWidth),
                        roundToGrid(event.MouseInput.Y + draggingHUDElementOffset.Y + rectHeight)
                    );
                } else {
                    hudElements[draggingHUDElementIndex]->bounds = core::rect<s32>(
                        event.MouseInput.X + draggingHUDElementOffset.X,
                        event.MouseInput.Y + draggingHUDElementOffset.Y,
                        event.MouseInput.X + draggingHUDElementOffset.X + rectWidth,
                        event.MouseInput.Y + draggingHUDElementOffset.Y + rectHeight
                    );
                }
                hudElements[draggingHUDElementIndex]->resizeBounds = core::rect<s32>(
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.X - 5,
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.Y - 5,
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.X + 5,
                    hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.Y + 5
                );
                g_settings->setV2F("HudElement_Position1_" + hudElements[draggingHUDElementIndex]->elementName, v2f(hudElements[draggingHUDElementIndex]->bounds.UpperLeftCorner.X, hudElements[draggingHUDElementIndex]->bounds.UpperLeftCorner.Y));
                g_settings->setV2F("HudElement_Position2_" + hudElements[draggingHUDElementIndex]->elementName, v2f(hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.X, hudElements[draggingHUDElementIndex]->bounds.LowerRightCorner.Y));
            }
            
            for (auto element : hudElements) {
                element->isResizeHovered = element->resizeBounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y));
                element->isHovered = (element->bounds.isPointInside(core::vector2d<s32>(event.MouseInput.X, event.MouseInput.Y)) && !element->isResizeHovered);
            }
            return true;
        }
    }
    
    return Parent ? Parent->OnEvent(event) : false; 
}

extern video::ITexture *g_bh_scene_tex;
static video::ITexture *bh_blur_tex[4] = {nullptr, nullptr, nullptr, nullptr};
static core::dimension2d<u32> bh_blur_size(0, 0);
static core::dimension2d<u32> bh_screen_size(0, 0);
static bool bh_blur_ready = false;

static void bhUpdateBlur(video::IVideoDriver *driver)
{
    bh_blur_ready = false;
    if (!g_bh_scene_tex || !g_settings->getBool("glass_blur"))
        return;
    core::dimension2d<u32> ss = g_bh_scene_tex->getSize();
    if (ss.Width < 64 || ss.Height < 64)
        return;
    if (ss != bh_blur_size) {
        for (auto &t : bh_blur_tex) {
            if (t) {
                driver->removeTexture(t);
                t = nullptr;
            }
        }
        bh_blur_size = ss;
        for (int i = 0; i < 4; i++) {
            u32 d = 2u << i;
            std::string nm = "bh_blur" + std::to_string(i);
            bh_blur_tex[i] = driver->addRenderTargetTexture(
                core::dimension2d<u32>(ss.Width / d, ss.Height / d), nm.c_str(), video::ECF_A8R8G8B8);
            if (!bh_blur_tex[i])
                return;
        }
    }
    bh_screen_size = driver->getScreenSize();
    video::ITexture *src = g_bh_scene_tex;
    core::dimension2d<u32> ssz = ss;
    for (int i = 0; i < 4; i++) {
        video::ITexture *dst = bh_blur_tex[i];
        if (!dst)
            return;
        core::dimension2d<u32> dsz = dst->getSize();
        driver->setRenderTarget(dst, video::ECBF_COLOR);
        driver->draw2DImage(src, core::rect<s32>(0, 0, dsz.Width, dsz.Height),
                core::rect<s32>(0, 0, ssz.Width, ssz.Height));
        src = dst;
        ssz = dsz;
    }
    driver->setRenderTargetEx(nullptr, video::ECBF_NONE);
    driver->OnResize(driver->getScreenSize());
    bh_blur_ready = true;
}

static void bhBlurRect(video::IVideoDriver *driver, const core::rect<s32> &r)
{
    if (!bh_blur_ready || !bh_blur_tex[3] || bh_screen_size.Width == 0)
        return;
    core::dimension2d<u32> ts = bh_blur_tex[3]->getSize();
    core::rect<s32> d = r;
    d.clipAgainst(core::rect<s32>(0, 0, bh_screen_size.Width, bh_screen_size.Height));
    if (d.getWidth() <= 0 || d.getHeight() <= 0)
        return;
    f32 fx = (f32)ts.Width / (f32)bh_screen_size.Width;
    f32 fy = (f32)ts.Height / (f32)bh_screen_size.Height;
    s32 x0 = std::max<s32>(0, (s32)(d.UpperLeftCorner.X * fx));
    s32 x1 = std::min<s32>((s32)ts.Width, (s32)(d.LowerRightCorner.X * fx) + 1);
    s32 y0 = std::max<s32>(0, (s32)(d.UpperLeftCorner.Y * fy));
    s32 y1 = std::min<s32>((s32)ts.Height, (s32)(d.LowerRightCorner.Y * fy) + 1);
    if (g_settings->getBool("glass_blur_flip")) {
        s32 t0 = (s32)ts.Height - y1;
        s32 t1 = (s32)ts.Height - y0;
        y0 = t0;
        y1 = t1;
    }
    if (x1 <= x0 || y1 <= y0)
        return;
    driver->draw2DImage(bh_blur_tex[3], d, core::rect<s32>(x0, y0, x1, y1));
}

static void bhGlassEdges(video::IVideoDriver *driver, const core::rect<s32> &r)
{
    if (r.getWidth() < 12 || r.getHeight() < 12)
        return;
    s32 band = std::min<s32>(r.getHeight() / 4, 14);
    driver->draw2DRectangle(core::rect<s32>(r.UpperLeftCorner.X + 1, r.UpperLeftCorner.Y + 1,
            r.LowerRightCorner.X - 1, r.UpperLeftCorner.Y + 1 + band),
        video::SColor(60, 255, 255, 255), video::SColor(60, 255, 255, 255),
        video::SColor(0, 255, 255, 255), video::SColor(0, 255, 255, 255));
    driver->draw2DRectangle(core::rect<s32>(r.UpperLeftCorner.X + 1, r.LowerRightCorner.Y - 1 - band,
            r.LowerRightCorner.X - 1, r.LowerRightCorner.Y - 1),
        video::SColor(0, 0, 0, 0), video::SColor(0, 0, 0, 0),
        video::SColor(55, 0, 0, 0), video::SColor(55, 0, 0, 0));
    driver->draw2DRectangleOutline(r, video::SColor(60, 255, 255, 255));
}

void NewMenu::drawCategory(video::IVideoDriver* driver, gui::IGUIFont* font, const size_t i, float dtime)
{
    GET_SCRIPT_POINTER
    // TODO REMOVE THESE AND STORE THEM AS SETTINGS
    const s32 unit_size = category_height / 9;

    const video::SColor rainbow_category_color = video::SColor(255, 21, 21, 21);
    const video::SColor rainbow_cheat_color = getNextRainbowColor();
    const video::SColor rainbow_cheat_color_enabled = video::SColor(255, 21, 21, 21);
    const video::SColor rainbow_outline_color = getNextRainbowColor();
    
    ///////////////////////// DRAW CATEGORY HEADER /////////////////////////
    video::SColor arrow_color = current_theme.text;
    video::SColor text_color = current_theme.text;
    if (dropdownHovered[i]) {
        arrow_color = current_theme.text_muted;
    }
    if (textHovered[i]) {
        text_color = current_theme.text_muted;
    }
    if (g_settings->get("ColorTheme") == "Rainbow") {
        driver->draw2DRectangle(rainbow_category_color, categoryRects[i]);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X, categoryRects[i].UpperLeftCorner.Y, categoryRects[i].LowerRightCorner.X - 1, categoryRects[i].LowerRightCorner.Y - 1), rainbow_outline_color);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X - 1, categoryRects[i].UpperLeftCorner.Y - 1, categoryRects[i].LowerRightCorner.X, categoryRects[i].LowerRightCorner.Y), rainbow_outline_color);
    } else {
        bhBlurRect(driver, categoryRects[i]);
        driver->draw2DRectangle(current_theme.background, categoryRects[i]);
        bhGlassEdges(driver, categoryRects[i]);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X, categoryRects[i].UpperLeftCorner.Y, categoryRects[i].LowerRightCorner.X - 1, categoryRects[i].LowerRightCorner.Y - 1), current_theme.border);
        driver->draw2DRectangleOutline(core::rect<s32>(categoryRects[i].UpperLeftCorner.X - 1, categoryRects[i].UpperLeftCorner.Y - 1, categoryRects[i].LowerRightCorner.X, categoryRects[i].LowerRightCorner.Y), current_theme.border);
    }

    const std::string& categoryName = script->m_cheat_categories[i]->m_name;
    std::wstring wCategoryName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(categoryName);

    core::dimension2d<u32> textSizeU32 = font->getDimension(wCategoryName.c_str());
    core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

    s32 textX = textRects[i].UpperLeftCorner.X + (textRects[i].getWidth() - textSize.Width) / 2;
    s32 textY = textRects[i].UpperLeftCorner.Y + (textRects[i].getHeight() - textSize.Height) / 2;


    font->draw(wCategoryName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

    core::position2d<s32> dropdown_center(dropdownRects[i].UpperLeftCorner.X + dropdownRects[i].getWidth() / 2 ,dropdownRects[i].UpperLeftCorner.Y + dropdownRects[i].getHeight() / 2);

    if (selectedCategory[i]) {
        
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        ///////////////////////// DRAW CHEATS /////////////////////////
        for (size_t cheat_index = 0; cheat_index < script->m_cheat_categories[i]->m_cheats.size(); ++cheat_index) {

            arrow_color = current_theme.text;
            text_color = current_theme.text;
            if (cheatDropdownHovered[i][cheat_index]) {
                arrow_color = current_theme.text_muted;
            }
            if (cheatTextHovered[i][cheat_index]) {
                text_color = current_theme.text_muted;
            }

            
            if (!script->m_cheat_categories[i]->m_cheats[cheat_index]->is_enabled() && cheatRectAnimationProgress[i][cheat_index] <= 1) {
                cheatRectAnimationProgress[i][cheat_index] += dtime * 8;
            } else if (script->m_cheat_categories[i]->m_cheats[cheat_index]->is_enabled() && cheatRectAnimationProgress[i][cheat_index] >= 0) {
                cheatRectAnimationProgress[i][cheat_index] -= dtime * 8;
            }
            if (g_settings->get("ColorTheme") == "Rainbow") {
                bhBlurRect(driver, cheatRects[i][cheat_index]);
                drawInterpolatedRectangle(driver, cheatRects[i][cheat_index], rainbow_cheat_color, rainbow_cheat_color_enabled, cheatRectAnimationProgress[i][cheat_index]);
            {
                f32 bh_on = 1.0f - std::min(1.0f, std::max(0.0f, cheatRectAnimationProgress[i][cheat_index]));
                const core::rect<s32> &bh_r = cheatRects[i][cheat_index];
                if (bh_on > 0.02f) {
                    s32 a1 = (s32)(bh_on * 70.0f);
                    driver->draw2DRectangle(bh_r,
                        video::SColor(a1, 255, 190, 110), video::SColor(a1, 255, 190, 110),
                        video::SColor(0, 255, 150, 60), video::SColor(0, 255, 150, 60));
                }
                bhGlassEdges(driver, bh_r);
            }
            } else {
                bhBlurRect(driver, cheatRects[i][cheat_index]);
                drawInterpolatedRectangle(driver, cheatRects[i][cheat_index], current_theme.primary, current_theme.background_bottom, cheatRectAnimationProgress[i][cheat_index]);
            {
                f32 bh_on = 1.0f - std::min(1.0f, std::max(0.0f, cheatRectAnimationProgress[i][cheat_index]));
                const core::rect<s32> &bh_r = cheatRects[i][cheat_index];
                if (bh_on > 0.02f) {
                    s32 a1 = (s32)(bh_on * 70.0f);
                    driver->draw2DRectangle(bh_r,
                        video::SColor(a1, 255, 190, 110), video::SColor(a1, 255, 190, 110),
                        video::SColor(0, 255, 150, 60), video::SColor(0, 255, 150, 60));
                }
                bhGlassEdges(driver, bh_r);
            }
            }

            const std::string& cheatName = script->m_cheat_categories[i]->m_cheats[cheat_index]->m_name;
            std::wstring wCheatName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(cheatName);

            textSizeU32 = font->getDimension(wCheatName.c_str());
            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);
            s32 textX = cheatTextRects[i][cheat_index].UpperLeftCorner.X + (cheatTextRects[i][cheat_index].getWidth() - textSize.Width) / 2;
            s32 textY = cheatTextRects[i][cheat_index].UpperLeftCorner.Y + (cheatTextRects[i][cheat_index].getHeight() - textSize.Height) / 2;

            font->draw(wCheatName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
            if (script->m_cheat_categories[i]->m_cheats[cheat_index]->has_settings()) {
                core::position2d<s32> dropdown_center(cheatDropdownRects[i][cheat_index].UpperLeftCorner.X + cheatDropdownRects[i][cheat_index].getWidth() / 2, 
                                                    cheatDropdownRects[i][cheat_index].UpperLeftCorner.Y + cheatDropdownRects[i][cheat_index].getHeight() / 2);

                if (selectedCheat[i][cheat_index]) {
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    ///////////////////////// DRAW CHEAT SETTINGS /////////////////////////

                    core::rect<s32> settingsBox = cheatSettingRects[i][cheat_index][0];
                    for (const auto &sr : cheatSettingRects[i][cheat_index]) {
                        settingsBox.addInternalPoint(sr.UpperLeftCorner);
                        settingsBox.addInternalPoint(sr.LowerRightCorner);
                    }
                    driver->draw2DRectangle(settingBackgroundColor, core::rect<s32>(settingsBox.UpperLeftCorner.X,
                                                                                    settingsBox.UpperLeftCorner.Y, 
                                                                                    settingsBox.LowerRightCorner.X,
                                                                                    settingsBox.LowerRightCorner.Y));
                    if (g_settings->get("ColorTheme") == "Rainbow") {
                        driver->draw2DRectangle(rainbow_cheat_color, core::rect<s32>(settingsBox.UpperLeftCorner.X + setting_bar_padding,
                                                                            settingsBox.UpperLeftCorner.Y + setting_bar_padding, 
                                                                            settingsBox.UpperLeftCorner.X + setting_bar_width + setting_bar_padding,
                                                                            settingsBox.LowerRightCorner.Y - setting_bar_padding));
                    } else {
                        driver->draw2DRectangle(settingBarColor, core::rect<s32>(settingsBox.UpperLeftCorner.X + setting_bar_padding,
                                                                            settingsBox.UpperLeftCorner.Y + setting_bar_padding, 
                                                                            settingsBox.UpperLeftCorner.X + setting_bar_width + setting_bar_padding,
                                                                            settingsBox.LowerRightCorner.Y - setting_bar_padding));
                    }

                    for (size_t s = 0; s < script->m_cheat_categories[i]->m_cheats[cheat_index]->m_cheat_settings.size(); ++s) {
                        ScriptApiCheatsCheatSetting* cheatSetting = script->m_cheat_categories[i]->m_cheats[cheat_index]->m_cheat_settings[s];

                        text_color = current_theme.text;
                        if (cheatSetting->m_type == "selectionbox") {
                            const std::string& settingName = cheatSetting->m_name;
                            std::wstring wSettingName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(settingName);

                            textSizeU32 = font->getDimension(wSettingName.c_str());
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            font->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

                            arrow_color = current_theme.text;
                            if (selectionBoxHovered[i][cheat_index][s]) {
                                arrow_color = current_theme.text_muted;
                            }

                            selectionBoxRects[i][cheat_index][s] = core::rect<s32>( cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                                                    cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height, 
                                                                                    cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - selection_box_padding,
                                                                                    cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y - selection_box_padding);

                            core::rect<s32> textBox = core::rect<s32>(  cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                                        cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height, 
                                                                        cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - category_height,
                                                                        cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y);

                            core::rect<s32> dropdownBox = core::rect<s32>(  cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - category_height,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height, 
                                                                            (cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - selection_box_padding),
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y - selection_box_padding);
                            if (g_settings->get("ColorTheme") == "Rainbow") {
                                driver->draw2DRectangle(rainbow_cheat_color, selectionBoxRects[i][cheat_index][s]);
                                driver->draw2DRectangleOutline(selectionBoxRects[i][cheat_index][s], rainbow_cheat_color);
                                driver->draw2DRectangleOutline(dropdownBox, rainbow_cheat_color);
                            } else {
                                driver->draw2DRectangle(current_theme.background_bottom, selectionBoxRects[i][cheat_index][s]);
                                driver->draw2DRectangleOutline(selectionBoxRects[i][cheat_index][s], settingBarColor);
                                driver->draw2DRectangleOutline(dropdownBox, settingBarColor);
                            }

                            core::position2d<s32> dropdown_center(dropdownBox.UpperLeftCorner.X + dropdownBox.getWidth() / 2 , dropdownBox.UpperLeftCorner.Y + dropdownBox.getHeight() / 2);
                            if (isSelecting && selectingCategoryIndex == i && selectingCheatIndex == cheat_index && selectingSettingIndex == s) {
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                            } else {
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                                driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                            }
                            

                            if (!g_settings->exists(cheatSetting->m_setting)) {
                                std::string defaultValue = *(cheatSetting->m_options[0]);

                                cheatSetting->set_value(defaultValue);
                            }
                            std::wstring wSelectedOption = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(g_settings->get(cheatSetting->m_setting));

                            textSizeU32 = font->getDimension(wSelectedOption.c_str());
                            textSize = core::dimension2d<s32>(textSizeU32.Width, textSizeU32.Height);

                            textX = textBox.UpperLeftCorner.X + (textBox.getWidth() - textSize.Width) / 2;
                            textY = textBox.UpperLeftCorner.Y + (textBox.getHeight() - textSize.Height) / 2;

                            font->draw(wSelectedOption.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), arrow_color);
                        } else if (cheatSetting->m_type == "bool" &&
                                   cheatSetting->m_setting.compare(0, 15, "auto_heal.slot_") == 0) {
                            const bool slot_on = g_settings->exists(cheatSetting->m_setting) &&
                                                 g_settings->getBool(cheatSetting->m_setting);
                            core::rect<s32> cell = cheatSettingRects[i][cheat_index][s];
                            cell.UpperLeftCorner.X += 1;
                            cell.UpperLeftCorner.Y += 1;
                            cell.LowerRightCorner.X -= 1;
                            cell.LowerRightCorner.Y -= 1;

                            video::SColor cell_color = video::SColor(255, 45, 45, 45);
                            if (slot_on) {
                                cell_color = video::SColor(255, 30, 120, 255);
                            }
                            driver->draw2DRectangle(cell_color, cell);
                            driver->draw2DRectangleOutline(cell, video::SColor(255, 100, 100, 100));

                            std::wstring wSlot = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(cheatSetting->m_name);
                            gui::IGUIFont* slotFont = g_fontengine->getFont(g_fontengine->getFontSize(FM_HD) * 0.56, FM_HD);
                            core::dimension2d<u32> slotSize = slotFont->getDimension(wSlot.c_str());
                            s32 slotX = cell.UpperLeftCorner.X + (cell.getWidth() - s32(slotSize.Width)) / 2;
                            s32 slotY = cell.UpperLeftCorner.Y + (cell.getHeight() - s32(slotSize.Height)) / 2;
                            slotFont->draw(wSlot.c_str(),
                                core::rect<s32>(slotX, slotY, slotX + s32(slotSize.Width), slotY + s32(slotSize.Height)),
                                current_theme.text);
                        } else if (cheatSetting->m_type == "bool") {
                            if(g_settings->exists(cheatSetting->m_setting) && g_settings->getBool(cheatSetting->m_setting) == true) {
                                if (g_settings->get("ColorTheme") == "Rainbow") {
                                    text_color = rainbow_cheat_color;
                                } else {
                                    text_color = current_theme.primary;
                                }
                            }
                            if(cheatSettingTextHovered[i][cheat_index][s] == true) {
                                if (g_settings->exists(cheatSetting->m_setting) && g_settings->getBool(cheatSetting->m_setting) == true) {
                                    if (g_settings->get("ColorTheme") == "Rainbow") {
                                        text_color = rainbow_cheat_color;
                                    } else {
                                        text_color = current_theme.primary_muted;
                                    }
                                } else {
                                    text_color = current_theme.text_muted;
                                }
                            }
                            const std::string& settingName = cheatSetting->m_name;
                            std::wstring wSettingName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(settingName);
                            
                            gui::IGUIFont* dropFont = g_fontengine->getFont(g_fontengine->getFontSize(FM_HD) * 0.76, FM_HD);
                            textSizeU32 = dropFont->getDimension(wSettingName.c_str());
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            dropFont->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
                        } else if (cheatSetting->m_type == "slider_int" || cheatSetting->m_type == "slider_float") {
                            std::string settingName = cheatSetting->m_name;

                            if (!g_settings->exists(cheatSetting->m_setting)) {
                                cheatSetting->set_value(roundToNearestStep((cheatSetting->m_min + cheatSetting->m_max) / 2, cheatSetting->m_min, cheatSetting->m_max, cheatSetting->m_steps));
                            }

                            if (cheatSetting->m_type == "slider_int") {
                                std::ostringstream oss;
                                oss << " ( " << round(g_settings->getFloat(cheatSetting->m_setting)) << " )"; 
                                settingName = settingName + oss.str();
                            } else if (cheatSetting->m_type == "slider_float") {
                                std::ostringstream oss;
                                oss.precision(1);
                                oss << std::fixed << g_settings->getFloat(cheatSetting->m_setting);
                                settingName = settingName + "( " + oss.str() + " )";
                            }
                            std::wstring wSettingName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(settingName);
                            textSizeU32 = font->getDimension(wSettingName.c_str());
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            font->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);

                            cheatSliderBarRects[i][cheat_index][s] = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + slider_width_padding,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height + slider_height_padding, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X - slider_width_padding,
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.Y - slider_height_padding);

                            core::rect<s32> filledRect;
                            core::rect<s32> remainingRect;
                            calculateSliderSplit(cheatSliderBarRects[i][cheat_index][s], g_settings->getFloat(cheatSetting->m_setting), cheatSetting->m_min, cheatSetting->m_max, filledRect, remainingRect);
                            if (g_settings->get("ColorTheme") == "Rainbow") {
                                driver->draw2DRectangle(rainbow_cheat_color, filledRect);
                                driver->draw2DRectangle(sliderBarColor, remainingRect);
                            } else {
                                driver->draw2DRectangle(sliderBarColorActive, filledRect);
                                driver->draw2DRectangle(sliderBarColor, remainingRect);
                            }


                            cheatSliderRects[i][cheat_index][s] = core::rect<s32>(filledRect.LowerRightCorner.X - slider_width_padding / 2,
                                                                                filledRect.UpperLeftCorner.Y - slider_height_padding / 1.5,
                                                                                filledRect.LowerRightCorner.X + slider_width_padding / 2,
                                                                                filledRect.LowerRightCorner.Y + slider_height_padding / 1.5);

                            if (cheatSliderHovered[i][cheat_index][s]) {
                                driver->draw2DRectangle(sliderColorActive, cheatSliderRects[i][cheat_index][s]);
                            } else {
                                driver->draw2DRectangle(sliderColor, cheatSliderRects[i][cheat_index][s]);
                            }
                        } else if (cheatSetting->m_type == "text") {
                            std::wstring currentText = cheatSettingTextFields[i][cheat_index][s]->getText();
                            if (currentText != cheatSettingTextLasts[i][cheat_index][s]) {
                                g_settings->set(script->m_cheat_categories[i]->m_cheats[cheat_index]->m_cheat_settings[s]->m_setting, std::wstring_convert<std::codecvt_utf8<wchar_t>>().to_bytes(cheatSettingTextFields[i][cheat_index][s]->getText()));
                            }
                            std::wstring wSettingName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(cheatSetting->m_name);
                            textSizeU32 = font->getDimension(wSettingName.c_str());
                            core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

                            core::rect<s32> settingTextRect = core::rect<s32>(cheatSettingRects[i][cheat_index][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y, 
                                                                            cheatSettingRects[i][cheat_index][s].LowerRightCorner.X,
                                                                            cheatSettingRects[i][cheat_index][s].UpperLeftCorner.Y + category_height);

                            s32 textX = settingTextRect.UpperLeftCorner.X + (settingTextRect.getWidth() - textSize.Width) / 2;
                            s32 textY = settingTextRect.UpperLeftCorner.Y + (settingTextRect.getHeight() - textSize.Height) / 2;

                            font->draw(wSettingName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
                        }
                    }
                } else {
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                    driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
                }
                
            }
        }
    } else {
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X - (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), dropdown_center.Y - (unit_size * 1.5)), arrow_color);
        driver->draw2DLine(core::position2d<s32>(dropdown_center.X, 1 + dropdown_center.Y + (unit_size * 1.5)), core::position2d<s32>(dropdown_center.X + (unit_size * 3), 1 + dropdown_center.Y - (unit_size * 1.5)), arrow_color);
    }
}

void NewMenu::drawSelectionBox(video::IVideoDriver * driver, gui::IGUIFont * font, const size_t i, const size_t c, const size_t s)
{
    GET_SCRIPT_POINTER

    for (size_t o = 0; o < script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size(); o++) {
        cheatSettingOptionRects[i][c][s][o] = core::rect<s32>(cheatSettingRects[i][c][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                            cheatSettingRects[i][c][s].UpperLeftCorner.Y + (category_height - selection_box_padding) + ((o+1) * category_height), 
                                                            (cheatSettingRects[i][c][s].LowerRightCorner.X - selection_box_padding),
                                                            cheatSettingRects[i][c][s].LowerRightCorner.Y + ((o+1) * category_height));

        driver->draw2DRectangle(option_color, cheatSettingOptionRects[i][c][s][o]);
        std::wstring wOptionName = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options[o]->c_str());
        
        core::dimension2d<u32> textSizeU32 = font->getDimension(wOptionName.c_str());
        core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

        s32 textX = cheatSettingOptionRects[i][c][s][o].UpperLeftCorner.X + (cheatSettingOptionRects[i][c][s][o].getWidth() - textSize.Width) / 2;
        s32 textY = cheatSettingOptionRects[i][c][s][o].UpperLeftCorner.Y + (cheatSettingOptionRects[i][c][s][o].getHeight() - textSize.Height) / 2;
        video::SColor option_text_color = current_theme.text;
        if (cheatSettingOptionHovered[i][c][s][o]) {
            option_text_color = current_theme.text_muted;
        }
        font->draw(wOptionName.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), option_text_color);
    }
    core::rect<s32> outlineBox = core::rect<s32>(cheatSettingRects[i][c][s].UpperLeftCorner.X + (setting_bar_padding * 2) + setting_bar_width + selection_box_padding,
                                                cheatSettingRects[i][c][s].UpperLeftCorner.Y + (category_height - selection_box_padding) + category_height, 
                                                (cheatSettingRects[i][c][s].LowerRightCorner.X - selection_box_padding),
                                                cheatSettingRects[i][c][s].LowerRightCorner.Y + (script->m_cheat_categories[i]->m_cheats[c]->m_cheat_settings[s]->m_options.size() * category_height));
    if (g_settings->get("ColorTheme") == "Rainbow") {
        driver->draw2DRectangleOutline(outlineBox, getNextRainbowColor());
    } else {
        driver->draw2DRectangleOutline(outlineBox, settingBarColor);
    }
}

void NewMenu::drawEditHudButton(video::IVideoDriver *driver, gui::IGUIFont *font)
{

    driver->draw2DRectangle(current_theme.background, editHUDbuttonBounds);
    bhGlassEdges(driver, editHUDbuttonBounds);
    driver->draw2DRectangleOutline(editHUDbuttonBounds, current_theme.background_bottom, 2);

    std::wstring editHUDtext = isEditing ? L"Stop Editing" : L"Edit HUD";

    video::SColor text_color = isEditingHovered ? current_theme.text_muted : current_theme.text;

    core::dimension2d<u32> textSizeU32 = font->getDimension(editHUDtext.c_str());
    core::dimension2d<s32> textSize(textSizeU32.Width, textSizeU32.Height);

    s32 textX = editHUDbuttonBounds.UpperLeftCorner.X + (editHUDbuttonBounds.getWidth() - textSize.Width) / 2;
    s32 textY = editHUDbuttonBounds.UpperLeftCorner.Y + (editHUDbuttonBounds.getHeight() - textSize.Height) / 2;


    font->draw(editHUDtext.c_str(), core::rect<s32>(textX, textY, textX + textSize.Width, textY + textSize.Height), text_color);
}

void NewMenu::drawHints(video::IVideoDriver* driver, gui::IGUIFont* font, const size_t i)
{
    GET_SCRIPT_POINTER

    if (selectedCategory[i]) {
        for (size_t cheat_index = 0; cheat_index < script->m_cheat_categories[i]->m_cheats.size(); ++cheat_index) {
            if (cheatTextHovered[i][cheat_index] && g_settings->getBool("use_hints") && !isSelecting) {
                std::wstring wCheatDes = std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(script->m_cheat_categories[i]->m_cheats[cheat_index]->m_description);
                core::dimension2d<u32> textSizeU32_des = font->getDimension(wCheatDes.c_str());
                core::dimension2d<s32> textSize_des(textSizeU32_des.Width, textSizeU32_des.Height);
                const s32 padding = 8;

            core::dimension2d<u32> screenSize = driver->getScreenSize();

            s32 backgroundWidth = textSize_des.Width + 2 * padding;
            s32 backgroundHeight = textSize_des.Height + 2 * padding;

            s32 backgroundX = cheatRects[i][cheat_index].UpperLeftCorner.X + 
                            (cheatRects[i][cheat_index].getWidth() - backgroundWidth) / 2;
            s32 backgroundY = cheatRects[i][cheat_index].LowerRightCorner.Y + padding / 2;

            if (backgroundX < 0)
                backgroundX = 0;
            if (backgroundX + backgroundWidth > (s32)screenSize.Width)
                backgroundX = screenSize.Width - backgroundWidth;

            if (backgroundY + backgroundHeight > (s32)screenSize.Height)
                backgroundY = screenSize.Height - backgroundHeight;

            if (backgroundY < 0)
               backgroundY = 0;

                if (!wCheatDes.empty()) {
                    driver->draw2DRectangle(current_theme.secondary_muted, core::rect<s32>(backgroundX, backgroundY, backgroundX + backgroundWidth, backgroundY + backgroundHeight));
                    bhGlassEdges(driver, core::rect<s32>(backgroundX, backgroundY, backgroundX + backgroundWidth, backgroundY + backgroundHeight));
                    driver->draw2DRectangleOutline(core::rect<s32>(backgroundX, backgroundY, backgroundX + backgroundWidth, backgroundY + backgroundHeight), current_theme.secondary, 2);
                }

                // Text should be inside the padded area
                s32 textXD = backgroundX + padding;
                s32 textYD = backgroundY + padding;

                font->draw(wCheatDes.c_str(), 
                        core::rect<s32>(textXD, textYD, textXD + textSize_des.Width, textYD + textSize_des.Height), 
                        current_theme.text);
            }
        }
    }
}

void NewMenu::draw() 
{
	if (!m_client)
		return;

    if (m_client == nullptr || m_client->stop_running_menu || m_client->isShutdown()) {
        return;
    }

    GET_SCRIPT_POINTER

    float dtime = getDeltaTime();
    // Glass look: let the game show through the menu colours
    current_theme.background.setAlpha(std::min<u32>(current_theme.background.getAlpha(), 150));
    current_theme.background_top.setAlpha(std::min<u32>(current_theme.background_top.getAlpha(), 150));
    current_theme.background_bottom.setAlpha(std::min<u32>(current_theme.background_bottom.getAlpha(), 150));

    video::IVideoDriver* driver = Environment->getVideoDriver();

    const irr::core::dimension2du screensize = driver->getScreenSize();

    gui::IGUIFont* font = g_fontengine->getFont(g_fontengine->getFontSize(FM_HD) * 0.62, FM_HD);

    if (g_settings->exists("use_menu_grid") && g_settings->getBool("use_menu_grid") && (isDragging || isEditing)) {
        for (size_t x = 0; x <= screensize.Width; x += category_height / 2) {
            driver->draw2DLine(core::position2d<s32>(x, 0), core::position2d<s32>(x, screensize.Height), video::SColor(50, 255, 255, 255));
        }

        for (size_t y = 0; y <= screensize.Height; y += category_height / 2) {
            driver->draw2DLine(core::position2d<s32>(0, y), core::position2d<s32>(screensize.Width, y), video::SColor(50, 255, 255, 255));
        }
    }
    
    if (m_is_open)
        bhUpdateBlur(driver);
    if (m_is_open) {
        if (isEditing) {
            for (auto element : hudElements) {
                element->draw(driver, font, dtime, m_client->getEnv(), true);
                video::SColor move_color = element->isHovered ? video::SColor(127, 50, 50, 50) : video::SColor(127, 127, 127, 127);

                driver->draw2DRectangle(move_color, element->bounds);

                video::SColor resize_color = element->isResizeHovered ? video::SColor(255, 127, 127, 127) : video::SColor(255, 200, 200, 200);

                driver->draw2DRectangle(resize_color, element->resizeBounds);
            }
        } else {
            for (size_t i = 0; i < script->m_cheat_categories.size(); i++) {
                if (screensize != lastScreenSize) {
                    moveMenu(i, category_positions[i]);
                }
                drawCategory(driver, font, i, dtime);
            }
            for (size_t i = 0; i < script->m_cheat_categories.size(); i++) {
                drawHints(driver, font, i);
            }
            if (isSelecting) {
                drawSelectionBox(driver, font, selectingCategoryIndex, selectingCheatIndex, selectingSettingIndex);
            }
        }

        drawEditHudButton(driver, font);
    } else {
        isEditing = false;
        for (auto element : hudElements) {
            element->draw(driver, font, dtime, m_client->getEnv(), false);
        }
    }
}
