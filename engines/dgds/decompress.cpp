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
#include "common/util.h"

#include "dgds/decompress.h"

namespace Dgds {

uint32 RleDecompressor::decompress(byte *dest, uint32 size, byte *source) {
	uint32 left = size;

	uint32 lenR = 0, lenW = 0;
	while (left > 0) {
		lenR = *source++;

		if (lenR == 128) {
			// no-op
			lenW = 0;
		} else if (lenR <= 127) {
			// literal run
			lenW = MIN(lenR & 0x7F, left);
			for (uint32 j = 0; j < lenW; j++) {
				*dest++ = *source++;
			}
			for (; lenR > lenW; lenR--) {
				source++;
			}
		} else {  // len > 128
			// expand run
			lenW = MIN(lenR & 0x7F, left);
			byte val = *source++;
			memset(dest, val, lenW);
			dest += lenW;
		}

		left -= lenW;
	}

	return size - left;
}

void LzwDecompressor::decompress(byte *dest, uint32 size, byte *source) {
	_source = source;

	byte litByte = 0;
	uint16 oldCode = 0;
	uint16 copyLength, maxCodeValue, code, nextCode, lastCode;

	byte *copyBuf = new byte[0x4000];

	struct { uint16 code; byte value; } codeTable[0x4000];
	memset(codeTable, 0, sizeof(codeTable));

	_codeLength = 9;
	nextCode = 0x101;
	maxCodeValue = 0x200;

	copyLength = 0;
	_sourceBitsLeft = 8;

	while (1) {
		debug("%u:%lu", size,(_source-source));

		code = getCode();

		if (code == 0x101)
			break;

		if (code == 0x100) {
			_codeLength = 9;
			nextCode = 0x101;
			maxCodeValue = 0x200;
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

} // End of namespace Dgds
