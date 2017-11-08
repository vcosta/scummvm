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
	uint32 inSize, outSize;
	
	void init(uint32 size);
};

void DgdsFileCtx::init(uint32 size) {
	inSize = size;
	outSize = 0;
}

struct DgdsChunk {
	char type[DGDS_TYPENAME_MAX+1];
	uint32 chunkSize;
	bool container;
	
	bool readHeader(DgdsFileCtx& ctx, Common::SeekableReadStream& file, const char *name);
	bool isSection(const Common::String& section);

	bool isPacked(const Common::String& ext);
	Common::SeekableReadStream* decode(DgdsFileCtx& ctx, Common::SeekableReadStream& file);
	Common::SeekableReadStream* copy(DgdsFileCtx& ctx, Common::SeekableReadStream& file);
};

bool DgdsChunk::isSection(const Common::String& section) {
       return section.equals(type);
}

bool isFlatfile(Common::Platform platform, const Common::String& ext) {
	bool flat = false;

	if (0) {}
	else if (ext.equals("RST"))
		flat = true;

	switch (platform) {
		case Common::kPlatformAmiga:
			if (0) {}
			else if (ext.equals("BMP"))
				flat = true;
			else if (ext.equals("SCR"))
				flat = true;
			else if (ext.equals("INS"))
				flat = true;
			else if (ext.equals("SNG"))
				flat = true;
			else if (ext.equals("AMG"))
				flat = true;
			break;

		case Common::kPlatformMacintosh:
			/* SOUNDS.SNG is particularly large. */
			if (0) {}
			else if (ext.equals("VIN"))
				flat = true;
			break;
		default:
			break;
	}
	return flat;
}

bool DgdsChunk::isPacked(const Common::String& ext) {
	bool packed = false;

	if (0) {
	}
	else if (ext.equals("ADS") || ext.equals("ADL") || ext.equals("ADH")) {
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
	else if (ext.equals("GDS")) {
		if (strcmp(type, "SDS:") == 0) packed = true;
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
		else if (strcmp(type, "T3V:") == 0) packed = true;
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

bool DgdsChunk::readHeader(DgdsFileCtx& ctx, Common::SeekableReadStream& file, const char *name) {
	if (file.pos() >= file.size()) {
		return false;
	}

	file.read(type, DGDS_TYPENAME_MAX);

	if (type[DGDS_TYPENAME_MAX-1] != ':') {
		type[0] = '\0';
		debug("bad header in: %s", name);
		return false;
	}
	type[DGDS_TYPENAME_MAX] = '\0';

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

static void explode(const char *indexName, bool save) {
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
			uint parent = DGDS_NONE;
			for (uint j=0; j<nfiles; j++) {
				uint32 hash, offset;
				struct DgdsFileCtx ctx;
				uint32 inSize;

				hash = index.readUint32LE();
				offset = index.readUint32LE();
				debug("  %u %u", hash, offset);
				
				volume.seek(offset);
				volume.read(name, sizeof(name));
				name[DGDS_FILENAME_MAX] = '\0';
				inSize = volume.readUint32LE();
				ctx.init(inSize);
				debug("  #%u %s %x=%x %u\n  --", j, name, hash, dgdsHash(name, salt), ctx.inSize);
				
				if (inSize == 0xFFFFFFFF) {
					continue;
				}
				
				file = new Common::SeekableSubReadStream(&volume, offset+13+4, offset+13+4+inSize, DisposeAfterUse::NO);

				if (save) {
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
				
				const char *ext;

				ext = name;
				while (*ext) {
					if (*ext == '.') {
						ext++;
						break;
					}
					ext++;
				}

				Common::Platform platform;

				platform = Common::kPlatformMacintosh;
				if (isFlatfile(platform, ext)) {
					if (strcmp(ext, "RST") == 0) {
						file->hexdump(64);
					}
					if (strcmp(ext, "SCR") == 0) {
						/* Unknown image format (Amiga). */
						file->hexdump(64);
					}
					if (strcmp(ext, "BMP") == 0) {
						/* Unknown image format (Amiga). */
						file->hexdump(64);
					}
					if (strcmp(ext, "INS") == 0) {
						/* IFF-8SVX sound sample (Amiga). */
						file->hexdump(16);
					}
					if (strcmp(ext, "SNG") == 0) {
						/* IFF-SMUS music (Amiga). */
						file->hexdump(16);
					}
					if (strcmp(ext, "AMG") == 0) {
						/* (Amiga). */
						Common::String line = file->readLine();
						while (!file->eos() && !line.empty()) {
							debug("    \"%s\"", line.c_str());
							line = file->readLine();
						}
					}
					if (strcmp(ext, "VIN") == 0) {
						/* (Macintosh). */
						Common::String line = file->readLine();
						while (!file->eos() && !line.empty()) {
							debug("    \"%s\"", line.c_str());
							line = file->readLine();
						}
						file->hexdump(file->size());
					}
				} else {
					uint16 tcount;
					uint16 *tw, *th;
					uint32 *toffsets;

					uint16 *mtx;
					uint16 mw, mh;

					struct DgdsChunk chunk;
					while (chunk.readHeader(ctx, *file, name)) {
						Common::SeekableReadStream *stream;

						bool packed = chunk.isPacked(ext);
						stream = packed ? chunk.decode(ctx, *file) : chunk.copy(ctx, *file);
						if (stream) ctx.outSize += stream->size();

						/*
								debug(">> %u:%u", stream->pos(), file->pos());
								stream->hexdump(stream->size());*/

						if (strcmp(ext, "PAL") == 0) {
							if (strcmp(name, "DYNAMIX.PAL") == 0) {
								if (chunk.isSection("VGA:")) {
									stream->read(palette, 256*3);
								}
							} else {
								if (chunk.isSection("VGA:")) {
									stream->skip(256*3);
								}
							}
						}
						if (strcmp(ext, "SCR") == 0) {
							// BGND.SCR
							if (strcmp(name, "DYNAMIX.SCR") == 0) {
								if (chunk.isSection("BIN:")) {
									stream->read(binData, stream->size());
								} else if (chunk.isSection("VGA:")) {
									stream->read(vgaData, stream->size());
								}
							} else {
								if (chunk.isSection("BIN:")) {
									stream->skip(stream->size());
								} else if (chunk.isSection("VGA:")) {
									stream->skip(stream->size());
								}
							}
						}

						if (strcmp(ext, "ADS") == 0 || strcmp(ext, "ADL") == 0 || strcmp(ext, "ADH") == 0) {
							if (chunk.isSection("VER:")) {
								char version[5];

								stream->read(version, sizeof(version));
								debug("        %s", version);
							} else if (chunk.isSection("RES:")) {
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
							} else if (chunk.isSection("SCR:")) {
								stream->hexdump(stream->size());
							} else if (chunk.isSection("TAG:")) {
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
							}
						}

						if (strcmp(ext, "REQ") == 0) {
							if (chunk.container) {
								if (chunk.isSection("TAG:")) {
									parent = DGDS_TAG;
								} else if (chunk.isSection("REQ:")) {
									parent = DGDS_REQ;
								}
							} else  {
								if (parent == DGDS_TAG) {
									if (chunk.isSection("REQ:")) {
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
									} else if (chunk.isSection("GAD:")) {
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
									}
								} else if (parent == DGDS_REQ) {
									stream->hexdump(stream->size());

									if (chunk.isSection("REQ:")) {
										stream->skip(stream->size());
									} else if (chunk.isSection("GAD:")) {
										stream->skip(stream->size());
									}
								}
							}
						}

						if (strcmp(ext, "SX") == 0) {
							if (chunk.isSection("INF:")) {
								uint16 count;

								stream->hexdump(2);
								stream->skip(2);

								count = stream->readUint16LE();
								debug("        %u:", count);
								for (uint16 k=0; k<count; k++) {
									uint16 idx;
									idx = stream->readUint16LE();
									debug("        %2u: %2u", k, idx);
								}
							} else if (chunk.isSection("TAG:")) {
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
							} else if (chunk.isSection("FNM:")) {
								uint16 count;

								count = stream->readUint16LE();
								debug("        %u", count);
								for (uint16 k=0; k<count; k++) {
									byte ch;
									uint16 idx;
									idx = stream->readUint16LE();

									Common::String str;
									while ((ch = stream->readByte()))
										str += ch;
									debug("        %2u: %2u, \"%s\"", k, idx, str.c_str());
								}
							} else if (chunk.isSection("DAT:")) {
								uint16 idx;
								idx = stream->readUint16LE();
								debug("        %2u", idx);

								stream->hexdump(stream->size());

								/* danger will robinson, danger. */
								uint16 dummy;
								dummy = stream->readUint16LE();

								uint8 compression;
								uint32 unpackSize;

								compression = stream->readByte();
								unpackSize = stream->readUint32LE();
								debug("        %u, %u, %u", dummy, compression, unpackSize);
								stream->hexdump(stream->size());
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
						}

						if (strcmp(ext, "SDS") == 0) {
							if (chunk.isSection("SDS:")) {
								stream->hexdump(stream->size());
							}
						}
						if (strcmp(ext, "GDS") == 0) {
							if (chunk.isSection("INF:")) {
								char version[7];

								// guess. 
								uint dummy = stream->readUint32LE();
								stream->read(version, sizeof(version));
								debug("        %u, \"%s\"", dummy, version);

							} else if (chunk.isSection("SDS:")) {
								stream->hexdump(stream->size());
							}
						}
						if (strcmp(ext, "TTM") == 0) {
							if (chunk.isSection("VER:")) {
								char version[5];

								stream->read(version, sizeof(version));
								debug("        %s", version);
							} else if (chunk.isSection("PAG:")) {
								uint16 pages;
								pages = stream->readUint16LE();
								debug("        %u", pages);
							} else if (chunk.isSection("TT3:")) {
								stream->hexdump(stream->size());
							} else if (chunk.isSection("TAG:")) {
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
						}
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
									mtx[k] = tile;
									//								debug("        %u", tile);
								}
							}

							// DCORNERS.BMP, DICONS.BMP, HELICOP2.BMP, WALKAWAY.BMP, KARWALK.BMP, BLGREND.BMP, FLAMDEAD.BMP, W.BMP, ARCADE.BMP
							// MTX: SCROLL.BMP (intro title), SCROLL2.BMP
							if (strcmp(name, "XXXDYNAMIX.BMP") == 0) {
								if (chunk.isSection("BIN:")) {
									stream->read(binData, stream->size());
								} else if (chunk.isSection("VGA:")) {
									stream->read(vgaData, stream->size());
								} else if (chunk.isSection("INF:")) {
									_tcount = tcount;
									_tw = tw;
									_th = th;
									_toffsets = toffsets;
								} else if (chunk.isSection("MTX:")) {
									_mtx = mtx;
									_mw = mw;
									_mh = mh;
								}
							} else {
								if (chunk.isSection("BIN:")) {
									stream->skip(stream->size());
								} else if (chunk.isSection("VGA:")) {
									stream->skip(stream->size());
								}
							}
						}

						delete stream;
					}
				}

				debug("  [%u:%u] --", file->pos(), ctx.outSize);
			}
			volume.close();
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
	explode("volume.rmf"/*vga,rmf*/, true);
//	return Common::kNoError;
	for (uint i=0; i<256*3; i+=3) {
		palette[i+0] <<= 2;
		palette[i+1] <<= 2;
		palette[i+2] <<= 2;
	}
/*
	// grayscale palette.
	for (uint i=0; i<256; i++) {
	    palette[i*3+0] = 255-i;
	    palette[i*3+1] = 255-i;
	    palette[i*3+2] = 255-i;
	}*/
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
					case Common::KEYCODE_ESCAPE:	return Common::kNoError;
					default:			break;
				}
			}
		}

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
		g_system->copyRectToScreen(imgData, w, 0, 0, w, h);
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
		g_system->updateScreen();
		g_system->delayMillis(20);
	}

	return Common::kNoError;
}

} // End of namespace Dgds

