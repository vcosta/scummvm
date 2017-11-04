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

#include "common/debug.h"
#include "common/debug-channels.h"
#include "common/file.h"

#include "engines/util.h"

#include "dgds/dgds.h"

namespace Dgds {

    typedef unsigned uint32;
    typedef int int32;
    typedef unsigned short uint16;
    
DgdsEngine::DgdsEngine(OSystem *syst, const DgdsGameDescription *gameDesc)
 : Engine(syst) {
	_console = new DgdsConsole(this);
}

DgdsEngine::~DgdsEngine() {
	DebugMan.clearAllDebugChannels();

	delete _console;
}

Common::Error DgdsEngine::run() {
	initGraphics(320, 200);

	debug("DgdsEngine::init");

	Common::File f, f2;

	if (f.open("volume.vga")) {
		uint32 version;
		uint16 nvolumes;

		debug("DgdsEngine::VGA");

		version = f.readUint32LE();
		nvolumes = f.readUint16LE();

		debug("%u %u", version, nvolumes);

		for (int i=0; i<nvolumes; i++) {
			char name[13];
			uint16 nfiles;

			f.read(name, sizeof(name));
			name[12] = '\0';
			
			nfiles = f.readUint16LE();
			
			f2.open(name);

			debug("--\n%s %u", name, nfiles);
			for (int j=0; j<nfiles; j++) {
				uint32 hash, offset;
				uint32 size;

				hash = f.readUint32LE();
				offset = f.readUint32LE();
				debug("  %u %u", hash, offset);
				
				f2.seek(offset);
				f2.read(name, sizeof(name));
				name[12] = '\0';
				size = f2.readUint32LE();
				debug("  %s %u\n  --", name, size);
				
				const char *ext;
				ext = name + strlen(name) - 3;
				
				
			}
			f2.close();
		}
	}


	return Common::kNoError;
}

} // End of namespace Dgds

