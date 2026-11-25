local F = core.formspec_escape
local S = core.get_translator("__builtin")

function core.show_death_screen(player, _reason)
	local fs = {
		"formspec_version[4]",
        "size[11,5.5,true]",
        "bgcolor[#320000b4;true]",
        "animated_image[4.1,0.15;2.8,2.8;bh_anim;death_blackhole_strip.png;30;67;1]",
        "image[2.5,3.0;6,0.8;death_title.png]",
        "style[btn_respawn;border=false;bgimg=death_btn.png;bgimg_hovered=death_btn_hover.png;bgimg_pressed=death_btn_pressed.png]",
        "button_exit[3.7,4.1;3.6,0.9;btn_respawn;]",
    }
	core.show_formspec(player:get_player_name(), "__builtin:death", table.concat(fs, ""))
end

core.register_on_dieplayer(function(player, reason)
if core.settings:get_bool("autorespawn") then
	player:respawn()
else
	core.show_death_screen(player, reason)
end
end)

core.register_on_joinplayer(function(player)
	if player:get_hp() == 0 then
		core.show_death_screen(player, nil)
	end
end)

core.register_on_player_receive_fields(function(player, formname, fields)
	if formname == "__builtin:death" and fields.quit and player:get_hp() == 0 then
		player:respawn()
		core.log("action", player:get_player_name() .. " respawns at " ..
				player:get_pos():to_string())
	end
end)
