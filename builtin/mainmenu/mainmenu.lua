-- Luanti - Main Menu Dialog
-- Copyright (C) 2024 siliconsniffer
-- SPDX-License-Identifier: LGPL-2.1-or-later

-- Define defaulttexturedir and dump if they aren't globally accessible.
-- For example:
-- local defaulttexturedir = core.get_current_modpath() .. "/textures/"
-- local dump = minetest.debug.dump -- Or your custom dump function

local BH_FRAMES = 21
local BH_FRAME_MS = 100
local BH_ASPECT = 384 / 216

local function get_screen_aspect()
    local aspect = 2.2
    if core.get_screen_info then
        local ok, info = pcall(core.get_screen_info)
        if ok and type(info) == "table" and info.window_width and info.window_height
                and info.window_width > 0 and info.window_height > 0 then
            aspect = info.window_width / info.window_height
        end
    end
    return math.max(1.0, math.min(aspect, 3.5))
end

local function main_menu_formspec(this)

	if cache_settings:get(LOGIN_USERNAME_SETTING_NAME)  == nil then
		cache_settings:set(LOGIN_USERNAME_SETTING_NAME, "")	
	end

	if this.hidden or (this.parent ~= nil and this.parent.hidden) then
		return ""
	end
    -- The main menu formspec defines the overall size and includes all buttons.
    -- Assuming a fixed size for the main menu dialog for simplicity.
    local height = 15
    local aspect = get_screen_aspect()
    local width = height * aspect

    -- Black hole picture: fill the whole screen, keep its shape.
    local img_w, img_h, img_x, img_y
    if aspect >= BH_ASPECT then
        img_w = width
        img_h = width / BH_ASPECT
        img_x = 0
        img_y = -(img_h - height) / 2
    else
        img_h = height
        img_w = height * BH_ASPECT
        img_x = width - img_w
        img_y = 0
    end
    local bh_image = string.format("animated_image[%f,%f;%f,%f;bh_anim;%s;%d;%d]",
        img_x, img_y, img_w, img_h,
        core.formspec_escape(defaulttexturedir .. "blackhole_anim.png"),
        BH_FRAMES, BH_FRAME_MS)
    local fixed_size = false -- Or true, depending on desired behavior

    -- The 'prepend' content from your original get_formspec forms the core of this dialog.
    local bar_gap = 0.3
    local bar_w = math.min(4, (width - 0.4 - 6 * bar_gap) / 7)
    local bar_x = (width - 0.4 - (7 * bar_w + 6 * bar_gap)) / 2
    local bar_y = 11.9
    local formspec_elements = {
        "formspec_version[8]",
        string.format("size[%f,%f,%s]", width, height, dump(fixed_size)),
        "position[0,0]",
        "anchor[0,0]",
		"padding[0,0]",
        "bgcolor[#0000]",
        "",
        "container[0.2,0.1]",
        -- Local Button
        "style_type[image_button;border=false;textcolor=white;font_size=*1.1;padding=0;font=bold;bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_button.png") .. ";bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_button_hovered.png") .. "]",
        "style[local_btn;bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_button_primary.png") .. ";bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_button_primary_hovered.png") .. ";textcolor=#3a1f00]",
        string.format("image_button[%f,%f;%f,0.85;;local_btn;%s]", bar_x + 0 * (bar_w + bar_gap), bar_y, bar_w, fgettext("Local")),
        -- Online Button
        string.format("image_button[%f,%f;%f,0.85;;online;%s]", bar_x + 1 * (bar_w + bar_gap), bar_y, bar_w, fgettext("Online")),
        -- CSMs Button
        string.format("image_button[%f,%f;%f,0.85;;csm;%s]", bar_x + 2 * (bar_w + bar_gap), bar_y, bar_w, fgettext("CSMs")),
        -- Content Button
        string.format("image_button[%f,%f;%f,0.85;;content;%s]", bar_x + 3 * (bar_w + bar_gap), bar_y, bar_w, fgettext("Content")),
        -- Cosmetics Button
        string.format("image_button[%f,%f;%f,0.85;;cosmetics;%s]", bar_x + 4 * (bar_w + bar_gap), bar_y, bar_w, fgettext("Cosmetics")),
        -- Settings Button
        string.format("image_button[%f,%f;%f,0.85;;settings;%s]", bar_x + 5 * (bar_w + bar_gap), bar_y, bar_w, fgettext("Settings")),
        -- About Button
        string.format("image_button[%f,%f;%f,0.85;;about;%s]", bar_x + 6 * (bar_w + bar_gap), bar_y, bar_w, fgettext("About")),
        -- Header Image
        "image[0,0;7,1.4;" .. core.formspec_escape(defaulttexturedir .. "menu_header.png") .. "]",

		-- Secondary menu ( at the bottom )

		-- Logged in as button
        "style_type[image_button;border=false;textcolor=white;font_size=*0.65;padding=0;font=bold;bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_account.png") .. ";bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_account_hovered.png") .. "]",
        "image_button[0,14.3;3.85,0.425;;logged_in;        " .. cache_settings:get(LOGIN_USERNAME_SETTING_NAME) .. "]",

		-- Exit button
		"style_type[image_button;border=false;textcolor=white;font_size=*0.65;padding=0;font=bold;bgimg=" .. core.formspec_escape(defaulttexturedir .. "menu_exit.png") .. ";bgimg_hovered=" .. core.formspec_escape(defaulttexturedir .. "menu_exit_hovered.png") .. "]",
        "image_button[3.9,14.3;1.9,0.425;;exit;        " .. fgettext("Exit") .. "]",

		-- github button


		-- discord button


		-- website button

        "container_end[]",
    }
	return table.concat(formspec_elements, "")
end

local function main_menu_buttonhandler(this, fields)
	if this.hidden then
		return false
	end
    if fields.local_btn then
        local dlg = create_local_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
        return true
    elseif fields.online then
        local dlg = create_online_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
        return true
    elseif fields.csm then
        local dlg = create_csm_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
        return true
    elseif fields.cosmetics then
        local dlg = create_cosmetics_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
        return true
    elseif fields.content then
        local dlg = create_content_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
        return true
    elseif fields.settings then
        local dlg = create_settings_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
        return true
    elseif fields.about then
        local dlg = create_about_dlg()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
        return true
	elseif fields.exit then
		core.close()
		return true
	elseif fields.logged_in then
		core.settings:set(SESSION_TOKEN_SETTING_NAME, "")
		fetch_capes()
		local dlg = create_sign_in_dialog()
		dlg:set_parent(this)
		this:hide()
		dlg:show()
		ui.update()
		return true
	elseif fields.github then
		core.open_url_dialog("https://github.com/TeamAcedia/CloakV4")
		return true
	elseif fields.discord then
		core.open_url_dialog("https://discord.gg/z2bW5h6PXQ")
		return true
	elseif fields.website then
		core.open_url_dialog("https://cloak-v4.web.app")
		return true
	elseif fields.try_quit then
		return true
    end
	
    core.log("error", "Main Menu: "..dump(fields))
    return false
end

local function main_menu_eventhandler(this, event)
	if this.hidden then
		return false
	end

	if event == "MenuQuit" then
		core.close()
	end
end

local function hide_menu(this)
	this.hidden=true
end

--------------------------------------------------------------------------------
local function show_menu(this)
	this.hidden=false
end

local tabview_metatable = {
	handle_buttons            = main_menu_buttonhandler,
	handle_events             = main_menu_eventhandler,
	get_formspec              = main_menu_formspec,
	show                      = show_menu,
	hide                      = hide_menu,
	delete                    = function(self) ui.delete(self) end,
	set_parent                = function(self,parent) self.parent = parent end,
	set_fixed_size			  = function(self,state) self.fixed_size = state end,
}

function create_main_menu(name)--, size, tabheaderpos)
	local self = {}

	self.name     = name
	self.type     = "toplevel"

	setmetatable(self, { __index = tabview_metatable })

	self.fixed_size     = true
	self.hidden         = true

	ui.add(self)
	return self
end