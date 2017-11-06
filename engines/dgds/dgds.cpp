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
#include "common/endian.h"
#include "common/events.h"
#include "common/file.h"
#include "common/memstream.h"
#include "common/system.h"

#include "graphics/palette.h"

#include "engines/util.h"

#include "dgds/decompress.h"

#include "dgds/dgds.h"

namespace Dgds {
	typedef unsigned uint32;
	typedef unsigned short uint16;
	typedef unsigned char uint8;
	typedef unsigned char byte;

byte palette[256*3];
byte binData[320000];
byte vgaData[320000];
byte *imgData;

DgdsEngine::DgdsEngine(OSystem *syst, const DgdsGameDescription *gameDesc)
 : Engine(syst) {
	_console = new DgdsConsole(this);
}

DgdsEngine::~DgdsEngine() {
	DebugMan.clearAllDebugChannels();

	delete _console;
}


#define DGDS_FILENAME_MAX 12
#define DGDS_TYPENAME_MAX 4

struct DgdsFileCtx {
	uint32 bytesRead, inSize, outSize;
	
	void init(uint32 size);
};

void DgdsFileCtx::init(uint32 size) {
	inSize = size;
	bytesRead = 0;
	outSize = 0;
}

struct DgdsChunk {
	char type[DGDS_TYPENAME_MAX+1];
	uint32 chunkSize;
	bool container;
	
	bool readHeader(DgdsFileCtx& ctx, Common::File& archive);
	bool isSection(const Common::String& section);

	bool isPacked(const Common::String& ext);	
	Common::SeekableReadStream* decode(DgdsFileCtx& ctx, Common::File& archive);
	Common::SeekableReadStream* copy(DgdsFileCtx& ctx, Common::File& archive);
};

bool DgdsChunk::isSection(const Common::String& section) {
       return section.equals(type);
}

bool DgdsChunk::isPacked(const Common::String& ext) {
	bool packed = false;

	if (0) {
	}
	else if (ext.equals("ADS")) {
		if (0) {
		} else if (strcmp(type, "SCR:") == 0) packed = true;
	}
	else if (ext.equals("BMP")) {
		if (0) {
		} else if (strcmp(type, "BIN:") == 0) packed = true;
		else if (strcmp(type, "VGA:") == 0) packed = true;
	}
	else if (ext.equals("DDS")) {
		if (strcmp(type, "DDS:") == 0) packed = true;
	}
	else if (ext.equals("OVL")) {
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
	else if (ext.equals("SCR")) {
		if (0) {
		} else if (strcmp(type, "BIN:") == 0) packed = true;
		else if (strcmp(type, "VGA:") == 0) packed = true;
	}
	else if (ext.equals("SDS")) {
		if (strcmp(type, "SDS:") == 0) packed = true;
	}
	else if (ext.equals("SNG")) {
		if (strcmp(type, "SNG:") == 0) packed = true;
	}
	else if (ext.equals("TDS")) {
		if (strcmp(type, "TDS:") == 0) packed = true;
	}

	else if (ext.equals("TTM")) {
		if (strcmp(type, "TT3:") == 0) packed = true;
	}
	return packed;
}

bool DgdsChunk::readHeader(DgdsFileCtx& ctx, Common::File& archive) {
	if (ctx.bytesRead >= ctx.inSize) {
		return false;
	}
	
	archive.read(type, DGDS_TYPENAME_MAX);

	if (type[DGDS_TYPENAME_MAX-1] != ':') {
		ctx.outSize = ctx.inSize;
		return false;
	}
	type[DGDS_TYPENAME_MAX] = '\0';

	ctx.bytesRead += DGDS_TYPENAME_MAX;
	ctx.outSize += DGDS_TYPENAME_MAX;

	chunkSize = archive.readUint32LE();
	if (chunkSize & 0x80000000) {
		chunkSize &= ~0x80000000;
		container = true;
	} else {
		container = false;
	}

	ctx.bytesRead += 4;
	ctx.outSize += 4;
	return true;
}
 
Common::SeekableReadStream* DgdsChunk::decode(DgdsFileCtx& ctx, Common::File& archive) {
	uint8 compression; /* 0=None, 1=RLE, 2=LZW */
	const char *descr[] = {"None", "RLE", "LZW"};
	uint32 unpackSize;
	Common::SeekableReadStream *ostream = 0;

	compression = archive.readByte();
	unpackSize = archive.readSint32LE();
	chunkSize -= (1 + 4);

	ctx.bytesRead += (1 + 4);
	ctx.outSize += (1 + 4);

	if (!container) {
		byte *dest = new byte[unpackSize];
		switch (compression) {
			case 0: {
				ctx.outSize += archive.read(dest,unpackSize);
				break;
				}
			case 1: {
				RleDecompressor dec;
				ctx.outSize += dec.decompress(dest,unpackSize,archive);
				break;
				}
			case 2:	{
				LzwDecompressor dec;
				ctx.outSize += dec.decompress(dest,unpackSize,archive);
				break;
				}
			default:
				debug("unknown chunk compression: %u", compression);
				break;
		}
		ostream = new Common::MemoryReadStream(dest, unpackSize, DisposeAfterUse::YES);
		ctx.bytesRead += chunkSize;
	}

	debug("    %s %u %s %u%c",
		type, chunkSize,
		descr[compression],
		unpackSize, (container ? '+' : ' '));
	return ostream;
}

Common::SeekableReadStream* DgdsChunk::copy(DgdsFileCtx& ctx, Common::File& archive) {
	Common::SeekableReadStream *ostream = 0;

	if (!container) {
		ostream = archive.readStream(chunkSize);
		ctx.bytesRead += chunkSize;
		ctx.outSize += chunkSize;
	}

	debug("    %s %u%c", type, chunkSize, (container ? '+' : ' '));
	return ostream;
}

static void explode(const char *indexName, bool save) {
	Common::File index, archive;

	if (index.open(indexName)) {
		uint32 version;
		uint16 nvolumes;

		version = index.readUint32LE();
		nvolumes = index.readUint16LE();

		debug("%u %u", version, nvolumes);

		for (uint i=0; i<nvolumes; i++) {
			char name[DGDS_FILENAME_MAX+1];
			uint16 nfiles;

			index.read(name, sizeof(name));
			name[DGDS_FILENAME_MAX] = '\0';
			
			nfiles = index.readUint16LE();
			
			archive.open(name);

			debug("--\n#%u %s %u", i, name, nfiles);
			for (uint j=0; j<nfiles; j++) {
				uint32 hash, offset;
				struct DgdsFileCtx ctx;
				uint32 inSize;

				hash = index.readUint32LE();
				offset = index.readUint32LE();
				debug("  %u %u", hash, offset);
				
				archive.seek(offset);
				archive.read(name, sizeof(name));
				name[DGDS_FILENAME_MAX] = '\0';
				inSize = archive.readUint32LE();
				ctx.init(inSize);
				debug("  #%u %s %u\n  --", j, name, ctx.inSize);
				
				if (inSize == 0xFFFFFFFF) {
					continue;
				}

				if (save) {
					Common::DumpFile out;
					char *buf;

					buf = new char[inSize];

					if (!out.open(name)) {
						debug("Couldn't write to %s", name);
					} else {
						archive.read(buf, inSize);
						out.write(buf, inSize);
						out.close();
						archive.seek(offset + 13 + 4);
					}
					delete [] buf;
				}
				
				const char *ext;

				ext = name + strlen(name) - 3;

				struct DgdsChunk chunk;
				while (chunk.readHeader(ctx, archive)) {
					bool packed = chunk.isPacked(ext);

					Common::SeekableReadStream *stream;
					stream = packed ? chunk.decode(ctx, archive) : chunk.copy(ctx, archive);
					if (strcmp(name, "DRAGON.PAL") == 0 && chunk.isSection("VGA:")) {
					    stream->read(palette, 256*3);
					}
					if (strcmp(name, "BGND.SCR") == 0 && chunk.isSection("BIN:")) {
					    stream->read(binData, 320000);
					}
					if (strcmp(name, "BGND.SCR") == 0 && chunk.isSection("VGA:")) {
					    stream->read(vgaData, 320000);
					}
					if (strcmp(ext, "BMP") == 0 && chunk.isSection("INF:")) {
						struct Tile {
							uint16 w, h;
						} *Tiles;
						uint16 count;

						count = stream->readUint16LE();
						Tiles = new Tile[count];
						debug("        [%u] =", count);
						for (uint16 k = 0; k < count; k++) {
							uint16 w, h;
							w = stream->readUint16LE();
							h = stream->readUint16LE();
							debug("        %ux%u", w, h);
							Tiles[k].w = w;
							Tiles[k].h = h;
						}
					}
					delete stream;
				}
				debug("  [%u:%u] --", ctx.bytesRead, ctx.outSize);

				archive.seek(offset + 13 + 4);
			}
			archive.close();
		}
	}	
}

Common::Error DgdsEngine::run() {
	initGraphics(320, 200);

	imgData = new byte[320*200];
	memset(imgData, 0, 320*200);

	debug("DgdsEngine::init");

	explode("volume.vga"/*vga,rmf*/, true);
	for (uint i=0; i<256*3; i+=3) {
	    palette[i+0] <<= 2;
	    palette[i+1] <<= 2;
	    palette[i+2] <<= 2;
	}

	g_system->getPaletteManager()->setPalette(palette, 0, 256);
	for (uint i=0; i<320*200; i+=2) {
	    imgData[i+0]  = ((vgaData[i/2] & 0xF0)     );
	    imgData[i+0] |= ((binData[i/2] & 0xF0) >> 4);
	    imgData[i+1]  = ((vgaData[i/2] & 0x0F) << 4);
	    imgData[i+1] |= ((binData[i/2] & 0x0F)     );
	}
	g_system->copyRectToScreen(imgData, 320, 0, 0, 320, 200);

	Common::EventManager *eventMan = g_system->getEventManager();
	Common::Event ev;

	while (!shouldQuit()) {
		eventMan->pollEvent(ev);

		g_system->updateScreen();
		g_system->delayMillis(20);
	}

	return Common::kNoError;
}

} // End of namespace Dgds

