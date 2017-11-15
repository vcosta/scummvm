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

#include "common/config-manager.h"
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
#include "audio/midiparser.h"
#include "audio/midiplayer.h"
#include "audio/decoders/aiff.h"

#include "graphics/palette.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
#include "graphics/surface.h"

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
byte blacks[256*3];

Graphics::Surface binData;
Graphics::Surface vgaData;
Graphics::Surface ma8Data;

Graphics::Surface _binData;
Graphics::Surface _vgaData;
Graphics::Surface _bmpData;

Graphics::Surface bottomBuffer;
Graphics::Surface topBuffer;

Graphics::ManagedSurface resData;

Common::MemoryReadStream *soundData;
byte *musicData;
uint32 musicSize;

uint16 _tcount;
uint16 _tw, _th;
uint32 _toffset;

uint16 *_mtx;
uint16 _mw, _mh;

Common::SeekableReadStream* ttm;

int sw = 320, sh = 200;


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

#define MKTAG24(a0,a1,a2) ((uint32)((a2) | (a1) << 8 | ((a0) << 16)))

#define ID_BIN	MKTAG24('B','I','N')
#define ID_DAT	MKTAG24('D','A','T')
#define ID_FNM	MKTAG24('F','N','M')
#define ID_FNT	MKTAG24('F','N','T')
#define ID_GAD	MKTAG24('G','A','D')
#define ID_INF	MKTAG24('I','N','F')
#define ID_MTX	MKTAG24('M','T','X')
#define ID_PAG	MKTAG24('P','A','G')
#define ID_REQ	MKTAG24('R','E','Q')
#define ID_RES	MKTAG24('R','E','S')
#define ID_SCR	MKTAG24('S','C','R')
#define ID_SDS	MKTAG24('S','D','S')
#define ID_SNG	MKTAG24('S','N','G')
#define ID_TAG	MKTAG24('T','A','G')
#define ID_TT3	MKTAG24('T','T','3')
#define ID_VER	MKTAG24('V','E','R')
#define ID_VGA	MKTAG24('V','G','A')
#define ID_VQT	MKTAG24('V','Q','T')

/* Heart of China */
#define ID_MA8	MKTAG24('M','A','8')
#define ID_DDS	MKTAG24('D','D','S')
#define ID_THD	MKTAG24('T','H','D')

#define	EX_ADH	MKTAG24('A','D','H')
#define	EX_ADL	MKTAG24('A','D','L')
#define	EX_ADS	MKTAG24('A','D','S')
#define	EX_AMG	MKTAG24('A','M','G')
#define	EX_BMP	MKTAG24('B','M','P')
#define	EX_GDS	MKTAG24('G','D','S')
#define	EX_INS	MKTAG24('I','N','S')
#define	EX_PAL	MKTAG24('P','A','L')
#define	EX_FNT	MKTAG24('F','N','T')
#define	EX_REQ	MKTAG24('R','E','Q')
#define	EX_RST	MKTAG24('R','S','T')
#define	EX_SCR	MKTAG24('S','C','R')
#define	EX_SDS	MKTAG24('S','D','S')
#define	EX_SNG	MKTAG24('S','N','G')
#define	EX_SX	MKTAG24('S','X', 0 )
#define	EX_TTM	MKTAG24('T','T','M')
#define	EX_VIN	MKTAG24('V','I','N')

/* Heart of China */
#define	EX_DAT	MKTAG24('D','A','T')
#define	EX_DDS	MKTAG24('D','D','S')
#define	EX_TDS	MKTAG24('T','D','S')

#define	EX_OVL	MKTAG24('O','V','L')

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
		case EX_VIN:
		case EX_DAT:
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
	memset(type, 0, sizeof(type));
	_type = 0;

	if (file.pos() >= file.size()) {
		return false;
	}

	file.read(type, DGDS_TYPENAME_MAX);

	if (type[DGDS_TYPENAME_MAX-1] != ':') {
		debug("bad header in: %s", name);
		return false;
	}
	type[DGDS_TYPENAME_MAX] = '\0';
	_type = MKTAG24(uint32(type[0]), uint32(type[1]), uint32(type[2]));

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
	byte compression; /* 0=None, 1=RLE, 2=LZW */
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

void loadBitmap4(Graphics::Surface& surf, uint16 tw, uint16 th, uint32 toffset, Common::SeekableReadStream* stream) {
	uint16 outPitch = tw>>1;
	surf.create(outPitch, th, Graphics::PixelFormat::createFormatCLUT8());
	byte *data = (byte *)surf.getPixels();
	
	stream->skip(toffset>>1);
	stream->read(data, uint32(outPitch)*th);
}

void loadBitmap8(Graphics::Surface& surf, uint16 tw, uint16 th, uint32 toffset, Common::SeekableReadStream* stream) {
	uint16 outPitch = tw;
	surf.create(outPitch, th, Graphics::PixelFormat::createFormatCLUT8());
	byte *data = (byte *)surf.getPixels();

	stream->skip(toffset);
	stream->read(data, uint32(outPitch)*th);
}

class PFont {
protected:
	byte _w;
	byte _h;
	byte _start;
	byte _count;

	byte *_offsets;
	byte *_widths;
	byte *_data;

public:
	static PFont *load_PFont(Common::SeekableReadStream &input);
};

PFont *PFont::load_PFont(Common::SeekableReadStream &input) {
	byte magic;

	magic = input.readByte();
	assert(magic == 0xFF);

	byte w, h;
	byte unknown, start, count, compression;
	int size;
	int uncompressedSize;

	w = input.readByte();
	h = input.readByte();
	unknown = input.readByte();
	start = input.readByte();
	count = input.readByte();
	size = input.readUint16LE();
	compression = input.readByte();
	uncompressedSize = input.readUint32LE();
	debug("    magic: 0x%x, w: %u, h: %u, unknown: 0x%x, start: 0x%x, count: %u\n"
	      "    size: %u, compression: 0x%x, uncompressedSize: %u",
			magic, w, h, unknown, start, count,
			size, compression, uncompressedSize);
	assert(uncompressedSize == size);

	size = input.size()-input.pos();

	byte *data = new byte[uncompressedSize];
	switch (compression) {
	case 0x00: {
		   input.read(data, size);
		   break;
		   }
	case 0x01: {
		   RleDecompressor dec;
		   dec.decompress(data, uncompressedSize, input);
		   break;
		   }
	case 0x02: {
		   LzwDecompressor dec;
		   dec.decompress(data, uncompressedSize, input);
		   break;
		   }
	default:
		   input.skip(size);
		   debug("unknown chunk compression: 0x%x", compression);
		   break;
	}

	PFont* fnt = new PFont;
	fnt->_w = w;
	fnt->_h = h;
	fnt->_start = start;
	fnt->_count = count;

	fnt->_offsets = data;
	fnt->_widths = data+2*count;
	fnt->_data = data+3*count;
	return fnt;
}


class Font : public Graphics::Font {
protected:
	byte _w;
	byte _h;
	byte _start;
	byte _count;

	byte *_data;

	void mapChar(byte chr, int& pos, int& bit) const;

public:
	int getFontHeight() const;
	int getMaxCharWidth() const;
	int getCharWidth(uint32 chr) const;
	void drawChar(Graphics::Surface* dst, uint32 chr, int x, int y, uint32 color) const;

	static Font *load_Font(Common::SeekableReadStream &input);
};

int Font::getFontHeight() const {
	return _h;
}

int Font::getMaxCharWidth() const {
	return _w;
}

int Font::getCharWidth(uint32 chr) const {
	return _w;
}

static inline uint
isSet(byte *set, uint bit)
{
	return (set[(bit >> 3)] & (1 << (bit & 7)));
}

void Font::mapChar(byte chr, int& pos, int& bit) const {
	pos = (chr-_start)*_h;
	bit = 8-_w;
}

void Font::drawChar(Graphics::Surface* dst, uint32 chr, int x, int y, uint32 color) const {
	const Common::Rect destRect(x, y, x+_w, y+_h);
	Common::Rect clippedDestRect(0, 0, dst->w, dst->h);
	clippedDestRect.clip(destRect);

	const Common::Point croppedBy(clippedDestRect.left-destRect.left, clippedDestRect.top-destRect.top);

	const int rows = clippedDestRect.height();
	const int columns = clippedDestRect.width();

	int pos, bit;
	mapChar(chr, pos, bit);
	int idx = bit + croppedBy.x;
	byte *src = _data + pos + croppedBy.y;
	byte *ptr = (byte *)dst->getBasePtr(clippedDestRect.left, clippedDestRect.top);
	for (int i=0; i<rows; ++i) {
		for (int j=0; j<columns; ++j) {
			if (isSet(src, idx+_w-1-j))
				ptr[j] = color;
		}
		ptr += dst->pitch;
		src++;
	}
}

PFont *_fntP;
Font *_fntF;

Font *Font::load_Font(Common::SeekableReadStream &input) {
	byte w, h, start, count;
	w = input.readByte();
	h = input.readByte();
	start = input.readByte();
	count = input.readByte();

	int size = h*count;
	debug("    w: %u, h: %u, start: 0x%x, count: %u", w, h, start, count);
	assert((4+size) == input.size());

	Font* fnt = new Font;
	fnt->_w = w;
	fnt->_h = h;
	fnt->_start = start;
	fnt->_count = count;
	fnt->_data = new byte[size];
	input.read(fnt->_data, size);
	return fnt;
}

void parseFile(Common::Platform platform, Common::SeekableReadStream& file, const char* name, int resource) {
	struct DgdsFileCtx ctx;

	const char *dot;
	DGDS_EX _ex;

	if ((dot = strrchr(name, '.'))) {
		_ex = MKTAG24(dot[1], dot[2], dot[3]);
	} else {
		_ex = 0;
	}

	uint parent = 0;

	ctx.init(file.size());

	if (isFlatfile(platform, _ex)) {
		uint16 tcount;
		uint16 *tw, *th;
		uint32 *toffset;
		Common::String line;
		
		switch (_ex) {
			case EX_RST: {
				uint32 mark;

				mark = file.readUint32LE();
				debug("    0x%X", mark);

				// elaborate guesswork. who knows it might be true.
				while (!file.eos()) {
					uint16 idx;
					uint16 vals[7];

					idx = file.readUint16LE();
					debugN("  #%u:\t", idx);
					if (idx == 0) break;
					for (int i=0; i<ARRAYSIZE(vals); i++) {
						vals[i] = file.readUint16LE();
						if (i != 0) debugN(", ");
						debugN("%u", vals[i]);
					}
					debug(".");
				}
				debug("-");

				while (!file.eos()) {
					uint16 idx;
					uint16 vals[2];
					idx = file.readUint16LE();
					debugN("  #%u:\t", idx);
					for (int i=0; i<ARRAYSIZE(vals); i++) {
						vals[i] = file.readUint16LE();
						if (i != 0) debugN(", ");
						debugN("%u", vals[i]);
					}
					debug(".");
					if (idx == 0) break;
				}
				debug("-");
				}
				break;
			case EX_SCR: {
				/* Unknown image format (Amiga). */
				byte tag[5];
				file.read(tag, 4);		/* maybe */
				tag[4] = '\0';

				uint16 pitch, planes;
				pitch = file.readUint16BE();	/* always 200 (320x200 screen). */
				planes = file.readUint16BE();	/* always 5 (32 color). */

				debug("    \"%s\" pitch:%u bpp:%u size:~%u",
						tag, pitch, planes,
						uint(320+15)/16*200*planes);

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
				toffset = new uint32[tcount];
				for (uint16 k=0; k<tcount; k++) {
					tw[k] = file.readUint16BE();
					th[k] = file.readUint16BE();
					debug("        %ux%u ~@%u", tw[k], th[k], sz);

					toffset[k] = sz;
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
				    _tw = tw[resource];
				    _th = th[resource];
				    _toffset = toffset[resource];
				}
				}
				break;
			case EX_INS: {
				/* AIFF sound sample (Amiga). */
			        byte *dest = new byte[file.size()];
			        file.read(dest, file.size());
			        soundData = new Common::MemoryReadStream(dest, file.size(), DisposeAfterUse::YES);
				}
				break;
			case EX_SNG:
				/* IFF-SMUS music (Amiga). */
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
				line = file.readLine();
				while (!file.eos()) {
					if (!line.empty()) debug("    \"%s\"", line.c_str());
					line = file.readLine();
				}
				break;
			default:
				break;
		}
		int leftover = file.size()-file.pos();
		file.hexdump(leftover);
		file.skip(leftover);
	} else {
		uint16 tcount;
		uint16 scount;
		uint16 *tw = 0, *th = 0;
		uint32 *toffset = 0;
		
		uint16 *mtx;
		uint16 mw, mh;

		scount = 0;

		struct DgdsChunk chunk;
		while (chunk.readHeader(ctx, file, name)) {
			if (chunk.container) {
				parent = chunk._type;
				continue;
			}

			Common::SeekableReadStream *stream;
			
			bool packed = chunk.isPacked(_ex);
			stream = packed ? chunk.decode(ctx, file) : chunk.copy(ctx, file);
			if (stream) ctx.outSize += stream->size();
			
			/*
			 debug(">> %u:%u", stream->pos(), file->pos());*/

			Common::DumpFile out;

			if (stream) {
				uint siz = stream->size();
				byte *dest = new byte[siz];
				Common::String cname = Common::String::format("%s:%s", name, chunk.type);

				if (!out.open(cname)) {
					debug("Couldn't write to %s", cname.c_str());
				} else {
					stream->read(dest, siz);
					out.write(dest, siz);
					stream->seek(0);
					out.close();
				}
			}

			switch (_ex) {
				case EX_TDS:
					/* Heart of China. */
					if (chunk.isSection(ID_THD)) {
						uint32 mark;

						mark = stream->readUint32LE();
						debug("    0x%X", mark);

						char version[7];
						stream->read(version, sizeof(version));
						debug("    \"%s\"", version);

						byte ch;
						Common::String bmpName;
						while ((ch = stream->readByte()))
							bmpName += ch;
						debug("    \"%s\"", bmpName.c_str());

						Common::String personName;
						while ((ch = stream->readByte()))
							personName += ch;
						debug("    \"%s\"", personName.c_str());
					}
					break;
				case EX_DDS:
					/* Heart of China. */
					if (chunk.isSection(ID_DDS)) {
						uint32 mark;

						mark = stream->readUint32LE();
						debug("    0x%X", mark);

						char version[7];
						stream->read(version, sizeof(version));
						debug("    \"%s\"", version);

						byte ch;
						Common::String tag;
						while ((ch = stream->readByte()))
							tag += ch;
						debug("    \"%s\"", tag.c_str());
					}
					break;
				case EX_SDS:
					if (chunk.isSection(ID_SDS)) {
						uint32 mark;

						mark = stream->readUint32LE();
						debug("    0x%X", mark);

						char version[7];
						stream->read(version, sizeof(version));
						debug("    %s", version);

						uint16 idx;
						idx = stream->readUint16LE();
						debug("    S%d.SDS", idx);
#if 0
						idx = stream->readUint16LE();
						debug("    %d", idx);

						idx = stream->readUint16LE();
						debug("    %d", idx);

						uint16 count;
						while (1) {
							uint16 code;
							code = stream->readUint16LE();
							count = stream->readUint16LE();
							idx = stream->readUint16LE();

							debugN("\tOP: 0x%8.8x %2u %2u\n", code, count, idx);

							uint16 pitch = (count+1)&(~1); // align to word.
							if ((stream->pos()+pitch) >= stream->size()) break;

							if (code == 0 && count == 0) break;

							stream->skip(pitch);
						}

						Common::String sval;
						byte ch;

						do {
							ch = stream->readByte();
							sval += ch;
						} while (ch != 0);

						debug("\"%s\"", sval.c_str());
							/*
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
*/
#endif
#if 0
						// probe for the .ADS name. are these shorts?
						uint count;
						count = 0;
						while (1) {
							uint16 x;
							x = stream->readUint16LE();
							if ((x & 0xFF00) != 0)
								break;
							debug("      %u: %u|0x%4.4X", count++, x, x);
						}
						stream->seek(-2, SEEK_CUR);

						// .ADS name.
						Common::String ads;
						byte ch;
						while ((ch = stream->readByte()))
							ads += ch;
						debug("    %s", ads.c_str());

						stream->hexdump(6);
						stream->skip(6);

						int w, h;

						w = stream->readSint16LE();
						h = stream->readSint16LE();
						debug("    %dx%d", w, h);

						// probe for the strings. are these shorts?
						count = 0;
						while (1) {
							uint16 x;
							x = stream->readUint16LE();
							if ((x & 0xFF00) != 0)
								break;
							if (stream->pos() >= stream->size()) break;
							debug("      %u: %u|0x%4.4X", count++, x, x);
						}
						stream->seek(-4, SEEK_CUR);
						// here we are.

						uint16 len;
						len = stream->readSint16LE();
						Common::String txt;
						for (uint16 j=0; j<len; j++) {
							ch = stream->readByte();
							txt += ch;
							debug("      \"%s\"", txt.c_str());
						}
#endif
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
						stream->hexdump(stream->size());
						uint32 mark;
						char version[7];

						mark = stream->readUint32LE();
						debug("    0x%X", mark);

						stream->read(version, sizeof(version));
						debug("    \"%s\"", version);
						
					} else if (chunk.isSection(ID_SDS)) {
						stream->hexdump(stream->size());

						uint32 x;
						x = stream->readUint32LE();
						debug("    %u", x);

						while (!stream->eos()) {
							uint16 x2;
							do {
								do {
									x2 = stream->readUint16LE();
									debugN("        %u: %u|%u, ", x2, (x2&0xF), (x2>>4));
									if (stream->pos() >= stream->size()) break;
								} while ((x2 & 0x80) != 0x80);
								debug("-");
								if (stream->pos() >= stream->size()) break;
							} while ((x2 & 0xF0) != 0xF0);
						}
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
						/* this is either a script, or a property sheet, i can't decide. */
						while (!stream->eos()) {
							uint16 code;
							code = stream->readUint16LE();
							if ((code&0xFF00) == 0) {
								uint16 tag = (code&0xFF);
								debug("          PUSH %d (0x%4.4X)", tag, tag); // ADS:TAG or TTM:TAG id.
							} else {
								const char *desc = "";
								switch (code) {
									case 0xF010:
									case 0xF200:
									case 0xFDA8:
									case 0xFE98:
									case 0xFF88:
									case 0xFF10:
										debug("          INT 0x%4.4X\t;", code);
										continue;

									case 0xFFFF:
										debug("          INT 0x%4.4X\t; return", code);
										debug("-");
										continue;

									case 0x0190:
									case 0x1070:
									case 0x1340:
									case 0x1360:
									case 0x1370:
									case 0x1420:
									case 0x1430:
									case 0x1500:
									case 0x1520:
									case 0x2000:
									case 0x2010:
									case 0x2020:
									case 0x3010:
									case 0x3020:
									case 0x30FF:
									case 0x4000:
									case 0x4010:	desc = "?";			break;

									case 0x1330:					break;
									case 0x1350:	desc = "? (res,rtag)";		break;

									case 0x1510:	desc = "? ()";			break;
									case 0x2005:	desc = "? (res,rtag,?,?)";	break;

									default:
										break;
								}
								debug("          OP 0x%4.4X\t;%s", code, desc);
							}
						}
						assert(stream->size()==stream->pos());
						stream->hexdump(stream->size());
					} else if (chunk.isSection(ID_TAG)) {
						readStrings(stream);
					}
					break;
				case EX_REQ:
					if (parent == ID_TAG) {
						if (chunk.isSection(ID_REQ)) {
							readStrings(stream);
						} else if (chunk.isSection(ID_GAD)) {
							readStrings(stream);
						}
					} else if (parent == ID_REQ) {
						if (chunk.isSection(ID_REQ)) {
						} else if (chunk.isSection(ID_GAD)) {
						}
					}
					break;
				case EX_SNG:
					/* DOS. */
					if (chunk.isSection(ID_SNG)) {
						musicSize = stream->size();
						musicData = (uint8 *)malloc(musicSize);

						debug("        %2u: %u bytes", scount, musicSize);
						stream->read(musicData, musicSize);
						scount++;
					} else if (chunk.isSection(ID_INF)) {
						uint32 count;
						count = stream->size()/2;
						debug("        [%u]", count);
						for (uint32 k = 0; k < count; k++) {
							uint16 idx;
							idx = stream->readUint16LE();
							debug("        %2u: %u", k, idx);
						}
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
							debug("        %2u: %u", k, idx);
						}
					} else if (chunk.isSection(ID_TAG)) {
						readStrings(stream);
					} else if (chunk.isSection(ID_FNM)) {
						readStrings(stream);
					} else if (chunk.isSection(ID_DAT)) {
						uint16 idx;
						idx = stream->readUint16LE();
						debug("        %2u", idx);
						/*
						 uint unpackSize = 1000;
						 byte *dest = new byte[unpackSize];
						 RleDecompressor dec;
						 dec.decompress(dest,unpackSize,file);
						 ostream = new Common::MemoryReadStream(dest, unpackSize, DisposeAfterUse::YES);*/
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
				case EX_FNT:
					if (resource == 0) {
						if (chunk.isSection(ID_FNT)) {
							byte magic = stream->readByte();
							stream->seek(-1, SEEK_CUR);
							debug("    magic: %u", magic);

							if (magic != 0xFF)
								_fntF = Font::load_Font(*stream);
							else
								_fntP = PFont::load_PFont(*stream);
						}
					}
					break;
				case EX_SCR:
					/* currently does not handle the VQT: and OFF: chunks
					 for the compressed pics in the DOS port. */
					if (resource == 0) {
						if (chunk.isSection(ID_BIN)) {
							loadBitmap4(binData, 320, 200, 0, stream);
						} else if (chunk.isSection(ID_VGA)) {
							loadBitmap4(vgaData, 320, 200, 0, stream);
						} else if (chunk.isSection(ID_MA8)) {
							loadBitmap8(ma8Data, 320, 200, 0, stream);
						} else if (chunk.isSection(ID_VQT)) {
							stream->skip(stream->size());
						}
					} else {
						if (chunk.isSection(ID_BIN)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_VGA)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_MA8)) {
							stream->skip(stream->size());
						} else if (chunk.isSection(ID_VQT)) {
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
						toffset = new uint32[tcount];
						for (uint16 k=0; k<tcount; k++) {
							debug("        %ux%u @%u", tw[k], th[k], sz);
							toffset[k] = sz;
							sz += uint32(tw[k])*th[k];
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
							loadBitmap4(_binData, tw[resource], th[resource], toffset[resource], stream);
						} else if (chunk.isSection(ID_VGA)) {
							loadBitmap4(_vgaData, tw[resource], th[resource], toffset[resource], stream);
						} else if (chunk.isSection(ID_INF)) {
							_tcount = tcount;
							_tw = tw[resource];
							_th = th[resource];
							_toffset = toffset[resource];
						} else if (chunk.isSection(ID_MTX)) {
							_mtx = mtx;
							_mw = mw;
							_mh = mh;
						}
					}
					if (chunk.isSection(ID_BIN)) {
						stream->skip(stream->size()-stream->pos());
					} else if (chunk.isSection(ID_VGA)) {
						stream->skip(stream->size()-stream->pos());
					} else if (chunk.isSection(ID_VQT)) {
						stream->skip(stream->size()-stream->pos());
					}
					break;
				default:
					break;
			}
			int leftover = stream->size()-stream->pos();
			stream->hexdump(leftover);
			stream->skip(leftover);
			
			delete stream;
		}
	}
#if 0
	if (_ex == EX_PAL) {
		Common::DumpFile out;

		Common::String cname = Common::String::format("%s.gpl", name);

		if (!out.open(cname)) {
			debug("Couldn't write to %s", cname.c_str());
		} else {
			Common::String header = Common::String::format("GIMP Palette\nName: %s\nColumns: 0\n#\n", name);
			out.writeString(header);
			for (uint i=0; i<256*3; i+=3) {
				Common::String color = Common::String::format("%3u %3u %3u\tUntitled\n",
						palette[i+0], palette[i+1], palette[i+2]);
				out.writeString(color);
			}
			out.close();
		}
		
	} else if (_ex == EX_SCR) {
		byte *vgaData_;
		byte *binData_;
		byte *ma8Data_;

		byte *scrData_;
		scrData_ = (byte *)bottomBuffer.getPixels();

		if (ma8Data.h != 0) {
			ma8Data_ = (byte *)ma8Data.getPixels();
			for (int i=0; i<sw*sh; i++) {
				scrData_[i] = ma8Data_[i];
			}
		} else if (vgaData.h != 0) {
			vgaData_ = (byte *)vgaData.getPixels();
			binData_ = (byte *)binData.getPixels();
			for (int i=0; i<sw*sh; i+=2) {
				scrData_[i+0]  = ((vgaData_[i>>1] & 0xF0)     );
				scrData_[i+0] |= ((binData_[i>>1] & 0xF0) >> 4);
				scrData_[i+1]  = ((vgaData_[i>>1] & 0x0F) << 4);
				scrData_[i+1] |= ((binData_[i>>1] & 0x0F)     );
			}
		}

		Common::DumpFile out;

		uint siz = sw*sh;
		Common::String cname = Common::String::format("%s.data", name);

		if (!out.open(cname)) {
			debug("Couldn't write to %s", cname.c_str());
		} else {
			out.write(scrData_, siz);
			out.close();
		}
	}
#endif
	debug("  [%u:%u] --", file.pos(), ctx.outSize);
}

static void explode(Common::Platform platform, const char *indexName, const char *fileName, int resource) {
	Common::File index, volume;
	Common::SeekableSubReadStream *file;
/*
	if (fileName && volume.open(fileName)) {
		parseFile(platform, volume, fileName, resource);
		volume.close();
		return;
	}
*/
	if (index.open(indexName)) {
		byte salt[4];
		uint16 nvolumes;

		index.read(salt, sizeof(salt));
		nvolumes = index.readUint16LE();

		if (!fileName)
			debug("(%u,%u,%u,%u) %u", salt[0], salt[1], salt[2], salt[3], nvolumes);

		for (uint i=0; i<nvolumes; i++) {
			char name[DGDS_FILENAME_MAX+1];
			uint16 nfiles;

			index.read(name, sizeof(name));
			name[DGDS_FILENAME_MAX] = '\0';
			
			nfiles = index.readUint16LE();
			
			debugN("--\n#%u %s, %u files", i, name, nfiles);

			if (!volume.open(name)) {
				debug(", failed to open");
				continue;
			}

			debug(", %d bytes", volume.size());

			for (uint j=0; j<nfiles; j++) {
				uint32 hash, offset;
				uint32 fileSize;

				hash = index.readUint32LE();
				offset = index.readUint32LE();

				volume.seek(offset);
				volume.read(name, sizeof(name));
				name[DGDS_FILENAME_MAX] = '\0';
				fileSize = volume.readUint32LE();

				if (!fileName || scumm_stricmp(name, fileName) == 0)
					debug("  #%u %s %x=%x %u %u\n  --", j, name, hash, dgdsHash(name, salt), offset, fileSize);

				if (fileSize == 0xFFFFFFFF) {
					continue;
				}

				if (fileName && scumm_stricmp(name, fileName)) {
					volume.skip(fileSize);
					continue;
				}

				file = new Common::SeekableSubReadStream(&volume, volume.pos(), volume.pos()+fileSize, DisposeAfterUse::NO);

				if (!fileName) {
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
				
				parseFile(platform, *file, name, resource);

				if (!fileName)
					debug("  #%u %s %d .", j, name, volume.pos());

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

int bw = 0, bh = 0;
int bk = -1;

char bmpNames[16][DGDS_FILENAME_MAX+1];
char scrNames[16][DGDS_FILENAME_MAX+1];
int id = 0, sid = 0;
const Common::Rect rect(0, 0, sw, sh);

int delay = 0;
Common::Rect drawWin(0, 0, sw, sh);

void interpret(Common::Platform platform, const char *rootName, DgdsEngine* syst) {
	if (!ttm) return;

	while (!ttm->eos()) {
		uint16 code;
		byte count;
		uint op;
		int16 ivals[8];

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

		byte *scrData_;
		byte *_bmpData_;

		scrData_ = (byte *)bottomBuffer.getPixels();
		_bmpData_ = (byte *)_bmpData.getPixels();

		Common::Rect bmpWin(0, 0, sw, sh);

		switch (op) {
			case 0x0000:
				// FINISH:	void
				goto EXIT;

			case 0xf010:
				// LOAD SCR:	filename:str
				Common::strlcpy(scrNames[sid], sval.c_str(), sizeof(scrNames[sid]));
				vgaData.free();
				binData.free();
				ma8Data.free();
				explode(platform, rootName, scrNames[sid], 0);

				if (ma8Data.h != 0) {
					ma8Data_ = (byte *)ma8Data.getPixels();
					for (int i=0; i<sw*sh; i++) {
						scrData_[i] = ma8Data_[i];
					}
				} else if (vgaData.h != 0) {
					vgaData_ = (byte *)vgaData.getPixels();
					binData_ = (byte *)binData.getPixels();
					for (int i=0; i<sw*sh; i+=2) {
						scrData_[i+0]  = ((vgaData_[i>>1] & 0xF0)     );
						scrData_[i+0] |= ((binData_[i>>1] & 0xF0) >> 4);
						scrData_[i+1]  = ((vgaData_[i>>1] & 0x0F) << 4);
						scrData_[i+1] |= ((binData_[i>>1] & 0x0F)     );
					}
				}
//				resData.copyRectToSurface(scrData, 0, 0, rect);
				break;
			case 0xf020:
				// LOAD BMP:	filename:str
				Common::strlcpy(bmpNames[id], sval.c_str(), sizeof(bmpNames[id]));
				break;
			case 0xf050:
				// LOAD PAL:	filename:str
				explode(platform, rootName, sval.c_str(), 0);
				break;
			case 0xf060:
				// LOAD SONG:	filename:str
				if (platform == Common::kPlatformAmiga) {
					const char *fileName = "DYNAMIX.INS";
					byte volume = 255;
					byte channel = 0;
					syst->stopSfx(channel);
					syst->playSfx(fileName, channel, volume);
				} else {
//					syst->playMusic(sval.c_str());
				}
				break;

			case 0x1030:
				// SET BMP:	id:int [-1:n]
				bk = ivals[0];

				_vgaData.free();
				_binData.free();
				if (bk != -1) {
					explode(platform, rootName, bmpNames[id], bk);

					if (_vgaData.h != 0) {
						bw = _tw; bh = _th;
						vgaData_ = (byte *)_vgaData.getPixels();
						binData_ = (byte *)_binData.getPixels();
						for (int i=0; i<bw*bh; i+=2) {
							_bmpData_[i+0]  = ((vgaData_[i>>1] & 0xF0)     );
							_bmpData_[i+0] |= ((binData_[i>>1] & 0xF0) >> 4);
							_bmpData_[i+1]  = ((vgaData_[i>>1] & 0x0F) << 4);
							_bmpData_[i+1] |= ((binData_[i>>1] & 0x0F)     );
						}
					}
				}
				break;
			case 0x1050:
				// SELECT BMP:	    id:int [0:n]
				id = ivals[0];
				break;
			case 0x1060:
				// SELECT SCR|PAL:  id:int [0]
				sid = ivals[0];
				break;
			case 0x1090:
				// SELECT SONG:	    id:int [0]
				break;

			case 0x4120:
				// FADE IN:	?,?,?,?:byte
				g_system->getPaletteManager()->setPalette(palette, 0, 256);
				break;

			case 0x4110:
				// FADE OUT:	?,?,?,?:byte
				g_system->delayMillis(delay);
				g_system->getPaletteManager()->setPalette(blacks, 0, 256);
				bottomBuffer.fillRect(rect, 0);
				break;

			// these 3 ops do interaction between the topBuffer (imgData) and the bottomBuffer (scrData) but... it might turn out this uses z values!
			case 0xa050: {//GFX?	    i,j,k,l:int	[i<k,j<l] // HAPPENS IN INTRO.TTM:INTRO9
				// it works like a bitblit, but it doesn't write if there's something already at the destination?
				resData.blitFrom(bottomBuffer);
				resData.transBlitFrom(topBuffer);
				topBuffer.copyFrom(resData);
				break;
				}
			case 0x0020: {//SAVE BG?:    void // OR PERHAPS SWAPBUFFERS ; it makes bmpData persist in the next frames.
				bottomBuffer.copyFrom(topBuffer);
				}
				break;
			case 0x4200: {
				// STORE AREA:	x,y,w,h:int [0..n]		; it makes this area of bmpData persist in the next frames.
				const Common::Rect destRect(ivals[0], ivals[1], ivals[0]+ivals[2], ivals[1]+ivals[3]);
				resData.blitFrom(bottomBuffer);
				resData.transBlitFrom(topBuffer);
				bottomBuffer.copyRectToSurface(resData, destRect.left, destRect.top, destRect);
				}
				break;

			case 0x0ff0: {
				// REFRESH:	void
				resData.blitFrom(bottomBuffer);
				Graphics::Surface bmpSub = topBuffer.getSubArea(bmpWin);
				resData.transBlitFrom(bmpSub, Common::Point(bmpWin.left, bmpWin.top));
				topBuffer.fillRect(bmpWin, 0);
				debug("FLUSH");
				}
				goto EXIT;

			case 0xa520:
				//DRAW BMP: x,y:int ; happens once in INTRO.TTM
			case 0xa500: {
				debug("DRAW \"%s\"", bmpNames[id]);

				// DRAW BMP: x,y,tile-id,bmp-id:int [-n,+n] (CHINA)
				// This is kind of file system intensive, will likely have to change to store all the BMPs.
				if (count == 4) {
					_vgaData.free();
					_binData.free();
					bk = ivals[2];
					id = ivals[3];
					if (bk != -1) {
						explode(platform, rootName, bmpNames[id], bk);

						if (_vgaData.h != 0) {
							bw = _tw; bh = _th;
							vgaData_ = (byte *)_vgaData.getPixels();
							binData_ = (byte *)_binData.getPixels();
							for (int i=0; i<bw*bh; i+=2) {
								_bmpData_[i+0]  = ((vgaData_[i>>1] & 0xF0)     );
								_bmpData_[i+0] |= ((binData_[i>>1] & 0xF0) >> 4);
								_bmpData_[i+1]  = ((vgaData_[i>>1] & 0x0F) << 4);
								_bmpData_[i+1] |= ((binData_[i>>1] & 0x0F)     );
							}
						}
					} else {
					    bw = bh = 0;
					}
				}

				// DRAW BMP: x,y:int [-n,+n] (RISE)
				const Common::Rect destRect(ivals[0], ivals[1], ivals[0]+bw, ivals[1]+bh);
				Common::Rect clippedDestRect(0, 0, sw, sh);
				clippedDestRect.clip(destRect);
				clippedDestRect.clip(drawWin);

				if (bk != -1) {
					// DRAW BMP: x,y:int [-n,+n] (RISE)
					const Common::Point croppedBy(clippedDestRect.left-destRect.left, clippedDestRect.top-destRect.top);

					const int rows = clippedDestRect.height();
					const int columns = clippedDestRect.width();

					byte *src = _bmpData_ + croppedBy.y * bw + croppedBy.x;
					byte *ptr = (byte *)topBuffer.getBasePtr(clippedDestRect.left, clippedDestRect.top);
					for (int i=0; i<rows; ++i) {
						for (int j=0; j<columns; ++j) {
							if (src[j])
								ptr[j] = src[j];
						}
						ptr += topBuffer.pitch;
						src += bw;
					}
				} else {
//					bmpData.fillRect(rect, 0);?
				}
				}
				break;

			case 0x1110: //SET SCENE?:  i:int   [1..n]
				// DESCRIPTION IN TTM TAGS.
				debug("SET SCENE: %u", ivals[0]);
				/*
				scrData.fillRect(rect, 0);
				bmpData.fillRect(rect, 0);*/
				break;

			case 0x4000:
				//SET WINDOW? x,y,w,h:int	[0..320,0..200]
				drawWin = Common::Rect(ivals[0], ivals[1], ivals[2], ivals[3]);
				break;

			case 0xa100:
				//SET WINDOW? x,y,w,h:int	[0..320,0..200]
				bmpWin = Common::Rect(ivals[0], ivals[1], ivals[0]+ivals[2], ivals[1]+ivals[3]);
				break;

			case 0x1020: //DELAY?:	    i:int   [0..n]
				delay = ivals[0]*10;
				break;

			case 0x10a0:
				// SET SCR|PAL:	    id:int [0]
			case 0x2000: //SET FRAME1?: i,j:int [0..255]

			case 0xa530:	// CHINA
				// DRAW BMP4:	x,y,tile-id,bmp-id:int	[-n,+n] (CHINA)
				// arguments similar to DRAW BMP but it draws the same BMP multiple times with radial simmetry? you can see this in the Dynamix logo star.
			case 0x0110: //PURGE IMGS?  void
			case 0x0080: //DRAW BG:	    void
			case 0x1100: //?	    i:int   [9]
			case 0x1300: //?	    i:int   [72,98,99,100,107]

			case 0x1310: //?	    i:int   [107]

			default:
				debug("        unimplemented opcode: 0x%04X", op);
				break;
		}

//		g_system->displayMessageOnOSD(txt.c_str());
#if 0
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
#endif
		break;
	}
EXIT:
	    Graphics::Surface *dst;
	    dst = g_system->lockScreen();
	    dst->copyRectToSurface(resData, 0, 0, rect);
	    g_system->unlockScreen();
	    g_system->updateScreen();

}

struct Channel {
	Audio::AudioStream *stream;
	Audio::SoundHandle handle;
	byte volume;
};

struct Channel _channels[2];

void DgdsEngine::playSfx(const char* fileName, byte channel, byte volume) {
	explode(_platform, _rmfName, fileName, 0);
	if (soundData) {
		Channel *ch = &_channels[channel];
		Audio::AudioStream *input = Audio::makeAIFFStream(soundData, DisposeAfterUse::YES);
		_mixer->playStream(Audio::Mixer::kSFXSoundType, &ch->handle, input, -1, volume);
		soundData = 0;
	}
}

void DgdsEngine::stopSfx(byte channel) {
	if (_mixer->isSoundHandleActive(_channels[channel].handle)) {
	    _mixer->stopHandle(_channels[channel].handle);
		_channels[channel].stream = 0;
	}
}

class DgdsMidiPlayer : public Audio::MidiPlayer {
public:
	DgdsMidiPlayer();

	void play(byte *data, uint32 size);
	void stop();
};

/**
 * The Standard MIDI File version of MidiParser.
 */
class MidiParser_DGDS : public MidiParser {
protected:
	byte *_init;
	byte *_last;

protected:
	void parseNextEvent(EventInfo &info);

public:
	MidiParser_DGDS();
	~MidiParser_DGDS();

	bool loadMusic(byte *data, uint32 size);
};

MidiParser_DGDS::MidiParser_DGDS() : _init(0), _last(0) {
}

MidiParser_DGDS::~MidiParser_DGDS() {
	free(_init);
}

void MidiParser_DGDS::parseNextEvent(EventInfo &info) {
	if (_position._playPos >= _last) {
		// fake an end-of-track meta event
		info.delta = 0;
		info.event = 0xFF;
		info.ext.type = 0x2F;
		info.length = 0;
		return;
	}

	info.start = _position._playPos;
	info.delta = 0;
	while (*_position._playPos == 0xF8) {
		info.delta += 240;
		_position._playPos++;
	}
	info.delta += *(_position._playPos++);

	// Process the next info.
	if ((_position._playPos[0] & 0xF0) >= 0x80)
		info.event = *(_position._playPos++);
	else
		info.event = _position._runningStatus;
	if (info.event < 0x80)
		return;

	_position._runningStatus = info.event;
	switch (info.command()) {
	case 0xC:
	case 0xD:
		info.basic.param1 = *(_position._playPos++);
		info.basic.param2 = 0;
		break;

	case 0xB:
		info.basic.param1 = *(_position._playPos++);
		info.basic.param2 = *(_position._playPos++);
		info.length = 0;
		break;

	case 0x8:
	case 0x9: 
	case 0xA:
	case 0xE:
		info.basic.param1 = *(_position._playPos++);
		info.basic.param2 = *(_position._playPos++);
		if (info.command() == 0x9 && info.basic.param2 == 0) {
			// NoteOn with param2==0 is a NoteOff
			info.event = info.channel() | 0x80;
		}
		info.length = 0;
		break;

	case 0xF: // System Common, Meta or SysEx event
		switch (info.event & 0x0F) {
		case 0x2: // Song Position Pointer
			info.basic.param1 = *(_position._playPos++);
			info.basic.param2 = *(_position._playPos++);
			break;

		case 0x3: // Song Select
			info.basic.param1 = *(_position._playPos++);
			info.basic.param2 = 0;
			break;

		case 0x6:
		case 0x8:
		case 0xA:
		case 0xB:
		case 0xC:
		case 0xE:
			info.basic.param1 = info.basic.param2 = 0;
			break;

		case 0x0: // SysEx
			info.length = readVLQ(_position._playPos);
			info.ext.data = _position._playPos;
			_position._playPos += info.length;
			break;

		case 0xF: // META event
			info.ext.type = *(_position._playPos++);
			info.length = readVLQ(_position._playPos);
			info.ext.data = _position._playPos;
			_position._playPos += info.length;
			break;

		default:
			warning("Unexpected midi event 0x%02X in midi data", info.event);
		}
	}
}

bool MidiParser_DGDS::loadMusic(byte *data, uint32 size) {
	unloadMusic();

	_init = data;
	_last = _init+size;

	uint32 len;
	uint32 totalSize;

	byte *pos = data;

        uint32 sci_header = 0;
	if (data[0] == 0x84 && data[1] == 0x00) sci_header = 2;

	pos += sci_header;
	if (pos[0] == 0xF0) {
	    debug("SysEx transfer = %d bytes", pos[1]);
	    pos += 2;
	    pos += 6;
	}

	_numTracks = 0;
	while (pos[0] != 0xFF) {
	    int idx = *pos++;

	    switch(idx) {
		case 0:	    debug("Adlib, Soundblaster");   break;
		case 7:	    debug("General MIDI");	    break;
		case 9:	    debug("CMS");		    break;
		case 12:    debug("MT-32");		    break;
		case 18:    debug("PC Speaker");	    break;
		case 19:    debug("Tandy 1000, PS/1");	    break;
		default:    debug("Unknown");		    break;
	    }

	    int tracksRead = 0;
	    totalSize = 0;
	    while (pos[0] != 0xFF) {
		pos++;

		if (pos[0] != 0) {
		    debug("%06ld: unknown track arg1 = %d", pos-data, pos[0]);
		}
		pos++;

		int off = read2low(pos) + sci_header;
		len = read2low(pos);
		totalSize += len;

		debug("%06d:%d", off, len);

		_tracks[_numTracks++] = data+off;

		if (_numTracks > ARRAYSIZE(_tracks)) {
		    warning("Can only handle %d tracks but was handed %d", (int)ARRAYSIZE(_tracks), _numTracks);
		    return false;
		}

		tracksRead++;
	    }

	    pos++;

	    debug("- Play parts = %d", tracksRead);
	}
	pos++;

	_ppqn = 1;
	setTempo(16667);

	totalSize = 0;
	int tracksRead = 0;
	while (tracksRead < _numTracks) {
		pos = _tracks[tracksRead];
		debug("Header bytes");

		debug("[%06d]  ", *pos);
		debug("[%02d]  ", *pos++);
		debug("[%02d]  ", *pos++);

		_tracks[tracksRead] += 2;

		totalSize += len;
		tracksRead++;
	}

	// Note that we assume the original data passed in
	// will persist beyond this call, i.e. we do NOT
	// copy the data to our own buffer. Take warning....
	resetTracking();
	setTrack(0);
	return true;
}

MidiParser *createParser_DGDS() { return new MidiParser_DGDS; }


DgdsMidiPlayer::DgdsMidiPlayer() {
	MidiPlayer::createDriver();

	int ret = _driver->open();
	if (ret == 0) {
		if (_nativeMT32)
			_driver->sendMT32Reset();
		else
			_driver->sendGMReset();
		_driver->setTimerCallback(this, &timerCallback);
	}
	debug("MidiPlayer()");
}

void DgdsMidiPlayer::play(byte *data, uint32 size) {
	Common::StackLock lock(_mutex);

	stop();
	if (!data) return;

	MidiParser *parser = createParser_DGDS();
	if (parser->loadMusic(data, size)) {
		parser->setTrack(0);
		parser->setMidiDriver(this);
		parser->setTimerRate(_driver->getBaseTempo());
		parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);

		_parser = parser;
		syncVolume();

		_isLooping = true;
		_isPlaying = true;
		debug("Playing music track");
	} else {
		debug("Cannot play music track");
		delete parser;
	}
}

void DgdsMidiPlayer::stop() {
	Audio::MidiPlayer::stop();
	debug("Stopping track");
}

void DgdsEngine::playMusic(const char* fileName) {
	//stopMusic();

	explode(_platform, _rmfName, fileName, 0);
	if (musicData) {
		_midiPlayer->play(musicData, musicSize);
	}
}

Common::Error DgdsEngine::run() {
	initGraphics(320, 200);

	soundData = 0;
	musicData = 0;

	_midiPlayer = new DgdsMidiPlayer();
	assert(_midiPlayer);

	memset(palette, 0, 256*3);
	memset(blacks, 0, 256*3);

	_bmpData.create(320, 200, Graphics::PixelFormat::createFormatCLUT8());

	bottomBuffer.create(320, 200, Graphics::PixelFormat::createFormatCLUT8());
	topBuffer.create(320, 200, Graphics::PixelFormat::createFormatCLUT8());
	resData.create(320, 200, Graphics::PixelFormat::createFormatCLUT8());

	ttm = 0;

	// Rise of the Dragon.
	debug("DgdsEngine::init");

	if (ConfMan.getBool("dump_scripts")) {
		explode(_platform, _rmfName, 0, -1);
		return Common::kNoError;
	}

	g_system->fillScreen(0);

	//playMusic("DYNAMIX.SNG");
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

//		    explode(_platform, _rmfName, "TVLMAP.TTM", 0);

		    switch ((k&3)) {
		    case 0:
			    explode(_platform, _rmfName, "TITLE1.TTM", 0);
			    break;
		    case 1:
			    explode(_platform, _rmfName, "TITLE2.TTM", 0);
			    break;
		    case 2:
			    explode(_platform, _rmfName, "INTRO.TTM", 0);
			    break;
		    case 3:
			    explode(_platform, _rmfName, "BIGTV.TTM", 0);
			    break;
		    }
		    k++;
		}
		interpret(_platform, _rmfName, this);

		explode(_platform, _rmfName, "4X5.FNT", 0);
//		explode(_platform, _rmfName, "P6X6.FNT", 0);
		explode(_platform, _rmfName, "DRAGON.FNT", 0);

		Common::String txt("WTHE QUICK BROWN BOX JUMPED OVER THE LAZY DOG");

		Graphics::Surface *dst;
		dst = g_system->lockScreen();
		_fntF->drawString(dst, txt, 20, 20, 320-20, 1);
		g_system->unlockScreen();
		g_system->updateScreen();

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
		g_system->delayMillis(40);
	}
	return Common::kNoError;
}

} // End of namespace Dgds

