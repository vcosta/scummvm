/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

namespace Dgds {

struct DgdsGameDescription {
	ADGameDescription desc;
};

static const DgdsGameDescription gameDescriptions[] = {
	// Rise of the Dragon (PC)
	{
		{
			"rise",
			0,
			{
				{"volume.vga", 0, "b0583c199614ed1c161a25398c5c7fba", 7823},
				{"volume.001", 0, "3483f61b9bf0023c00a7fc1b568a54fa", 769811},
				{"dragon.exe", 0, "f0123506e223d67ff78fd1b419f6a67c", 102094},
				AD_LISTEND
			},
			Common::EN_ANY,
			Common::kPlatformDOS,
			ADGF_NO_FLAGS,
			GUIO1(GUIO_NONE)
		},
	},

	// Heart of China (PC)
	{
		{
			"china",
			0,
			{
				{"volume.rmf", 0, "677b91bc6961824f1997c187292f174e", 9791},
				{"volume.001", 0, "3efe89a72940e85d2137162609b8b883", 851843},
				{"hoc.exe", 0, "0300c8787c8e37c62ce80b94cd658c26", 288832},
				AD_LISTEND
			},
			Common::EN_ANY,
			Common::kPlatformDOS,
			ADGF_NO_FLAGS,
			GUIO1(GUIO_NONE)
		},
	},

	{ AD_TABLE_END_MARKER }
};

} // End of namespace Dgds

