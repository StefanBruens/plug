/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2019  offa
 * Copyright (C) 2010-2016  piorekf <piorek@piorekf.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "data_structs.h"
#include "effects_enum.h"
#include "com/MustangConstants.h"
#include "com/Packet.h"
#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace plug::com
{
    std::string decodeNameFromData(const std::array<Packet, 7>& data);
    amp_settings decodeAmpFromData(const std::array<Packet, 7>& data);

    std::array<fx_pedal_settings, 4> decodeEffectsFromData(const std::array<Packet, 7>& data);
    std::vector<std::string> decodePresetListFromData(const std::vector<Packet>& data);

    Packet serializeAmpSettings(const amp_settings& value);
    Packet serializeAmpSettingsUsbGain(const amp_settings& value);
    Packet serializeName(std::uint8_t slot, std::string_view name);
    Packet serializeEffectSettings(const fx_pedal_settings& value);
    Packet serializeClearEffectSettings();
    Packet serializeSaveEffectName(std::uint8_t slot, std::string_view name, const std::vector<fx_pedal_settings>& effects);
    std::vector<Packet> serializeSaveEffectPacket(std::uint8_t slot, const std::vector<fx_pedal_settings>& effects);

    Packet serializeLoadSlotCommand(std::uint8_t slot);
    Packet serializeLoadCommand();
    Packet serializeApplyCommand();
    Packet serializeApplyCommand(fx_pedal_settings effect);

    std::array<Packet, 2> serializeInitCommand();

}
