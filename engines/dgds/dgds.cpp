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

	uint32 readRLE(void *dataPtr, uint32 dataSize, Common::File& archive);
};

bool DgdsChunk::isSection(const Common::String& section) {
       return section.equals(type);
}

uint32 DgdsChunk::readRLE(void *dataPtr, uint32 dataSize, Common::File& archive) {
	byte *out = (byte *)dataPtr;
	uint32 left = dataSize;

	uint32 lenR = 0, lenW = 0;
	while (left > 0 && !archive.eos()) {
		lenR = archive.readByte();

		if (lenR == 128) {
			// no-op
			lenW = 0;
		} else if (lenR <= 127) {
			// literal run
			lenW = MIN(lenR & 0x7F, left);
			for (uint32 j = 0; j < lenW; j++) {
				*out++ = archive.readByte();
			}
			for (; lenR > lenW; lenR--) {
				archive.readByte();
			}
		} else {  // len > 128
			// expand run
			lenW = MIN(lenR & 0x7F, left);
			byte val = archive.readByte();
			memset(out, val, lenW);
			out += lenW;
		}

		left -= lenW;
	}

	return dataSize - left;
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

	ctx.bytesRead += sizeof(type);
	ctx.outSize += sizeof(type);

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
	uint8 method; /* 0=None, 1=RLE, 2=LZW */
	const char *descr[] = {"None", "RLE", "LZW"};
	uint32 unpackSize;
	Common::SeekableReadStream *ostream = 0;

	method = archive.readByte();
	unpackSize = archive.readSint32LE();
	chunkSize -= (1 + 4);

	ctx.bytesRead += (1 + 4);
	ctx.outSize += (1 + 4);

	if (!container) {
		Common::SeekableReadStream *istream;
		istream = archive.readStream(chunkSize);
		ctx.bytesRead += chunkSize;
		// TODO: decompress
		delete istream;
	}
	/*
	outSize += (1 + 4 + unpackSize);*/

	debug("    %s %u %s %u%c",
		type, chunkSize,
		descr[method],
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

// DGDS, Sherlock, SCI, DM, there's also a nice one in engines/agi/lzw.cpp
// LZW
void LZW::SkipBits() {
	if (nextbit) {
		_res->skip(1); //XXX
		nextbit = 0;
		current = _res->readByte();
	}
}

unsigned int LZW::GetBits(const unsigned int n) {
	unsigned int x = 0;
	for (unsigned int i = 0; i < n; i++) {
		if (current & (1 << nextbit)) {
			x += (1 << i);
		}
		nextbit++;
		if (nextbit > 7) {
			current = _res->readByte();
			nextbit = 0;
		}
	}
	return x;
}

typedef struct _CodeTableEntry {
	uint16 prefix;
	uint8  append;
} CodeTableEntry;

LZW::LZW(Common::SeekableReadStream *res, byte *data) {
	_res = res;
	current = _res->readByte();
	int posout = 0;

	nextbit = 0;

	CodeTableEntry *codetable = new CodeTableEntry[4096];
	uint8 *decodestack = new uint8[4096];
	uint8 *stackptr = decodestack;
	unsigned int n_bits = 9;
	unsigned int free_entry = 257;
	unsigned int oldcode = GetBits(n_bits);
	unsigned int lastbyte = oldcode;
	unsigned int bitpos = 0;

	data[posout] = oldcode;
	posout++;

	while (!_res->eos()) {
		unsigned int newcode = GetBits(n_bits);
		bitpos += n_bits;
		if (newcode == 256) {
			SkipBits();
			_res->skip((((bitpos-1)+((n_bits<<3)-(bitpos-1+(n_bits<<3))%(n_bits<<3)))-bitpos)>>3);
			n_bits = 9;
			free_entry = 256;
			bitpos = 0;
		} else {
			unsigned int code = newcode;
			if (code >= free_entry) {
				*stackptr++ = lastbyte;
				code = oldcode;
			}
			while (code >= 256) {
				*stackptr++ = codetable[code].append;
				code = codetable[code].prefix;
			}
			*stackptr++ = code;
			lastbyte = code;
			while (stackptr > decodestack) {
				data[posout] = (*--stackptr);
				posout++;
			}
			if (free_entry < 4096) {
				codetable[free_entry].prefix = oldcode;
				codetable[free_entry].append = lastbyte;
				free_entry++;
				if ((free_entry >= (unsigned int)(1 << n_bits)) && (n_bits < 12)) {
					n_bits++;
					bitpos = 0;
				}
			}
			oldcode = newcode;
		}
	}
	delete[] decodestack;
	delete[] codetable;
}

// another
LZWdecompressor::LZWdecompressor() {
	_repetitionEnabled = false;
	_codeBitCount = 0;
	_currentMaximumCode = 0;
	_absoluteMaximumCode = 4096;
	for (int i = 0; i < 12; ++i)
		_inputBuffer[i] = 0;
	_dictNextAvailableCode = 0;
	_dictFlushed = false;

	byte leastSignificantBitmasks[9] = {0x00,0x01,0x03,0x07,0x0F,0x1F,0x3F,0x7F,0xFF};
	for (uint16 i = 0; i < 9; ++i)
		_leastSignificantBitmasks[i] = leastSignificantBitmasks[i];
	_inputBufferBitIndex = 0;
	_inputBufferBitCount = 0;
	_charToRepeat = 0;

	_tempBuffer = new byte[5004];
	_prefixCode = new int16[5003];
	_appendCharacter = new byte[5226];
}

LZWdecompressor::~LZWdecompressor() {
	delete[] _appendCharacter;
	delete[] _prefixCode;
	delete[] _tempBuffer;
}

int16 LZWdecompressor::getNextInputCode(Common::MemoryReadStream &inputStream, int32 *inputByteCount) {
	byte *inputBuffer = _inputBuffer;
	if (_dictFlushed || (_inputBufferBitIndex >= _inputBufferBitCount) || (_dictNextAvailableCode > _currentMaximumCode)) {
		if (_dictNextAvailableCode > _currentMaximumCode) {
			_codeBitCount++;
			if (_codeBitCount == 12) {
				_currentMaximumCode = _absoluteMaximumCode;
			} else {
				_currentMaximumCode = (1 << _codeBitCount) - 1;
			}
		}
		if (_dictFlushed) {
			_currentMaximumCode = (1 << (_codeBitCount = 9)) - 1;
			_dictFlushed = false;
		}
		if (*inputByteCount > _codeBitCount) {
			_inputBufferBitCount = _codeBitCount;
		} else {
			_inputBufferBitCount = *inputByteCount;
		}
		if (_inputBufferBitCount > 0) {
			inputStream.read(_inputBuffer, _inputBufferBitCount);
			*inputByteCount -= _inputBufferBitCount;
		} else {
			return -1;
		}
		_inputBufferBitIndex = 0;
		_inputBufferBitCount = (_inputBufferBitCount << 3) - (_codeBitCount - 1);
	}
	int16 bitIndex = _inputBufferBitIndex;
	int16 requiredInputBitCount = _codeBitCount;
	inputBuffer += bitIndex >> 3; /* Address of byte in input buffer containing current bit */
	bitIndex &= 0x0007; /* Bit index of the current bit in the byte */
	int16 nextInputCode = *inputBuffer++ >> bitIndex; /* Get the first bits of the next input code from the input buffer byte */
	requiredInputBitCount -= 8 - bitIndex; /* Remaining number of bits to get for a complete input code */
	bitIndex = 8 - bitIndex;
	if (requiredInputBitCount >= 8) {
		nextInputCode |= *inputBuffer++ << bitIndex;
		bitIndex += 8;
		requiredInputBitCount -= 8;
	}
	nextInputCode |= (*inputBuffer & _leastSignificantBitmasks[requiredInputBitCount]) << bitIndex;
	_inputBufferBitIndex += _codeBitCount;
	return nextInputCode;
}

void LZWdecompressor::outputCharacter(byte character, byte **out) {
	byte *output = *out;

	if (false == _repetitionEnabled) {
		if (character == 0x90)
			_repetitionEnabled = true;
		else
			*output++ = _charToRepeat = character;
	} else {
		if (character) { /* If character following 0x90 is not 0x00 then it is the repeat count */
			while (--character)
				*output++ = _charToRepeat;
		} else /* else output a 0x90 character */
			*output++ = 0x90;

		_repetitionEnabled = false;
	}

	*out = output;
	return;
}

int32 LZWdecompressor::decompress(Common::MemoryReadStream &inStream, int32 inputByteCount, byte *out) {
	byte *reversedDecodedStringStart;
	byte *reversedDecodedStringEnd = reversedDecodedStringStart = _tempBuffer;
	byte *originalOut = out;
	_repetitionEnabled = false;
	_codeBitCount = 9;
	_dictFlushed = false;
	_currentMaximumCode = (1 << (_codeBitCount = 9)) - 1;
	for (int16 code = 255; code >= 0; code--) {
		_prefixCode[code] = 0;
		_appendCharacter[code] = code;
	}
	_dictNextAvailableCode = 257;
	int16 oldCode;
	int16 character = oldCode = getNextInputCode(inStream, &inputByteCount);
	if (oldCode == -1) {
		return -1L;
	}
	outputCharacter(character, &out);
	int16 code;
	while ((code = getNextInputCode(inStream, &inputByteCount)) > -1) {
		if (code == 256) { /* This code is used to flush the dictionary */
			for (int i = 0; i < 256; ++i)
				_prefixCode[i] = 0;
			_dictFlushed = true;
			_dictNextAvailableCode = 256;
			if ((code = getNextInputCode(inStream, &inputByteCount)) == -1) {
				break;
			}
		}
		/* This code checks for the special STRING+CHARACTER+STRING+CHARACTER+STRING case which generates an undefined code.
		It handles it by decoding the last code, adding a single character to the end of the decoded string */
		int16 newCode = code;
		if (code >= _dictNextAvailableCode) { /* If code is not defined yet */
			*reversedDecodedStringEnd++ = character;
			code = oldCode;
		}
		/* Use the string table to decode the string corresponding to the code and store the string in the temporary buffer */
		while (code >= 256) {
			*reversedDecodedStringEnd++ = _appendCharacter[code];
			code = _prefixCode[code];
		}
		*reversedDecodedStringEnd++ = (character = _appendCharacter[code]);
		/* Output the decoded string in reverse order */
		do {
			outputCharacter(*(--reversedDecodedStringEnd), &out);
		} while (reversedDecodedStringEnd > reversedDecodedStringStart);
		/* If possible, add a new code to the string table */
		if ((code = _dictNextAvailableCode) < _absoluteMaximumCode) {
			_prefixCode[code] = oldCode;
			_appendCharacter[code] = character;
			_dictNextAvailableCode = code + 1;
		}
		oldCode = newCode;
	}
	return out - originalOut; /* Byte count of decompressed data */
}

// another
void LzwDecompressor::decompress(byte *source, byte *dest) {

	_source = source;

	byte litByte = 0;
	uint16 oldCode = 0;
	uint16 copyLength, maxCodeValue, code, nextCode, lastCode;

	byte *copyBuf = new byte[8192];

	struct { uint16 code; byte value; } codeTable[8192];
	memset(codeTable, 0, sizeof(codeTable));

	_codeLength = 9;
	nextCode = 258;
	maxCodeValue = 512;

	copyLength = 0;
	_sourceBitsLeft = 8;

	while (1) {

		code = getCode();

		if (code == 257)
			break;

		if (code == 256) {
			_codeLength = 9;
			nextCode = 258;
			maxCodeValue = 512;
			lastCode = getCode();
			oldCode = lastCode;
			litByte = lastCode;
			*dest++ = litByte;
		} else {
			lastCode = code;
			if (code >= nextCode) {
				lastCode = oldCode;
				copyBuf[copyLength++] = litByte;
			}
			while (lastCode > 255) {
				copyBuf[copyLength++] = codeTable[lastCode].value;
				lastCode = codeTable[lastCode].code;
			}
			litByte = lastCode;
			copyBuf[copyLength++] = lastCode;
			while (copyLength > 0)
				*dest++ = copyBuf[--copyLength];
			codeTable[nextCode].value = lastCode;
			codeTable[nextCode].code = oldCode;
			nextCode++;
			oldCode = code;
			if (nextCode >= maxCodeValue && _codeLength <= 12) {
				_codeLength++;
				maxCodeValue <<= 1;
			}
		}

	}

	delete[] copyBuf;

}

uint16 LzwDecompressor::getCode() {
	const byte bitMasks[9] = {
		0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F, 0x0FF
	};

	byte   resultBitsLeft = _codeLength;
	byte   resultBitsPos = 0;
	uint16 result = 0;
	byte   currentByte = *_source;
	byte   currentBits = 0;

	// Get bits of current byte
	while (resultBitsLeft) {
		if (resultBitsLeft < _sourceBitsLeft) {
			// we need less than we have left
			currentBits = (currentByte >> (8 - _sourceBitsLeft)) & bitMasks[resultBitsLeft];
			result |= (currentBits << resultBitsPos);
			_sourceBitsLeft -= resultBitsLeft;
			resultBitsLeft = 0;

		} else {
			// we need as much as we have left or more
			resultBitsLeft -= _sourceBitsLeft;
			currentBits = currentByte >> (8 - _sourceBitsLeft);
			result |= (currentBits << resultBitsPos);
			resultBitsPos += _sourceBitsLeft;

			// Go to next byte
			_source++;

			_sourceBitsLeft = 8;
			if (resultBitsLeft) {
				currentByte = *_source;
			}
		}
	}
	return result;
}

// another 

} // End of namespace Dgds

