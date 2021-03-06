#include "../../command.hpp"
#include "../../../signature/hook.hpp"
#include "../../../signature/signature.hpp"
#include "../../../chimera.hpp"
#include "../../../output/output.hpp"
#include "../../../halo_data/player.hpp"
#include "../../../halo_data/object.hpp"
#include "../../../halo_data/map.hpp"
#include "../../../halo_data/tag.hpp"
#include "../../../halo_data/controls.hpp"
#include "../../../event/frame.hpp"
#include "../../../event/tick.hpp"
#include "../../../halo_data/game_engine.hpp"
#include "../../../localization/localization.hpp"
#include <optional>

namespace Chimera {
    static std::vector<std::uint16_t> blocked_ids;

    static std::byte *get_player_data() noexcept {
        static std::optional<std::byte *> player_data;
        if(!player_data.has_value()) {
            player_data = **reinterpret_cast<std::byte ***>(get_chimera().get_signature("spectate_fp_camera_position_sig").data() + 2);
        }
        return *player_data;
    }

    static bool is_blocked(const TagID &id) {
        for(auto &bid : blocked_ids) {
            if(bid == id.index.index) {
                return true;
            }
        }
        return false;
    }

    static std::size_t get_weapon_count(const UnitDynamicObject *o) {
        std::size_t rv = 0;
        for(auto &w : o->weapons) {
            if(w.is_null()) {
                continue;
            }
            auto *wo = ObjectTable::get_object_table().get_dynamic_object(w);
            if(!wo || is_blocked(wo->tag_id)) {
                continue;
            }
            rv++;
        }
        return rv;
    }

    static void on_pretick() {
        if(get_tick_count() == 0 || get_tick_count() == 1) {
            remove_pretick_event(on_pretick);
            blocked_ids.clear();
            return;
        }

        auto *player = PlayerTable::get_player_table().get_client_player();
        if(!player) {
            return;
        }
        auto *unit = reinterpret_cast<UnitDynamicObject *>(ObjectTable::get_object_table().get_dynamic_object(player->object_id));
        if(!unit) {
            return;
        }

        // Cycle through until we have a weapon we didn't block (or we cycled twice)
        bool looped_once = false;
        bool blocked_once = false;
        std::uint8_t slot = unit->weapon_slot;

        while(true) {
            // If we're >= 4, stop if we didn't check a weapon previously, or loop over if we did (and stop if we already looped over)
            if(slot >= 4) {
                if(!blocked_once || looped_once) {
                    return;
                }
                looped_once = true;
                slot = 0;
            }

            // If the weapon is invalid and we haven't yet checked a weapon, stop
            if(unit->weapons[slot].is_null()) {
                if(!blocked_once) {
                    return;
                }
            }
            else {
                auto *weapon = ObjectTable::get_object_table().get_dynamic_object(unit->weapons[slot]);
                if(!weapon) {
                    if(!blocked_once) {
                        return;
                    }
                }
                else {
                    if(is_blocked(weapon->tag_id)) {
                        blocked_once = true;
                    }
                    else if(!blocked_once) {
                        return;
                    }
                    else {
                        *reinterpret_cast<std::uint8_t *>(get_player_data() + 0x30) = slot; // set to new slot
                        return;
                    }
                }
            }

            slot++;
        }
    }

    bool block_extra_weapon_command(int, const char **) {
        auto *player = PlayerTable::get_player_table().get_client_player();
        if(!player) {
            console_error(localize("chimera_block_extra_weapon_error_cannot_get_object"));
            return false;
        }

        auto *unit = reinterpret_cast<UnitDynamicObject *>(ObjectTable::get_object_table().get_dynamic_object(player->object_id));
        if(!unit) {
            console_error(localize("chimera_block_extra_weapon_error_cannot_get_object"));
            return false;
        }

        if(get_weapon_count(unit) <= 2) {
            console_error(localize("chimera_block_extra_weapon_error_not_enough_weapons"));
            return false;
        }

        if(unit->weapon_slot >= 4 || unit->weapons[unit->weapon_slot].is_null()) {
            console_error(localize("chimera_block_extra_weapon_error_cannot_get_weapon"));
            return false;
        }

        auto *weapon = ObjectTable::get_object_table().get_dynamic_object(unit->weapons[unit->weapon_slot]);
        if(!weapon) {
            console_error(localize("chimera_block_extra_weapon_error_cannot_get_weapon"));
            return false;
        }

        if(is_blocked(weapon->tag_id)) {
            console_error(localize("chimera_block_extra_weapon_error_weapon_already_blocked"));
            return false;
        }

        blocked_ids.emplace_back(weapon->tag_id.index.index);
        console_output("Blocked %s", get_tag(weapon->tag_id)->path);
        add_pretick_event(on_pretick);

        return true;
    }

    bool unblock_all_extra_weapons_command(int, const char **) {
        if(blocked_ids.size()) {
            console_output(localize("chimera_unblock_all_extra_weapons_success"));
            blocked_ids.clear();
            remove_pretick_event(on_pretick);
            return true;
        }
        else {
            console_error(localize("chimera_unblock_all_extra_weapons_fail"));
            return false;
        }
    }
}
