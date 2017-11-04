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


#define DGDS_FILENAME_MAX 12
#define DGDS_TYPENAME_MAX 4

struct DgdsFileCtx {
	uint32 k, inSize, outSize;
	
	void init(uint32 inSize_);
};

void DgdsFileCtx::init(uint32 inSize_) {
	inSize = inSize_;
	k = 0;
	outSize = 0;
}

struct DgdsChunk {
	char type[DGDS_TYPENAME_MAX+1];
	uint32 chunkSize;
	bool chunky, packed;
	
	bool readHeader(DgdsFileCtx& ctx, Common::File& archive);
	bool isPacked(const char *ext);
	Common::SeekableReadStream* readData(DgdsFileCtx& ctx, Common::File& archive);
};

bool DgdsChunk::isPacked(const char *ext) {
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
	else if (strcmp(ext, "TDS") == 0 ) {
		if (strcmp(type, "TDS:") == 0) packed = true;
	}

	else if (strcmp(ext, "TTM") == 0) {
		if (strcmp(type, "TT3:") == 0) packed = true;
	}
	return packed;
}

bool DgdsChunk::readHeader(DgdsFileCtx& ctx, Common::File& archive) {
	archive.read(type, DGDS_TYPENAME_MAX);
	type[DGDS_TYPENAME_MAX] = '\0';

	if (type[DGDS_TYPENAME_MAX-1] != ':') {
		ctx.outSize = ctx.inSize;
		return false;
	}

	ctx.k += sizeof(type);
	ctx.outSize += sizeof(type);

	chunkSize = archive.readUint32LE();
	if (chunkSize & 0xF0000000) {
		chunkSize &= ~0xF0000000;
		chunky = true;
	} else {
		chunky = false;
	}

	ctx.k += 4;
	ctx.outSize += 4;
	return true;
}
 
Common::SeekableReadStream* DgdsChunk::readData(DgdsFileCtx& ctx, Common::File& archive) {
	Common::SeekableReadStream *ostream = 0;

	if (packed) {
		uint8 method; /* 0=None, 1=RLE, 2=LZW */
		const char *descr[] = {"None", "RLE", "LZW"};
		uint32 unpackSize;

		method = archive.readByte();
		unpackSize = archive.readSint32LE();
		chunkSize -= (1 + 4);

		ctx.k += (1 + 4);
		ctx.outSize += (1 + 4);

		if (!chunky) {
			Common::SeekableReadStream *istream;
			istream = archive.readStream(chunkSize);
			ctx.k += chunkSize;
			// TODO: decompress
			delete istream;
		}
		/*
		outSize += (1 + 4 + unpackSize);*/

		debug("    %s %u %s %u%c",
			type, chunkSize,
			descr[method],
			unpackSize, (chunky ? '+' : ' '));
	} else {
		if (!chunky) {
			ostream = archive.readStream(chunkSize);
			ctx.k += chunkSize;
			ctx.outSize += chunkSize;
		}

		debug("    %s %u%c", type, chunkSize, (chunky ? '+' : ' '));
	}

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

				while (ctx.k < ctx.inSize) {
					struct DgdsChunk chunk;
					
					if (!chunk.readHeader(ctx, archive)) {
						debug("    binary");
						break;
					}

					chunk.isPacked(ext);

					Common::SeekableReadStream *stream;
					stream = chunk.readData(ctx, archive);
					delete stream;
				}
				debug("  [%u] --", ctx.outSize);

				archive.seek(offset + 13 + 4);
			}
			archive.close();
		}
	}	
}

Common::Error DgdsEngine::run() {
	initGraphics(320, 200);

	debug("DgdsEngine::init");

	explode("volume.vga", false);

	return Common::kNoError;
}

} // End of namespace Dgds

