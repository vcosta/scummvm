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

#include "common/iff_container.h"

#include "audio/audiostream.h"
#include "audio/mixer.h"
#include "audio/decoders/iff_sound.h"
#include "audio/decoders/aiff.h"

#include "graphics/palette.h"
#include "graphics/surface.h"
#include "graphics/font.h"
#include "graphics/fontman.h"

#include "engines/advancedDetector.h"
#include "engines/util.h"

#include "dgds/decompress.h"
#include "dgds/detection_tables.h"

#include "dgds/dgds.h"

namespace Dgds {
	typedef unsigned uint32;
	typedef unsigned short uint16;
	typedef unsigned char uint8;
	typedef unsigned char byte;

byte palette[256*3];

byte binData[32000];
byte vgaData[32000];

byte ma8Data[64000];

Graphics::Surface* _binData = 0;
Graphics::Surface* _vgaData = 0;

byte *imgData, *_imgData;
Common::MemoryReadStream *soundData;

uint16 _tcount;
uint16 *_tw, *_th;
uint32 *_toffsets;

uint16 *_mtx;
uint16 _mw, _mh;

Common::SeekableReadStream* ttm;


DgdsEngine::DgdsEngine(OSystem *syst, const DgdsGameDescription *gameDesc)
 : Engine(syst) {
	 syncSoundSettings();

	_console = new DgdsConsole(this);

	_platform = gameDesc->desc.platform;
	_rmfName = gameDesc->desc.filesDescriptions[0].fileName;
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
#define ID_TAG	MKTAG('T','A','G',':')
#define ID_TT3	MKTAG('T','T','3',':')
#define ID_VER	MKTAG('V','E','R',':')
#define ID_VGA	MKTAG('V','G','A',':')

/* Heart of China */
#define ID_MA8	MKTAG('M','A','8',':')
#define ID_DDS	MKTAG('D','D','S',':')
#define ID_THD	MKTAG('T','H','D',':')

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

/* Heart of China */
#define	EX_DDS	MKTAG('.','D','D','S')
#define	EX_TDS	MKTAG('.','T','D','S')

#define	EX_OVL	MKTAG('.','O','V','L')

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
			else if (_type == ID_MA8) packed = true;
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
		case EX_TDS:
			if (0) {}
			else if (_type == ID_THD) packed = true;
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
			else if (strcmp(type, "TDS:") == 0) packed = true;  /* ? */
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

void parseFile(Common::Platform platform, DGDS_EX _ex, Common::SeekableReadStream& file, const char* name, int resource) {
	struct DgdsFileCtx ctx;
	uint parent = DGDS_NONE;

	ctx.init(file.size());

	if (isFlatfile(platform, _ex)) {
		uint16 tcount;
		uint16 *tw, *th;
		uint32 *toffsets;
		Common::String line;
		
		switch (_ex) {
			case EX_RST:
				file.hexdump(64);
				break;
			case EX_SCR: {
				/* Unknown image format (Amiga). */
				byte tag[5];
				file.read(tag, 4);		/* maybe */
				tag[4] = '\0';

				uint16 height, planes;
				height = file.readUint16BE();	/* always 200 (320x200 screen). */
				planes = file.readUint16BE();	/* always 5 (32 color). */

				debug("    \"%s\" height:%u bpp:%u size:~%u",
						tag, height, planes,
						uint(320+15)/16*200*planes);
				file.hexdump(file.size());

				if (resource == 0) {
				    //file.read(binData, file.size());
				}
				}
			        break;
			case EX_BMP: {
				/* Unknown image format (Amiga). */
				tcount = file.readUint16BE();
				tw = new uint16[tcount];
				th = new uint16[tcount];

				uint32 packedSize, unpackedSize;
				unpackedSize = file.readUint32BE();
				debug("        [%u] %u =", tcount, unpackedSize);

				uint32 sz = 0;
				toffsets = new uint32[tcount];
				for (uint16 k=0; k<tcount; k++) {
					tw[k] = file.readUint16BE();
					th[k] = file.readUint16BE();
					debug("        %ux%u ~@%u", tw[k], th[k], sz);

					toffsets[k] = sz;
					sz += uint(tw[k]+15)/16*th[k]*5;
				}
				debug("    ~= [%u]", sz);

				/* this is a wild guess. */
				byte version[13];
				file.read(version, 12);
				version[12] = '\0';
				debug("    %s", version);

				unpackedSize = file.readUint32BE();
				packedSize = file.readUint32BE();
				debug("        %u -> %u",
						packedSize, unpackedSize);

				if (resource >= 0) {
//				    file.read(_binData, file.size());

				    _tcount = tcount;
				    _tw = tw;
				    _th = th;
				    _toffsets = toffsets;
				}
				}
				break;
			case EX_INS: {
				/* AIFF sound sample (Amiga). */
			        byte *dest = new byte[file.size()];
			        file.read(dest,file.size());
			        soundData = new Common::MemoryReadStream(dest, file.size(), DisposeAfterUse::YES);
				}
				break;
			case EX_SNG:
				/* IFF-SMUS music (Amiga). */
				file.hexdump(16);
				break;
			case EX_AMG:
				/* (Amiga). */
				line = file.readLine();
				while (!file.eos() && !line.empty()) {
					debug("    \"%s\"", line.c_str());
					line = file.readLine();
				}
				break;
			case EX_VIN:
				/* (Macintosh). */
				line = file.readLine();
				while (!file.eos() && !line.empty()) {
					debug("    \"%s\"", line.c_str());
					line = file.readLine();
				}
				file.hexdump(file.size());
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
		while (chunk.readHeader(ctx, file, name)) {
			Common::SeekableReadStream *stream;
			
			bool packed = chunk.isPacked(_ex);
			stream = packed ? chunk.decode(ctx, file) : chunk.copy(ctx, file);
			if (stream) ctx.outSize += stream->size();
			
			/*
			 debug(">> %u:%u", stream->pos(), file->pos());
			 stream->hexdump(stream->size());*/
			switch (_ex) {
				case EX_TDS:
					if (chunk.isSection(ID_THD)) {
						stream->hexdump(stream->size());
					}
					break;
				case EX_DDS:
					if (chunk.isSection(ID_DDS)) {
						stream->hexdump(stream->size());
					}
					break;
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
						if (resource == 0) {
							ttm = stream->readStream(stream->size());
						} else {
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
							}
						}
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
					if (chunk.isSection(ID_SNG)) {
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
					if (resource == 0) {
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
					/* currently does not handle the VQT: and OFF: chunks
					 for the compressed pics in the DOS port. */
					if (resource == 0) {
						if (chunk.isSection(ID_BIN)) {
							stream->read(binData, stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->read(vgaData, stream->size());
						} else if (chunk.isSection(ID_MA8)) {
							stream->read(ma8Data, stream->size());
						}
					} else {
						if (chunk.isSection(ID_BIN)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_MA8)) {
							stream->skip(stream->size());
						}
					}
					break;
				case EX_BMP:
					/* currently does not handle the VQT: and OFF: chunks
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
					if (resource >= 0) {
						if (chunk.isSection(ID_BIN)) {
							uint16 outPitch = tw[resource]>>1;
							_binData = new Graphics::Surface();
							_binData->create(outPitch, th[resource], Graphics::PixelFormat::createFormatCLUT8());
							byte *data = (byte *)_binData->getPixels();

							stream->skip(_toffsets[resource]>>1);
							stream->read(data, uint32(outPitch)*th[resource]);
						} else if (chunk.isSection(ID_VGA)) {
							uint16 outPitch = tw[resource]>>1;
							_vgaData = new Graphics::Surface();
							_vgaData->create(outPitch, th[resource], Graphics::PixelFormat::createFormatCLUT8());
							byte *data = (byte *)_vgaData->getPixels();

							stream->skip(_toffsets[resource]>>1);
							stream->read(data, uint32(outPitch)*th[resource]);
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

	debug("  [%u:%u] --", file.pos(), ctx.outSize);
}

static void explode(Common::Platform platform, const char *indexName, const char *fileName, int resource) {
	Common::File index, volume;
	Common::SeekableSubReadStream *file;

	if (index.open(indexName)) {
		byte salt[4];
		uint16 nvolumes;

		index.read(salt, sizeof(salt));
		nvolumes = index.readUint16LE();

		if (resource == -1)
			debug("(%u,%u,%u,%u) %u", salt[0], salt[1], salt[2], salt[3], nvolumes);

		for (uint i=0; i<nvolumes; i++) {
			char name[DGDS_FILENAME_MAX+1];
			uint16 nfiles;

			index.read(name, sizeof(name));
			name[DGDS_FILENAME_MAX] = '\0';
			
			nfiles = index.readUint16LE();
			
			volume.open(name);

			if (resource == -1)
				debug("--\n#%u %s %u", i, name, nfiles);

			for (uint j=0; j<nfiles; j++) {
				uint32 hash, offset;
				uint32 fileSize;

				hash = index.readUint32LE();
				offset = index.readUint32LE();

				volume.seek(offset);
				volume.read(name, sizeof(name));
				name[DGDS_FILENAME_MAX] = '\0';
				fileSize = volume.readUint32LE();

				if (resource == -1 || scumm_stricmp(name, fileName) == 0)
					debug("  #%u %s %x=%x %u %u\n  --", j, name, hash, dgdsHash(name, salt), offset, fileSize);

				if (fileSize == 0xFFFFFFFF) {
					continue;
				}

				if (resource >= 0 && scumm_stricmp(name, fileName)) {
					volume.skip(fileSize);
					continue;
				}

				file = new Common::SeekableSubReadStream(&volume, volume.pos(), volume.pos()+fileSize, DisposeAfterUse::NO);

				if (resource == -1) {
					Common::DumpFile out;
					char *buf;

					buf = new char[fileSize];

					if (!out.open(name)) {
						debug("Couldn't write to %s", name);
					} else {
						file->read(buf, fileSize);
						out.write(buf, fileSize);
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

				parseFile(platform, _ex, *file, name, resource);

				if (resource >= 0) {
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
uint32 bw = 0, bh = 0;
int bk = 0;

char bmpNames[16][DGDS_FILENAME_MAX+1];
char scrNames[16][DGDS_FILENAME_MAX+1];
int id = 0, sid = 0;

void interpret(Common::Platform platform, const char *rootName, DgdsEngine* syst) {
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
		Common::String txt;
		txt += Common::String::format("OP: 0x%4.4x %2u ", op, count);
		if (count == 0x0F) {
			byte ch[2];

			do {
				ch[0] = ttm->readByte();
				ch[1] = ttm->readByte();
				sval += ch[0];
				sval += ch[1];
			} while (ch[0] != 0 && ch[1] != 0);

			debugN("\"%s\"", sval.c_str());
			txt += "\"" + sval + "\"";
		} else {
			int16 ival;

			for (byte k=0; k<count; k++) {
				ival = ttm->readSint16LE();
				ivals[k] = ival;

				if (k == 0) {
					debugN("%d", ival);
					txt += Common::String::format("%d", ival);
				} else {
					debugN(", %d", ival);
					txt += Common::String::format(", %d", ival);
				}
			}
		}
		debug(" ");

		byte *vgaData_;
		byte *binData_;
		byte *ma8Data_;
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
				explode(platform, rootName, sval.c_str(), 0);
				g_system->getPaletteManager()->setPalette(palette, 0, 256);
				break;
			case 0xf060: {
				if (platform == Common::kPlatformAmiga) {
					// LOAD SONG
					const char *fileName = "DYNAMIX.INS";
					byte volume = 255;
					byte channel = 0;
					syst->stopSfx(channel);
					syst->playSfx(fileName, channel, volume);
					}
				}
				break;

			case 0x10a0:
				// SET SCR?
				explode(platform, rootName, scrNames[sid], 0);

				vgaData_ = vgaData;
				binData_ = binData;
				for (int i=0; i<sw*sh; i+=2) {
					imgData[i+0]  = ((vgaData_[i/2] & 0xF0)     );
					imgData[i+0] |= ((binData_[i/2] & 0xF0) >> 4);
					imgData[i+1]  = ((vgaData_[i/2] & 0x0F) << 4);
					imgData[i+1] |= ((binData_[i/2] & 0x0F)     );
				}
				/*
				ma8Data_ = ma8Data;
				for (int i=0; i<sw*sh; i++) {
					imgData[i] = ma8Data_[i];
				}
				*/
				g_system->copyRectToScreen(imgData, sw, 0, 0, sw, sh);
				g_system->updateScreen();
				break;

			case 0x1030:
				// SET BMP?
				bk = ivals[0];

				delete _binData;
				delete _vgaData;
				explode(platform, rootName, bmpNames[id], bk);

				bw = _tw[bk]; bh = _th[bk];

				vgaData_ = (byte *)_vgaData->getPixels();
				binData_ = (byte *)_binData->getPixels();
				for (uint32 i=0; i<bw*bh; i+=2) {
					_imgData[i+0]  = ((vgaData_[i>>1] & 0xF0)     );
					_imgData[i+0] |= ((binData_[i>>1] & 0xF0) >> 4);
					_imgData[i+1]  = ((vgaData_[i>>1] & 0x0F) << 4);
					_imgData[i+1] |= ((binData_[i>>1] & 0x0F)     );
				}
				break;

			case 0x1050:
				// SELECT BMP
				id = ivals[0];
				break;
			case 0x1060:
				// SELECT SCR|PAL
				sid = ivals[0];
				break;
			case 0x1090:
				// SELECT SONG
				break;

			case 0x4120:
				// FADE IN?
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

			case 0xa530:
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

		const Graphics::Font *font = FontMan.getFontByUsage(Graphics::FontManager::kConsoleFont);
		int w = font->getStringWidth(txt);
		/*
		int x = sh - w - 5; // lx + logo->w - w + 5;
		int y = sh - font->getFontHeight() - 5; //ly + logo->h + 5;*/

		dst = g_system->lockScreen();
		Common::Rect r(0, 7, sw, font->getFontHeight()+13);
		dst->fillRect(r, 0);
		font->drawString(dst, txt, 10, 10, w, 2);
		g_system->unlockScreen();
		break;
	}
}

struct Channel {
	Audio::AudioStream *stream;
	Audio::SoundHandle handle;
	byte volume;
};

struct Channel _channels[2];

void DgdsEngine::playSfx(const char* fileName, byte channel, byte volume) {
	soundData = 0;
	explode(_platform, _rmfName, fileName, 0);
	if (soundData) {
		Channel *ch = &_channels[channel];
		Audio::AudioStream *input = Audio::makeAIFFStream(soundData, DisposeAfterUse::YES);
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &ch->handle, input, -1, volume);
	}
}

void DgdsEngine::stopSfx(byte channel) {
	if (_mixer->isSoundHandleActive(_channels[channel].handle)) {
	    _mixer->stopHandle(_channels[channel].handle);
		_channels[channel].stream = 0;
	}
}

Common::Error DgdsEngine::run() {
	initGraphics(320, 200);

	soundData = 0;

	memset(palette, 1, 256*3);

	imgData = new byte[320*200];
	_imgData = new byte[320*200];
	memset(imgData, 0, 320*200);
	memset(_imgData, 0, 320*200);
	ttm = 0;

	debug("DgdsEngine::init");

	// Rise of the Dragon.
/*
	explode(_platform, _rmfName, "TITLE1.TTM", 0);
	explode(_platform, _rmfName, "DYNAMIX.SNG", 0);*/
/*
	explode(_platform, _rmfName, 0, -1);
	return Common::kNoError;
*/
	g_system->fillScreen(0);

	// grayscale palette.
/*
	for (uint i=0; i<256; i++) {
	    palette[i*3+0] = 255-i;
	    palette[i*3+1] = 255-i;
	    palette[i*3+2] = 255-i;
	}
*/
/*
	// Amiga grayscale palette.
	for (uint i=0; i<32; i++) {
	    palette[i*3+0] = 255-i*8;
	    palette[i*3+1] = 255-i*8;
	    palette[i*3+2] = 255-i*8;
	}
	g_system->getPaletteManager()->setPalette(palette, 0, 256);
*/
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
//		    explode(_platform, _rmfName, "TITLE.TTM");
		    if ((k%2) == 0)
			explode(_platform, _rmfName, "TITLE1.TTM", 0);
		    else
			explode(_platform, _rmfName, "TITLE2.TTM", 0);
		    k++;
		}
		interpret(_platform, _rmfName, this);
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

