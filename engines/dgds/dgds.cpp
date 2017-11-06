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

uint16 _tcount;
uint16 *_tw, *_th;
uint32 *_toffsets;

uint16 *_mtx;
uint16 _mw, _mh;

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
		uint size;
		switch (compression) {
			case 0x00: {
				size = archive.read(dest,chunkSize);
				break;
				}
			case 0x01: {
				RleDecompressor dec;
				size = dec.decompress(dest,unpackSize,archive);
				break;
				}
			case 0x02: {
				LzwDecompressor dec;
				size = dec.decompress(dest,unpackSize,archive);
				break;
				}
			default:
				archive.skip(chunkSize);
				size = 0;
				debug("unknown chunk compression: %u", compression);
				break;
		}
		ostream = new Common::MemoryReadStream(dest, unpackSize, DisposeAfterUse::YES);
		ctx.bytesRead += chunkSize;
		ctx.outSize += size;
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

/*
  input:
    s - pointer to ASCIIZ filename string
    idx - pointer to an array of 4 bytes (hash indexes)
  return:
    hash - filename hash
*/
uint32 dgdsHash(const char *s, byte *idx) {
	uint32 i, c;
	uint16 isum, ixor;
	isum = 0;
	ixor = 0;
	for (i = 0; s[i]; i++) {
		c = toupper(s[i]);
		isum += c;
		ixor ^= c;
	}
	/* both types here MUST be int16 */
	isum *= ixor;
	c = 0;
	for (ixor = 0; ixor < 4; ixor++) {
		c <<= 8;
		/* can use only existing characters
		   ("i" holds the string length now) */
		if (i > idx[ixor]) {
			c |= toupper(s[idx[ixor]]);
		}
	}
	c += isum;
	return c;
}

static void explode(const char *indexName, bool save) {
	Common::File index, archive;

	if (index.open(indexName)) {
		byte salt[4];
		uint16 nvolumes;

		index.read(salt, sizeof(salt));
		nvolumes = index.readUint16LE();

		debug("(%u,%u,%u,%u) %u", salt[0], salt[1], salt[2], salt[3], nvolumes);

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
				debug("  #%u %s %x=%x %u\n  --", j, name, hash, dgdsHash(name, salt), ctx.inSize);
				
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
					if (strcmp(name, "BGND.SCR")) {
/*
						if (chunk.isSection("BIN:"))
							stream->read(binData, stream->size());
						else if (chunk.isSection("VGA:"))
							stream->read(vgaData, stream->size());
*/
					}

					uint16 tcount;
					uint16 *tw, *th;
					uint32 *toffsets;

					uint16 *mtx;
					uint16 mw, mh;

					if (strcmp(ext, "BMP") == 0) {
						if (chunk.isSection("INF:")) {
							tcount = stream->readUint16LE();
							debug("        [%u] =", tcount);

							tw = new uint16[tcount];
							th = new uint16[tcount];
							for (uint16 k=0; k<tcount; k++)
								tw[k] = stream->readUint16LE();
							for (uint16 k=0; k<tcount; k++)
								th[k] = stream->readUint16LE();

							uint32 sz = 0;
							toffsets = new uint32[tcount];
							for (uint16 k=0; k<tcount; k++) {
								debug("        %ux%u @%u", tw[k], th[k], sz);
								toffsets[k] = sz;
								sz += tw[k]*th[k];
							}
							debug("        BIN|VGA: %u bytes", (sz+1)/2);
						} else if (chunk.isSection("MTX:")) {
							uint32 mcount;

							mw = stream->readUint16LE();
							mh = stream->readUint16LE();
							mcount = uint32(mw)*mh;
							debug("        %ux%u: %u bytes", mw, mh, mcount*2);

							mtx = new uint16[mcount];
							for (uint32 k=0; k<mcount; k++) {
								uint16 tile;
								tile = stream->readUint16LE();
								//mtx[k%mh*mw + k/mh] = tile;
								mtx[k] = tile;
//								debug("        %u", tile);
							}
						}
					}

					// DCORNERS.BMP, DICONS.BMP, HELICOP2.BMP, WALKAWAY.BMP, KARWALK.BMP, BLGREND.BMP, FLAMDEAD.BMP, W.BMP, ARCADE.BMP
					// MTX: SCROLL.BMP (intro title), SCROLL2.BMP
					if (strcmp(name, "SCROLL2.BMP") == 0) {
						if (chunk.isSection("BIN:"))
							stream->read(binData, stream->size());
						else if (chunk.isSection("VGA:"))
							stream->read(vgaData, stream->size());

						_tcount = tcount;
						_tw = tw;
						_th = th;
						_toffsets = toffsets;

						_mtx = mtx;
						_mw = mw;
						_mh = mh;
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
	memset(palette, 1, 256*3);
	memset(imgData, 0, 320*200);

	debug("DgdsEngine::init");

	// Rise of the Dragon.
	explode("volume.vga"/*vga,rmf*/, true);
	for (uint i=0; i<256*3; i+=3) {
		palette[i+0] <<= 2;
		palette[i+1] <<= 2;
		palette[i+2] <<= 2;
	}

	g_system->getPaletteManager()->setPalette(palette, 0, 256);

	Common::EventManager *eventMan = g_system->getEventManager();
	Common::Event ev;

	int k = 0;
	while (!shouldQuit()) {
		uint w, h;
		byte *vgaData_;
		byte *binData_;

		if (eventMan->pollEvent(ev)) {
			int n = (int)_tcount-1;
			if (ev.type == Common::EVENT_KEYDOWN) {
				switch (ev.kbd.keycode) {
					case Common::KEYCODE_LEFT:	if (k > 0) k--;	break;
					case Common::KEYCODE_RIGHT:	if (k < n) k++;	break;
					default:				break;
				}
			}
		}
/*
		w = _tw[k]; h = _th[k];
		vgaData_ = vgaData + (_toffsets[k]>>1);
		binData_ = binData + (_toffsets[k]>>1);

		for (uint i=0; i<w*h; i+=2) {
			imgData[i+0]  = ((vgaData_[i/2] & 0xF0)     );
			imgData[i+0] |= ((binData_[i/2] & 0xF0) >> 4);
			imgData[i+1]  = ((vgaData_[i/2] & 0x0F) << 4);
			imgData[i+1] |= ((binData_[i/2] & 0x0F)     );
		}
		//g_system->fillScreen(0);
		g_system->copyRectToScreen(imgData, w, 0, 0, w, h);*/
		uint cx, cy;
		cx = cy = 0;
		// 8x13 tiles, 320x9 matrix, 9x5 tiles.
		for (uint y=0; y<40; y++) {
			for (uint x=0; x<_mh; x++) {
				uint j = y*_mh+x;
				w = _tw[_mtx[j]]; h = _th[_mtx[j]];
				vgaData_ = vgaData + (_toffsets[_mtx[j]]>>1);
				binData_ = binData + (_toffsets[_mtx[j]]>>1);

				for (uint i=0; i<w*h; i+=2) {
					imgData[i+0]  = ((vgaData_[i/2] & 0xF0)     );
					imgData[i+0] |= ((binData_[i/2] & 0xF0) >> 4);
					imgData[i+1]  = ((vgaData_[i/2] & 0x0F) << 4);
					imgData[i+1] |= ((binData_[i/2] & 0x0F)     );
				}
				g_system->copyRectToScreen(imgData, w, cx, cy, w, h);
				cy += h;
			}
			cy = 0;
			cx += w;
		}

		g_system->updateScreen();
		g_system->delayMillis(20);
	}

	return Common::kNoError;
}

} // End of namespace Dgds

