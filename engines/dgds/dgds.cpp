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
typedef unsigned char uint8;

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
				int32 inSize, outSize;

				hash = f.readUint32LE();
				offset = f.readUint32LE();
				debug("  %u %u", hash, offset);
				
				f2.seek(offset);
				f2.read(name, sizeof(name));
				name[12] = '\0';
				inSize = f2.readSint32LE();
				debug("  %s %d\n  --", name, inSize);
				
				if (inSize == -1) {
					continue;
				}

				const char *ext;
				bool chunky, packed;
				int32 k;

				ext = name + strlen(name) - 3;

				k = 0;
				outSize = 0;
				chunky = true;
				while (k < inSize) {
					char type[4+1];
					int32 chunkSize;
					uint8 marker;
					
					f2.read(type, 4);
					type[4] = '\0';
					
					if (type[3] != ':') {
						chunky = false;
						outSize = inSize;
						debug("    binary");
						break;
					}

					k += 4;
					outSize += 4;

					chunkSize = f2.readSint32LE();
					if (chunkSize < 0) {
						chunkSize = -chunkSize;
						debug("    MULTI");
					}
					marker = (chunkSize & 0xff);
					chunkSize >>= 8;

					k += 4;
					outSize += 4;
					
					packed = false;

					if (0) {
					}
					else if (strcmp(ext, "ADS") == 0) {
						if (0) {
						} else if (strcmp(type, "SCR:") == 0) packed = true;
					}
					else if (strcmp(ext, "BMP") == 0) {
						if (0) {
						} else if (strcmp(type, "BIN:") == 0) packed = true;
						else if (strcmp(type, "VGA:") == 0) packed = true;
					}
					else if (strcmp(ext, "DDS") == 0) {
						if (strcmp(type, "DDS:") == 0) packed = true;
					}
					else if (strcmp(ext, "OVL") == 0) {
						if (0) {
						} else if (strcmp(type, "ADL:") == 0) packed = true;
						else if (strcmp(type, "ADS:") == 0) packed = true;
						else if (strcmp(type, "APA:") == 0) packed = true;
						else if (strcmp(type, "ASB:") == 0) packed = true;
						else if (strcmp(type, "GMD:") == 0) packed = true;
						else if (strcmp(type, "M32:") == 0) packed = true;
						else if (strcmp(type, "NLD:") == 0) packed = true;
						else if (strcmp(type, "PRO:") == 0) packed = true;
						else if (strcmp(type, "PS1:") == 0) packed = true;
						else if (strcmp(type, "SBL:") == 0) packed = true;
						else if (strcmp(type, "SBP:") == 0) packed = true;
						else if (strcmp(type, "STD:") == 0) packed = true;
						else if (strcmp(type, "TAN:") == 0) packed = true;
						else if (strcmp(type, "001:") == 0) packed = true;
						else if (strcmp(type, "003:") == 0) packed = true;
						else if (strcmp(type, "004:") == 0) packed = true;
						else if (strcmp(type, "101:") == 0) packed = true;

						else if (strcmp(type, "VGA:") == 0) packed = true;
					}
					else if (strcmp(ext, "SDS") == 0) {
						if (strcmp(type, "SDS:") == 0) packed = true;
					}
					else if (strcmp(ext, "SNG") == 0) {
						if (strcmp(type, "SNG:") == 0) packed = true;
					}
					else if( strcmp( ext, "TDS" ) == 0 ) {
						if( strcmp( type, "TDS:" ) == 0 ) packed = true;
					}

					else if( strcmp( ext, "TTM" ) == 0 ) {
						if( strcmp( type, "TT3:" ) == 0 ) packed = true;
					}

					if (packed) {
						uint8 method;
						int32 unpackSize;
						method = f2.readByte();
						unpackSize = f2.readSint32LE();
						chunkSize -= (1 + 4);
						
						k += (1 + 4 + chunkSize);
						outSize += (1 + 4 + unpackSize);
					} else if (marker == 0x80) {
						chunkSize = 0;
					} else {
						k += chunkSize;
						outSize += chunkSize;
					}
					
					debug("    %s %d %u", type, chunkSize, marker);
					f2.seek(chunkSize, SEEK_CUR);
				}
				debug("  [%u] --", outSize);

				f2.seek(offset + 13 + 4);
			}
			f2.close();
		}
	}


	return Common::kNoError;
}

} // End of namespace Dgds

