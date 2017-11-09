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
#include "common/stream.h"
#include "common/memstream.h"
#include "common/substream.h"
#include "common/system.h"
#include "common/platform.h"

#include "graphics/palette.h"
#include "graphics/surface.h"

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
byte _binData[320000];
byte _vgaData[320000];
byte *imgData, *_imgData;

uint16 _tcount;
uint16 *_tw, *_th;
uint32 *_toffsets;

uint16 *_mtx;
uint16 _mw, _mh;

Common::SeekableReadStream* ttm;

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
	uint32 inSize, outSize;
	
	void init(uint32 size);
};

void DgdsFileCtx::init(uint32 size) {
	inSize = size;
	outSize = 0;
}

typedef uint32 DGDS_ID;
typedef uint32 DGDS_EX;

#define ID_BIN	MKTAG('B','I','N',':')
#define ID_DAT	MKTAG('D','A','T',':')
#define ID_FNM	MKTAG('F','N','M',':')
#define ID_GAD	MKTAG('G','A','D',':')
#define ID_INF	MKTAG('I','N','F',':')
#define ID_MTX	MKTAG('M','T','X',':')
#define ID_PAG	MKTAG('P','A','G',':')
#define ID_REQ	MKTAG('R','E','Q',':')
#define ID_RES	MKTAG('R','E','S',':')
#define ID_SCR	MKTAG('S','C','R',':')
#define ID_SDS	MKTAG('S','D','S',':')
#define ID_SNG	MKTAG('S','N','G',':')
#define ID_SSM	MKTAG('S','S','M',':')
#define ID_TAG	MKTAG('T','A','G',':')
#define ID_TT3	MKTAG('T','T','3',':')
#define ID_VER	MKTAG('V','E','R',':')
#define ID_VGA	MKTAG('V','G','A',':')

#define	EX_ADH	MKTAG('.','A','D','H')
#define	EX_ADL	MKTAG('.','A','D','L')
#define	EX_ADS	MKTAG('.','A','D','S')
#define	EX_AMG	MKTAG('.','A','M','G')
#define	EX_BMP	MKTAG('.','B','M','P')
#define	EX_GDS	MKTAG('.','G','D','S')
#define	EX_INS	MKTAG('.','I','N','S')
#define	EX_PAL	MKTAG('.','P','A','L')
#define	EX_REQ	MKTAG('.','R','E','Q')
#define	EX_RST	MKTAG('.','R','S','T')
#define	EX_SCR	MKTAG('.','S','C','R')
#define	EX_SDS	MKTAG('.','S','D','S')
#define	EX_SNG	MKTAG('.','S','N','G')
#define	EX_SX	MKTAG('.','S','X', 0 )
#define	EX_TTM	MKTAG('.','T','T','M')
#define	EX_VIN	MKTAG('.','V','I','N')

#define	EX_DDS	MKTAG('.','D','D','S')
#define	EX_OVL	MKTAG('.','O','V','L')
#define	EX_TDS	MKTAG('.','T','D','S')

struct DgdsChunk {
	char type[DGDS_TYPENAME_MAX+1];
	DGDS_ID _type;

	uint32 chunkSize;
	bool container;
	
	bool readHeader(DgdsFileCtx& ctx, Common::SeekableReadStream& file, const char *name);
	bool isSection(const Common::String& section);
	bool isSection(DGDS_ID section);

	bool isPacked(DGDS_EX ex);
	Common::SeekableReadStream* decode(DgdsFileCtx& ctx, Common::SeekableReadStream& file);
	Common::SeekableReadStream* copy(DgdsFileCtx& ctx, Common::SeekableReadStream& file);
};

bool DgdsChunk::isSection(const Common::String& section) {
       return section.equals(type);
}

bool DgdsChunk::isSection(DGDS_ID section) {
       return (section == _type);
}

bool isFlatfile(Common::Platform platform, DGDS_EX _ex) {
	bool flat = false;

	switch (_ex) {
		case EX_RST:
			flat = true;
			break;
		default:
			break;
	}

	switch (platform) {
		case Common::kPlatformAmiga:
			switch (_ex) {
				case EX_BMP:
				case EX_SCR:
				case EX_INS:
//				case EX_SNG:
				case EX_AMG:
					flat = true;
					break;
				default:
					break;
			}

		case Common::kPlatformMacintosh:
			/* SOUNDS.SX is particularly large. */
			switch (_ex) {
				case EX_VIN:
					flat = true;
					break;
				default:
					break;
			}
		default:
			break;
	}
	return flat;
}

bool DgdsChunk::isPacked(DGDS_EX ex) {
	bool packed = false;

	switch (ex) {
		case EX_ADS:
		case EX_ADL:
		case EX_ADH:
			if (0) {}
			else if (_type == ID_SCR) packed = true;
			break;
		case EX_BMP:
			if (0) {}
			else if (_type == ID_BIN) packed = true;
			else if (_type == ID_VGA) packed = true;
			break;
		case EX_GDS:
			if (0) {}
			else if (_type == ID_SDS) packed = true;
			break;
		case EX_SCR:
			if (0) {}
			else if (_type == ID_BIN) packed = true;
			else if (_type == ID_VGA) packed = true;
			break;
		case EX_SDS:
			if (0) {}
			else if (_type == ID_SDS) packed = true;
			break;
		case EX_SNG:
			if (0) {}
			else if (_type == ID_SNG) packed = true;
			break;
		case EX_TTM:
			if (0) {}
			else if (_type == ID_TT3) packed = true;
			break;
		default:
			break;
	}

	switch (ex) {
		case EX_DDS:
			if (0) {}
			else if (strcmp(type, "DDS:") == 0) packed = true;
			break;
		case EX_OVL:
			if (0) {}
			else if (strcmp(type, "ADL:") == 0) packed = true;
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
			else if (strcmp(type, "T3V:") == 0) packed = true;
			else if (strcmp(type, "001:") == 0) packed = true;
			else if (strcmp(type, "003:") == 0) packed = true;
			else if (strcmp(type, "004:") == 0) packed = true;
			else if (strcmp(type, "101:") == 0) packed = true;
			else if (strcmp(type, "VGA:") == 0) packed = true;
			break;
		case EX_TDS:
			if (0) {}
			else if (strcmp(type, "TDS:") == 0) packed = true;
			break;
		default:
			break;
	}
	return packed;
}

bool DgdsChunk::readHeader(DgdsFileCtx& ctx, Common::SeekableReadStream& file, const char *name) {
	if (file.pos() >= file.size()) {
		return false;
	}

	file.read(type, DGDS_TYPENAME_MAX);

	if (type[DGDS_TYPENAME_MAX-1] != ':') {
		type[0] = '\0';
		_type = 0;
		debug("bad header in: %s", name);
		return false;
	}
	type[DGDS_TYPENAME_MAX] = '\0';
	_type = MKTAG(uint32(type[0]), uint32(type[1]), uint32(type[2]), uint32(type[3]));

	chunkSize = file.readUint32LE();
	if (chunkSize & 0x80000000) {
		chunkSize &= ~0x80000000;
		container = true;
	} else {
		container = false;
	}
	return true;
}
 
Common::SeekableReadStream* DgdsChunk::decode(DgdsFileCtx& ctx, Common::SeekableReadStream& file) {
	uint8 compression; /* 0=None, 1=RLE, 2=LZW */
	const char *descr[] = {"None", "RLE", "LZW"};
	uint32 unpackSize;
	Common::SeekableReadStream *ostream = 0;

	compression = file.readByte();
	unpackSize = file.readUint32LE();
	chunkSize -= (1 + 4);

	if (!container) {
		byte *dest = new byte[unpackSize];
		switch (compression) {
			case 0x00: {
				file.read(dest,chunkSize);
				break;
				}
			case 0x01: {
				RleDecompressor dec;
				dec.decompress(dest,unpackSize,file);
				break;
				}
			case 0x02: {
				LzwDecompressor dec;
				dec.decompress(dest,unpackSize,file);
				break;
				}
			default:
				file.skip(chunkSize);
				debug("unknown chunk compression: %u", compression);
				break;
		}
		ostream = new Common::MemoryReadStream(dest, unpackSize, DisposeAfterUse::YES);
	}

	debug("    %s %u %s %u%c",
		type, chunkSize,
		descr[compression],
		unpackSize, (container ? '+' : ' '));
	return ostream;
}

Common::SeekableReadStream* DgdsChunk::copy(DgdsFileCtx& ctx, Common::SeekableReadStream& file) {
	Common::SeekableReadStream *ostream = 0;

	if (!container) {
		ostream = new Common::SeekableSubReadStream(&file, file.pos(), file.pos()+chunkSize, DisposeAfterUse::NO);
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

enum {DGDS_NONE, DGDS_TAG, DGDS_REQ};

uint16 readStrings(Common::SeekableReadStream* stream){
	uint16 count;
	
	count = stream->readUint16LE();
	debug("        %u:", count);
	for (uint16 k=0; k<count; k++) {
		byte ch;
		uint16 idx;
		idx = stream->readUint16LE();
		
		Common::String str;
		while ((ch = stream->readByte()))
			str += ch;
		debug("        %2u: %2u, \"%s\"", k, idx, str.c_str());
	}
	return count;
}

void parseFile(Common::Platform platform, DGDS_EX _ex, Common::SeekableReadStream& input, const char* name, bool set) {
	struct DgdsFileCtx ctx;
	uint parent = DGDS_NONE;

	ctx.init(input.size());

	if (isFlatfile(platform, _ex)) {
		Common::String line;
		
		switch (_ex) {
			case EX_RST:
				input.hexdump(64);
				break;
			case EX_SCR:
				/* Unknown image format (Amiga). */
				input.hexdump(64);
				break;
			case EX_BMP:
				/* Unknown image format (Amiga). */
				input.hexdump(64);
				break;
			case EX_INS:
				/* IFF-8SVX sound sample (Amiga). */
				input.hexdump(16);
				break;
			case EX_SNG:
				/* IFF-SMUS music (Amiga). */
				input.hexdump(16);
				break;
			case EX_AMG:
				/* (Amiga). */
				line = input.readLine();
				while (!input.eos() && !line.empty()) {
					debug("    \"%s\"", line.c_str());
					line = input.readLine();
				}
				break;
			case EX_VIN:
				/* (Macintosh). */
				line = input.readLine();
				while (!input.eos() && !line.empty()) {
					debug("    \"%s\"", line.c_str());
					line = input.readLine();
				}
				input.hexdump(input.size());
				break;
			default:
				break;
		}
	} else {
		uint16 tcount;
		uint16 *tw, *th;
		uint32 *toffsets;
		
		uint16 *mtx;
		uint16 mw, mh;
		
		struct DgdsChunk chunk;
		while (chunk.readHeader(ctx, input, name)) {
			Common::SeekableReadStream *stream;
			
			bool packed = chunk.isPacked(_ex);
			stream = packed ? chunk.decode(ctx, input) : chunk.copy(ctx, input);
			if (stream) ctx.outSize += stream->size();
			
			/*
			 debug(">> %u:%u", stream->pos(), file->pos());
			 stream->hexdump(stream->size());*/
			switch (_ex) {
				case EX_SDS:
					if (chunk.isSection(ID_SDS)) {
						stream->hexdump(stream->size());
					}
					break;
				case EX_TTM:
					if (chunk.isSection(ID_VER)) {
						char version[5];
						
						stream->read(version, sizeof(version));
						debug("        %s", version);
					} else if (chunk.isSection(ID_PAG)) {
						uint16 pages;
						pages = stream->readUint16LE();
						debug("        %u", pages);
					} else if (chunk.isSection(ID_TT3)) {
						if (set) {
							ttm = stream->readStream(stream->size());
						} else {
							stream->skip(stream->size());
						}/*
						 while (!stream->eos()) {
						 uint16 code;
						 byte count;
						 uint op;
						 
						 code = stream->readUint16LE();
						 count = code & 0x000F;
						 op = code & 0xFFF0;
						 
						 debugN("\tOP: 0x%4.4x %2u ", op, count);
						 if (count == 0x0F) {
						 Common::String sval;
						 byte ch[2];
						 
						 do {
						 ch[0] = stream->readByte();
						 ch[1] = stream->readByte();
						 sval += ch[0];
						 sval += ch[1];
						 } while (ch[0] != 0 && ch[1] != 0);
						 
						 debugN("\"%s\"", sval.c_str());
						 } else {
						 int ival;
						 
						 for (byte k=0; k<count; k++) {
						 ival = stream->readSint16LE();
						 
						 if (k == 0)
						 debugN("%d", ival);
						 else
						 debugN(", %d", ival);
						 }
						 }
						 debug(" ");
						 }*/
					} else if (chunk.isSection(ID_TAG)) {
						stream->hexdump(stream->size());
						uint16 count;
						
						count = stream->readUint16LE();
						debug("        %u", count);
						// something fishy here. the first two entries sometimes are an empty string or non-text junk.
						// most of the time entries have text (sometimes with garbled characters).
						// this parser is likely not ok. but the NUL count seems to be ok.
						for (uint16 k=0; k<count; k++) {
							byte ch;
							uint16 idx;
							Common::String str;
							
							idx = stream->readUint16LE();
							while ((ch = stream->readByte()))
								str += ch;
							debug("        %2u: %2u, \"%s\"", k, idx, str.c_str());
						}
					}
					break;
				case EX_GDS:
					if (chunk.isSection(ID_INF)) {
						char version[7];
						
						// guess. 
						uint dummy = stream->readUint32LE();
						stream->read(version, sizeof(version));
						debug("        %u, \"%s\"", dummy, version);
						
					} else if (chunk.isSection(ID_SDS)) {
						stream->hexdump(stream->size());
					}
					break;
				case EX_ADS:
				case EX_ADL:
				case EX_ADH:
					if (chunk.isSection(ID_VER)) {
						char version[5];
						
						stream->read(version, sizeof(version));
						debug("        %s", version);
					} else if (chunk.isSection(ID_RES)) {
						readStrings(stream);
					} else if (chunk.isSection(ID_SCR)) {
						stream->hexdump(stream->size());
					} else if (chunk.isSection(ID_TAG)) {
						readStrings(stream);
					}
					break;
				case EX_REQ:
					if (chunk.container) {
						if (chunk.isSection(ID_TAG)) {
							parent = DGDS_TAG;
						} else if (chunk.isSection(ID_REQ)) {
							parent = DGDS_REQ;
						}
					} else  {
						if (parent == DGDS_TAG) {
							if (chunk.isSection(ID_REQ)) {
								readStrings(stream);
							} else if (chunk.isSection(ID_GAD)) {
								readStrings(stream);
							}
						} else if (parent == DGDS_REQ) {
							stream->hexdump(stream->size());
							
							if (chunk.isSection(ID_REQ)) {
								stream->skip(stream->size());
							} else if (chunk.isSection(ID_GAD)) {
								stream->skip(stream->size());
							}
						}
					}
					break;
				case EX_SNG:
					/* DOS. */
					debug("ENTER SNG");
					if (chunk.isSection(ID_SSM)) {
						stream->hexdump(stream->size());
						stream->skip(stream->size());
					} else if (chunk.isSection(ID_SNG)) {
						stream->hexdump(stream->size());
						stream->skip(stream->size());
					} else if (chunk.isSection(ID_INF)) {
						stream->hexdump(stream->size());
						stream->skip(stream->size());
					}
					break;
				case EX_SX:
					/* Macintosh. */
					if (chunk.isSection(ID_INF)) {
						uint16 type, count;
						
						type = stream->readUint16LE();
						count = stream->readUint16LE();
						
						debug("        %u [%u]:", type, count);
						for (uint16 k = 0; k < count; k++) {
							uint16 idx;
							idx = stream->readUint16LE();
							debug("        %2u: %2u", k, idx);
						}
					} else if (chunk.isSection(ID_TAG)) {
						readStrings(stream);
					} else if (chunk.isSection(ID_FNM)) {
						readStrings(stream);
					} else if (chunk.isSection(ID_DAT)) {
						uint16 idx;
						idx = stream->readUint16LE();
						debug("        %2u", idx);
						
						stream->hexdump(stream->size());
						stream->skip(stream->size()-stream->pos());
						/*
						 uint unpackSize = 1000;
						 byte *dest = new byte[unpackSize];
						 RleDecompressor dec;
						 dec.decompress(dest,unpackSize,file);
						 ostream = new Common::MemoryReadStream(dest, unpackSize, DisposeAfterUse::YES);*/
						/*
						 uint size;
						 size = stream->size();
						 
						 byte *dest = new byte[unpackSize];
						 
						 Common::DumpFile out;
						 char *buf;
						 
						 buf = new char[size];
						 
						 if (!out.open(name)) {
						 debug("Couldn't write to %s", name);
						 } else {
						 stream->read(buf, size);
						 out.write(buf, size);
						 out.close();
						 }
						 delete [] buf;
						 */
					}
					break;
				case EX_PAL:
					/* DOS & Macintosh. */
					if (set) {
						if (chunk.isSection(ID_VGA)) {
							stream->read(palette, 256*3);
							
							for (uint k=0; k<256*3; k+=3) {
								palette[k+0] <<= 2;
								palette[k+1] <<= 2;
								palette[k+2] <<= 2;
							}
						}
					} else {
						if (chunk.isSection(ID_VGA)) {
							stream->skip(256*3);
						}
					}
					break;
				case EX_SCR:
					/* currently does not handle the VQT: and OFF: tags
					 for the compressed pics in the DOS port. */
					if (set) {
						if (chunk.isSection(ID_BIN)) {
							stream->read(binData, stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->read(vgaData, stream->size());
						}
					} else {
						if (chunk.isSection(ID_BIN)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->skip(stream->size());
						}
					}
					break;
				case EX_BMP:
					/* currently does not handle the VQT: and OFF: tags
					 for the compressed pics in the DOS port. */
					if (chunk.isSection(ID_INF)) {
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
					} else if (chunk.isSection(ID_MTX)) {
						uint32 mcount;
						
						mw = stream->readUint16LE();
						mh = stream->readUint16LE();
						mcount = uint32(mw)*mh;
						debug("        %ux%u: %u bytes", mw, mh, mcount*2);
						
						mtx = new uint16[mcount];
						for (uint32 k=0; k<mcount; k++) {
							uint16 tile;
							tile = stream->readUint16LE();
							mtx[k] = tile;
							//								debug("        %u", tile);
						}
					}
					
					// DCORNERS.BMP, DICONS.BMP, HELICOP2.BMP, WALKAWAY.BMP, KARWALK.BMP, BLGREND.BMP, FLAMDEAD.BMP, W.BMP, ARCADE.BMP
					// MTX: SCROLL.BMP (intro title), SCROLL2.BMP
					if (set) {
						if (chunk.isSection(ID_BIN)) {
							stream->read(_binData, stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->read(_vgaData, stream->size());
						} else if (chunk.isSection(ID_INF)) {
							_tcount = tcount;
							_tw = tw;
							_th = th;
							_toffsets = toffsets;
						} else if (chunk.isSection(ID_MTX)) {
							_mtx = mtx;
							_mw = mw;
							_mh = mh;
						}
					} else {
						if (chunk.isSection(ID_BIN)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->skip(stream->size());
						}
					}
					break;
				default:
					break;
			}
			
			delete stream;
		}
	}

	debug("  [%u:%u] --", input.pos(), ctx.outSize);
}

static void explode(Common::Platform platform, const char *indexName, const char *fileName) {
	Common::File index, volume;
	Common::SeekableSubReadStream *file;

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
			
			volume.open(name);

			debug("--\n#%u %s %u", i, name, nfiles);
			for (uint j=0; j<nfiles; j++) {
				uint32 hash, offset;
				uint32 inSize;

				hash = index.readUint32LE();
				offset = index.readUint32LE();
				debug("  %u %u", hash, offset);
				
				volume.seek(offset);
				volume.read(name, sizeof(name));
				name[DGDS_FILENAME_MAX] = '\0';
				inSize = volume.readUint32LE();
				debug("  #%u %s %x=%x %u\n  --", j, name, hash, dgdsHash(name, salt), inSize);
				
				if (inSize == 0xFFFFFFFF) {
					continue;
				}

				if (fileName && scumm_stricmp(name, fileName)) {
					volume.seek(offset+13+4+inSize);
					continue;
				}

				file = new Common::SeekableSubReadStream(&volume, offset+13+4, offset+13+4+inSize, DisposeAfterUse::NO);

				if (!fileName) {
					Common::DumpFile out;
					char *buf;

					buf = new char[inSize];

					if (!out.open(name)) {
						debug("Couldn't write to %s", name);
					} else {
						file->read(buf, inSize);
						out.write(buf, inSize);
						out.close();
						file->seek(0);
					}
					delete [] buf;
				}
				
				const char *dot;
				DGDS_EX _ex;

				if ((dot = strrchr(name, '.'))) {
					_ex = MKTAG(dot[0], dot[1], dot[2], dot[3]);
				} else {
					_ex = 0;
				}

				parseFile(platform, _ex, *file, name, fileName);

				if (fileName) {
				    volume.close();
				    index.close();
				    return;
				}
			}
			volume.close();
		}
		index.close();
	}	
}

int sw = 320, sh = 200;
int bw = 0, bh = 0;
int bk = 0;

char bmpNames[16][DGDS_FILENAME_MAX+1];
char scrNames[16][DGDS_FILENAME_MAX+1];
int id = 0, sid = 0;

void interpret(Common::Platform platform, const char *rootName) {
	if (!ttm) return;

	while (!ttm->eos()) {
		uint16 code;
		byte count;
		uint op;
		int16 ivals[16];

		Common::String sval;

		code = ttm->readUint16LE();
		count = code & 0x000F;
		op = code & 0xFFF0;

		debugN("\tOP: 0x%4.4x %2u ", op, count);
		if (count == 0x0F) {
			byte ch[2];

			do {
				ch[0] = ttm->readByte();
				ch[1] = ttm->readByte();
				sval += ch[0];
				sval += ch[1];
			} while (ch[0] != 0 && ch[1] != 0);

			debugN("\"%s\"", sval.c_str());
		} else {
			int16 ival;

			for (byte k=0; k<count; k++) {
				ival = ttm->readSint16LE();
				ivals[k] = ival;

				if (k == 0)
					debugN("%d", ival);
				else
					debugN(", %d", ival);
			}
		}
		debug(" ");

		byte *vgaData_;
		byte *binData_;
		Graphics::Surface *dst;
		switch (op) {
			case 0xf010:
				// LOAD SCR
				Common::strlcpy(scrNames[sid], sval.c_str(), sizeof(scrNames[sid]));
				break;
			case 0xf020:
				// LOAD BMP
				Common::strlcpy(bmpNames[id], sval.c_str(), sizeof(bmpNames[id]));
				break;
			case 0xf050:
				// LOAD PAL
				explode(platform, rootName, sval.c_str());
				g_system->getPaletteManager()->setPalette(palette, 0, 256);
				break;

			case 0x1050:
				// SELECT BMP
				id = ivals[0];
				break;

			case 0x1060:
				// SELECT SCR
				sid = ivals[0];
				break;

			case 0x1030:
				// SET BMP?
				explode(platform, rootName, bmpNames[id]);

				bk = ivals[0];

				bw = _tw[bk]; bh = _th[bk];
				vgaData_ = _vgaData + (_toffsets[bk]>>1);
				binData_ = _binData + (_toffsets[bk]>>1);

				for (int i=0; i<bw*bh; i+=2) {
					_imgData[i+0]  = ((vgaData_[i/2] & 0xF0)     );
					_imgData[i+0] |= ((binData_[i/2] & 0xF0) >> 4);
					_imgData[i+1]  = ((vgaData_[i/2] & 0x0F) << 4);
					_imgData[i+1] |= ((binData_[i/2] & 0x0F)     );
				}
				break;
			case 0x1090:
				// SET PAL? UPDATE SCREEN?
				explode(platform, rootName, scrNames[sid]);

				vgaData_ = vgaData;
				binData_ = binData;

				for (int i=0; i<sw*sh; i+=2) {
					imgData[i+0]  = ((vgaData_[i/2] & 0xF0)     );
					imgData[i+0] |= ((binData_[i/2] & 0xF0) >> 4);
					imgData[i+1]  = ((vgaData_[i/2] & 0x0F) << 4);
					imgData[i+1] |= ((binData_[i/2] & 0x0F)     );
				}
				g_system->copyRectToScreen(imgData, sw, 0, 0, sw, sh);
				g_system->updateScreen();
				break;

			case 0x4120:
				// STORE?
				ivals[0] = 0;
				ivals[1] = 0;
				ivals[2] = 320;
				ivals[3] = 200;
			case 0x4200: {
				// STORE AREA?
				const Common::Rect destRect(ivals[0], ivals[1], ivals[0]+ivals[2], ivals[1]+ivals[3]);
				Common::Rect clippedDestRect(0, 0, sw, sh);
				clippedDestRect.clip(destRect);

				const int rows = clippedDestRect.height();
				const int columns = clippedDestRect.width();

				dst = g_system->lockScreen();

				byte *src = imgData + clippedDestRect.top*sw + clippedDestRect.left;
				byte *ptr = (byte *)dst->getBasePtr(clippedDestRect.left, clippedDestRect.top);
				for (int i=0; i<rows; ++i) {
					for (int j=0; j<columns; ++j) {
						src[j] = ptr[j];
					}
					ptr += dst->pitch;
					src += sw;
				}
				g_system->unlockScreen();
				}
				break;


			case 0x4110:
				// FADE TO BLACK?
				memset(imgData, 0, 320*200);
				g_system->fillScreen(0);

				g_system->copyRectToScreen(imgData, sw, 0, 0, sw, sh);
				g_system->updateScreen();
				break;

			case 0x0ff0:
				// RESET AREA?
				g_system->updateScreen();
				g_system->copyRectToScreen(imgData, sw, 0, 0, sw, sh);
				break;

			case 0xa500: {
				// DRAW BMP?
				const Common::Rect destRect(ivals[0], ivals[1], ivals[0]+bw, ivals[1]+bh);
				Common::Rect clippedDestRect(0, 0, sw, sh);
				clippedDestRect.clip(destRect);

				const Common::Point croppedBy(clippedDestRect.left-destRect.left, clippedDestRect.top-destRect.top);

				const int rows = clippedDestRect.height();
				const int columns = clippedDestRect.width();

				dst = g_system->lockScreen();

				byte *src = _imgData + croppedBy.y * bw + croppedBy.x;
				byte *ptr = (byte *)dst->getBasePtr(clippedDestRect.left, clippedDestRect.top);
				for (int i=0; i<rows; ++i) {
					for (int j=0; j<columns; ++j) {
						if (src[j])
							ptr[j] = src[j];
					}
					ptr += dst->pitch;
					src += bw;
				}
				g_system->unlockScreen();
				}
				break;
			default:
				debug("        MEEP!");
				break;
		}
		break;
	}
}

Common::Error DgdsEngine::run() {
	Common::Platform platform;
	const char *rootName;

	initGraphics(320, 200);

	memset(palette, 1, 256*3);

	imgData = new byte[320*200];
	_imgData = new byte[320*200];
	memset(imgData, 0, 320*200);
	memset(_imgData, 0, 320*200);
	ttm = 0;

	debug("DgdsEngine::init");

	// Rise of the Dragon.
	platform = Common::kPlatformMacintosh;
	rootName = "volume.rmf"; // volume.rmf, volume.vga
/*
	explode(platform, rootName, "TITLE1.TTM");
	return Common::kNoError;
	explode(platform, rootName, "DYNAMIX.SNG");*/

//	explode(platform, rootName, 0);

	g_system->fillScreen(0);

/*
	// grayscale palette.
	for (uint i=0; i<256; i++) {
	    palette[i*3+0] = 255-i;
	    palette[i*3+1] = 255-i;
	    palette[i*3+2] = 255-i;
	}*/

	Common::EventManager *eventMan = g_system->getEventManager();
	Common::Event ev;

	int k = 0;
	while (!shouldQuit()) {/*
		uint w, h;
		byte *vgaData_;
		byte *binData_;*/

		if (eventMan->pollEvent(ev)) {
			//int n = (int)_tcount-1;
			if (ev.type == Common::EVENT_KEYDOWN) {
				switch (ev.kbd.keycode) {
					case Common::KEYCODE_LEFT:	/*if (k > 0) k--;*/	break;
					case Common::KEYCODE_RIGHT:	/*if (k < n) k++;*/	break;
					case Common::KEYCODE_ESCAPE:	return Common::kNoError;
					default:			break;
				}
			}
		}

		if (!ttm || ttm->eos()) {
		    delete ttm;
		    ttm = 0;
		    if ((k%2) == 0)
			explode(platform, rootName, "TITLE1.TTM");
		    else
			explode(platform, rootName, "TITLE2.TTM");
		    k++;
		}
		interpret(platform, rootName);
/*
		// SCR:BIN|VGA viewer.
		w = 320; h = 200;
		vgaData_ = vgaData;
		binData_ = binData;

		for (uint i=0; i<w*h; i+=2) {
			imgData[i+0]  = ((vgaData_[i/2] & 0xF0)     );
			imgData[i+0] |= ((binData_[i/2] & 0xF0) >> 4);
			imgData[i+1]  = ((vgaData_[i/2] & 0x0F) << 4);
			imgData[i+1] |= ((binData_[i/2] & 0x0F)     );
		}
		g_system->fillScreen(0);
		g_system->copyRectToScreen(imgData, w, 0, 0, w, h);*/
/*
		// BMP:INF|BIN|VGA browser.
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
/*
		// BMP:INF|BIN|VGA|MTX browser.
		uint cx, cy;
		cx = cy = 0;
		g_system->fillScreen(0);
		for (uint y=0; cx<_mw; y++) {
			if ((k+y) >= _mw)
			    break;

			for (uint x=0; x<_mh; x++) {
				uint j = (k+y)*_mh+x;
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
*/
		g_system->delayMillis(50);
	}

	return Common::kNoError;
}

} // End of namespace Dgds

