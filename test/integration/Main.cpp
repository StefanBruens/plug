/*
 * PLUG - software to operate Fender Mustang amplifier
 *        Linux replacement for Fender FUSE software
 *
 * Copyright (C) 2017-2021  offa
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

#include "com/Mustang.h"
#include "com/ConnectionFactory.h"
#include "com/UsbContext.h"
#include "Version.h"
#include <algorithm>
#include <exception>
#include <iostream>

namespace
{
    void printData(const plug::com::InitalData& data)
    {
        const auto [chain, presets] = data;

        std::cout << "-- Selected : " << chain.name() << "\n";

        std::cout << "-- Presets (" << presets.size() << ")\n";
        std::for_each(presets.cbegin(), presets.cend(), [](const auto& p)
                      { std::cout << "    - " << p << "\n"; });
        std::cout << "---\n\n";
    }
}

int main()
{
    using namespace plug::com;

    std::cout << " === Plug v" << plug::version() << " ===\n\n";

    try
    {
        usb::Context ctx;

        std::cout << " * Setup Mustang\n";
        auto mustang = std::make_unique<plug::com::Mustang>(plug::com::createUsbConnection());

        std::cout << " * Start amp / receive amp data\n";
        const auto data = mustang->start_amp();
        printData(data);

        std::cout << " * Stop amp\n";
        mustang->stop_amp();

        std::cout << " * Closed\n";
    }
    catch (const std::exception& ex)
    {
        std::cout << "ERROR: " << ex.what() << "\n";
        return 1;
    }
    return 0;
}
