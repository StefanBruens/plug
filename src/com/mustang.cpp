/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2018  offa
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

#include "com/mustang.h"
#include "com/UsbComm.h"
#include "com/IdLookup.h"
#include "com/PacketSerializer.h"
#include <cstring>

namespace plug::com
{
    namespace
    {
        inline constexpr std::initializer_list<std::uint16_t> pids{
            SMALL_AMPS_USB_PID,
            BIG_AMPS_USB_PID,
            SMALL_AMPS_V2_USB_PID,
            BIG_AMPS_V2_USB_PID,
            MINI_USB_PID,
            FLOOR_USB_PID};


        inline constexpr std::uint8_t endpointSend{0x01};
        inline constexpr std::uint8_t endpointRecv{0x81};
    }

    Mustang::Mustang()
        : comm(std::make_unique<UsbComm>())
    {
        applyCommand.fill(0x00);
        applyCommand[0] = 0x1c;
        applyCommand[1] = 0x03;

        memset(prev_array, 0x00, packetSize * 4);
        for (int i = 0; i < 4; ++i)
        {
            prev_array[i][0] = 0x1c;
            prev_array[i][1] = 0x03;
            prev_array[i][6] = prev_array[i][7] = prev_array[i][21] = 0x01;
            prev_array[i][20] = 0x08;
            prev_array[i][FXSLOT] = 0xff;
        }
    }

    Mustang::~Mustang()
    {
        this->stop_amp();
    }

    void Mustang::start_amp(char list[][32], char* name, amp_settings* amp_set, fx_pedal_settings* effects_set)
    {
        Packet array{};

        if (comm->isOpen() == false)
        {
            comm->openFirst(USB_VID, pids);
        }

        // initialization which is needed if you want
        // to get any replies from the amp in the future
        array.fill(0x00);
        array[1] = 0xc3;
        sendPacket(array);
        receivePacket();

        array.fill(0x00);
        array[0] = 0x1a;
        array[1] = 0x03;
        sendPacket(array);
        receivePacket();

        loadInitialData(list, name, amp_set, effects_set);
    }

    void Mustang::stop_amp()
    {
        comm->close();
    }

    void Mustang::set_effect(fx_pedal_settings value)
    {
        Packet array{{0x1c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x08, 0x01, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};

        // clear effect on previous DSP before setting a new one
        for (int i = 0; i < 4; ++i)
        {
            if (prev_array[i][FXSLOT] == value.fx_slot || prev_array[i][FXSLOT] == (value.fx_slot + 4))
            {
                memcpy(&array[0], prev_array[i], packetSize);
                prev_array[i][FXSLOT] = 0xff;
                break;
            }
        }
        array[EFFECT] = 0x00;
        array[KNOB1] = 0x00;
        array[KNOB2] = 0x00;
        array[KNOB3] = 0x00;
        array[KNOB4] = 0x00;
        array[KNOB5] = 0x00;
        array[KNOB6] = 0x00;

        sendPacket(array);
        receivePacket();
        sendPacket(applyCommand);
        receivePacket();

        const auto effectType = static_cast<effects>(value.effect_num);

        if (effectType == effects::EMPTY)
        {
            return;
        }

        // send packet to the amp
        const auto settingsPacket = serializeEffectSettings(value);
        sendPacket(settingsPacket);
        receivePacket();
        sendPacket(applyCommand);
        receivePacket();

        // save current settings
        memcpy(prev_array[settingsPacket[DSP] - 6], settingsPacket.data(), packetSize);
    }

    void Mustang::set_amplifier(amp_settings value)
    {
        const auto settingsPacket = serializeAmpSettings(value);
        sendPacket(settingsPacket);
        receivePacket();
        sendPacket(applyCommand);
        receivePacket();

        const auto settingsGainPacket = serializeAmpSettingsUsbGain(value);
        sendPacket(settingsGainPacket);
        receivePacket();
        sendPacket(applyCommand);
        receivePacket();
    }

    void Mustang::save_on_amp(std::string_view name, std::uint8_t slot)
    {
        const auto data = serializeName(slot, name);
        sendPacket(data);
        receivePacket();
        load_memory_bank(slot, nullptr, nullptr, nullptr);
    }

    void Mustang::load_memory_bank(int slot, char* name, amp_settings* amp_set, fx_pedal_settings* effects_set)
    {
        Packet array{};
        unsigned char data[7][packetSize];

        array.fill(0x00);
        array[0] = 0x1c;
        array[1] = 0x01;
        array[2] = 0x01;
        array[SAVE_SLOT] = slot;
        array[6] = 0x01;

        auto n = sendPacket(array);

        for (int i = 0; n != 0; ++i)
        {
            const auto recvData = receivePacket();
            n = recvData.size();

            if (i < 7)
            {
                std::copy(recvData.cbegin(), recvData.cend(), data[i]);
            }
        }

        if (name != nullptr || amp_set != nullptr || effects_set != nullptr)
        {
            decode_data(data, name, amp_set, effects_set);
        }
    }

    void Mustang::decode_data(unsigned char data[7][packetSize], char* name, amp_settings* amp_set, fx_pedal_settings* effects_set)
    {
        if (name != nullptr)
        {
            const std::string nameDecoded = decodeNameFromData(data);
            std::copy(nameDecoded.cbegin(), nameDecoded.cend(), name);
        }

        if (amp_set != nullptr)
        {
            *amp_set = decodeAmpFromData(data);
        }

        if (effects_set != nullptr)
        {
            decodeEffectsFromData(prev_array, data, effects_set);
        }
    }

    void Mustang::save_effects(int slot, std::string_view name, const std::vector<fx_pedal_settings>& effects)
    {
        unsigned char fxknob;
        std::size_t repeat{0};
        Packet array{{0x1c, 0x01, 0x04, 0x00, 0x00, 0x00, 0x01, 0x01,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
        applyCommand[2] = 0x00; // why this must be here?

        if (effects.size() > 2)
        {
            repeat = 1;
        }
        else
        {
            repeat = effects.size();
        }

        for (std::size_t i = 0; i < repeat; ++i)
        {
            if (effects[i].effect_num < value(effects::SINE_CHORUS))
            {
                throw std::invalid_argument{"Invalid effect"};
            }
        }

        if (effects[0].effect_num >= value(effects::SINE_CHORUS) && effects[0].effect_num <= value(effects::PITCH_SHIFTER))
        {
            fxknob = 0x01;
            repeat = 1; //just to be sure
        }
        else
        {
            fxknob = 0x02;
        }

        array[FXKNOB] = fxknob;
        array[SAVE_SLOT] = slot;

        // set and send the name
        constexpr std::size_t nameLength{22};
        std::string sizedName{name};
        sizedName.resize(nameLength, '\0');
        std::copy(sizedName.cbegin(), std::next(sizedName.cend()), std::next(array.begin(), 16));

        sendPacket(array);
        receivePacket();

        array[1] = 0x03;
        array[6] = 0x00;
        std::fill(std::next(array.begin(), 16), std::prev(array.end(), 16), 0x00);

        for (std::size_t i = 0; i < repeat; ++i)
        {
            array[19] = 0x00;
            array[20] = 0x08;
            array[21] = 0x01;
            array[KNOB6] = 0x00;

            if (effects[i].put_post_amp)
            {
                array[FXSLOT] = effects[i].fx_slot + 4;
            }
            else
            {
                array[FXSLOT] = effects[i].fx_slot;
            }
            array[KNOB1] = effects[i].knob1;
            array[KNOB2] = effects[i].knob2;
            array[KNOB3] = effects[i].knob3;
            array[KNOB4] = effects[i].knob4;
            array[KNOB5] = effects[i].knob5;

            const auto effect = static_cast<plug::effects>(effects[i].effect_num);
            // some effects have more knobs
            if (hasExtraKnob(effect) == true)
            {
                array[KNOB6] = effects[i].knob6;
            }

            // fill the form with missing data
            switch (effect)
            {
                case effects::SINE_CHORUS:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x12;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::TRIANGLE_CHORUS:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x13;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::SINE_FLANGER:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x18;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::TRIANGLE_FLANGER:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x19;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::VIBRATONE:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x2d;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::VINTAGE_TREMOLO:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x40;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::SINE_TREMOLO:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x41;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::RING_MODULATOR:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x22;
                    array[19] = 0x01;
                    if (array[KNOB4] > 0x01)
                    {
                        array[KNOB4] = 0x01;
                    }
                    break;

                case effects::STEP_FILTER:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x29;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    break;

                case effects::PHASER:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x4f;
                    array[19] = 0x01;
                    array[20] = 0x01;
                    if (array[KNOB5] > 0x01)
                    {
                        array[KNOB5] = 0x01;
                    }
                    break;

                case effects::PITCH_SHIFTER:
                    array[DSP] = 0x07;
                    array[EFFECT] = 0x1f;
                    array[19] = 0x01;
                    break;

                case effects::MONO_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x16;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::MONO_ECHO_FILTER:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x43;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::STEREO_ECHO_FILTER:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x48;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::MULTITAP_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x44;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::PING_PONG_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x45;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::DUCKING_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x15;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::REVERSE_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x46;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::TAPE_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x2b;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::STEREO_TAPE_DELAY:
                    array[DSP] = 0x08;
                    array[EFFECT] = 0x2a;
                    array[19] = 0x02;
                    array[20] = 0x01;
                    break;

                case effects::SMALL_HALL_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x24;
                    break;

                case effects::LARGE_HALL_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x3a;
                    break;

                case effects::SMALL_ROOM_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x26;
                    break;

                case effects::LARGE_ROOM_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x3b;
                    break;

                case effects::SMALL_PLATE_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x4e;
                    break;

                case effects::LARGE_PLATE_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x4b;
                    break;

                case effects::AMBIENT_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x4c;
                    break;

                case effects::ARENA_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x4d;
                    break;

                case effects::FENDER_63_SPRING_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x21;
                    break;

                case effects::FENDER_65_SPRING_REVERB:
                    array[DSP] = 0x09;
                    array[EFFECT] = 0x0b;
                    break;

                default:
                    break;
            }
            // send packet
            sendPacket(array);
            receivePacket();
        }

        applyCommand[FXKNOB] = fxknob;
        sendPacket(applyCommand);
        receivePacket();
        applyCommand[FXKNOB] = 0x00;
    }

    void Mustang::loadInitialData(char list[][32], char* name, amp_settings* amp_set, fx_pedal_settings* effects_set)
    {
        if (list != nullptr || name != nullptr || amp_set != nullptr || effects_set != nullptr)
        {
            unsigned char recieved_data[296][packetSize];
            memset(recieved_data, 0x00, 296 * packetSize);

            std::size_t i{0};
            std::size_t j{0};
            Packet array{};
            array.fill(0x00);
            array[0] = 0xff;
            array[1] = 0xc1;
            auto recieved = sendPacket(array);

            for (i = 0; recieved != 0; i++)
            {
                const auto recvData = receivePacket();
                recieved = recvData.size();
                std::copy(recvData.cbegin(), recvData.cend(), recieved_data[i]);
            }

            const std::size_t max_to_receive = (i > 143 ? 200 : 48);
            if (list != nullptr)
            {
                for (i = 0, j = 0; i < max_to_receive; i += 2, ++j)
                {
                    memcpy(list[j], recieved_data[i] + 16, 32);
                }
            }

            if (name != nullptr || amp_set != nullptr || effects_set != nullptr)
            {
                unsigned char data[7][packetSize];

                for (j = 0; j < 7; ++i, ++j)
                {
                    memcpy(data[j], recieved_data[i], packetSize);
                }
                decode_data(data, name, amp_set, effects_set);
            }
        }
    }

    std::size_t Mustang::sendPacket(const Packet& packet)
    {
        return comm->interruptWrite(endpointSend, packet);
    }

    std::vector<std::uint8_t> Mustang::receivePacket()
    {
        return comm->interruptReceive(endpointRecv, packetSize);
    }
}
