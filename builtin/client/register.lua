local builtin_shared = ...

local make_registration = builtin_shared.make_registration

core.registered_globalsteps, core.register_globalstep = make_registration()
core.registered_on_mods_loaded, core.register_on_mods_loaded = make_registration()
core.registered_on_shutdown, core.register_on_shutdown = make_registration()
core.registered_on_receiving_chat_message, core.register_on_receiving_chat_message = make_registration()
core.registered_on_sending_chat_message, core.register_on_sending_chat_message = make_registration()
core.registered_on_chatcommand, core.register_on_chatcommand = make_registration()
core.registered_on_death, core.register_on_death = make_registration()
core.registered_on_hp_modification, core.register_on_hp_modification = make_registration()
core.registered_on_damage_taken, core.register_on_damage_taken = make_registration()
core.registered_on_formspec_input, core.register_on_formspec_input = make_registration()
core.registered_on_receiving_formspec, core.register_on_receiving_formspec = make_registration()
core.registered_on_dignode, core.register_on_dignode = make_registration()
core.registered_on_punchnode, core.register_on_punchnode = make_registration()
core.registered_on_placenode, core.register_on_placenode = make_registration()
core.registered_on_item_use, core.register_on_item_use = make_registration()
core.registered_on_modchannel_message, core.register_on_modchannel_message = make_registration()
core.registered_on_modchannel_signal, core.register_on_modchannel_signal = make_registration()
core.registered_on_inventory_open, core.register_on_inventory_open = make_registration()
core.registered_on_recieve_physics_override, core.register_on_recieve_physics_override = make_registration()
core.registered_on_object_properties_change, core.register_on_object_properties_change = make_registration()
core.registered_on_object_hp_change, core.register_on_object_hp_change = make_registration()
core.registered_on_object_add, core.register_on_object_add = make_registration()

core.registered_nodes = {}
core.registered_items = {}
core.object_refs = {}

-- Adds an extra "Input" slot to mcl_chests formspecs.
core.register_on_receiving_formspec(function(formname, formspec)
    core.log("action", "[RFS] formname=" .. tostring(formname) .. " len=" .. #formspec)
    if formname:find("^mcl_chests:") then
        for l in formspec:gmatch("list%[[^%]]*%]") do
            core.log("action", "[RFS] " .. l)
        end
    end
    if not (ws and ws.get_itemslot_bg_v4) then
        return nil
    end
    local x, y, z = formname:match("^mcl_chests:chest_(%-?%d+)_(%-?%d+)_(%-?%d+)$")
    if not x then
        return nil
    end
    local loc = "nodemeta:" .. x .. "," .. y .. "," .. z

    local px, py = 3, 0
    local inject = ws.get_itemslot_bg_v4(px, py, 1, 1) ..
    "list[" .. loc .. ";input;"..px..","..py..";1,1;]"..
    "label["..px..","..py..";Input]"
    return formspec .. inject
end)
