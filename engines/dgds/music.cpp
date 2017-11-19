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
#include "audio/midiparser.h"

#include "dgds/music.h"

namespace Dgds {

/**
 * The Standard MIDI File version of MidiParser.
 */
class MidiParser_DGDS : public MidiParser {
protected:
	byte *_init;
	byte *_last_;
	byte _channels;
	uint16 _size[128];

protected:
	void parseNextEvent(EventInfo &info);

public:
	MidiParser_DGDS();
	~MidiParser_DGDS();

	void sendInitCommands();
	
	bool loadMusic(byte *data, uint32 size);

	bool validateNextRead(uint i, uint16 *_curPos);
	byte midiGetNextChannel(uint16 *_curPos, uint32 *_time, long ticker);
	void mixChannels();
};

MidiParser_DGDS::MidiParser_DGDS() : _init(0), _last_(0) {
	memset(_size, 0, sizeof(_size));
}

void MidiParser_DGDS::sendInitCommands() {
}

MidiParser_DGDS::~MidiParser_DGDS() {
	free(_init);
}

void MidiParser_DGDS::parseNextEvent(EventInfo &info) {
	if (_position._playPos >= _last_) {
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
	info.delta += *_position._playPos++;

	// Process the next info.
	if ((_position._playPos[0] & 0xF0) >= 0x80)
		info.event = *_position._playPos++;
	else
		info.event = _position._runningStatus;
	if (info.event < 0x80)
		return;

	_position._runningStatus = info.event;
	switch (info.command()) {
	case 0x9: 
		info.basic.param1 = *_position._playPos++;
		info.basic.param2 = *_position._playPos++;
		if (info.basic.param2 == 0) {
			// NoteOn with param2==0 is a NoteOff
			info.event = info.channel() | 0x80;
		}
		info.length = 0;
		break;

	case 0xC:
	case 0xD:
		info.basic.param1 = *_position._playPos++;
		info.basic.param2 = 0;
		break;

	case 0x8:
	case 0xA:
	case 0xB:
	case 0xE:
		info.basic.param1 = *_position._playPos++;
		info.basic.param2 = *_position._playPos++;
		info.length = 0;
		break;

	case 0xF: // System Common, Meta or SysEx event
		switch (info.event & 0x0F) {
		case 0x2: // Song Position Pointer
			info.basic.param1 = *_position._playPos++;
			info.basic.param2 = *_position._playPos++;
			break;

		case 0x3: // Song Select
			info.basic.param1 = *_position._playPos++;
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
			info.ext.type = *_position._playPos++;
			info.length = readVLQ(_position._playPos);
			info.ext.data = _position._playPos;
			_position._playPos += info.length;
			break;

		default:
			warning("Unexpected midi event 0x%02X in midi data", info.event);
		}
	}
}


byte MidiParser_DGDS::midiGetNextChannel(uint16 *_trackPos, uint32 *_trackTimer, long ticker) {
	byte curr = 0xFF;
	long closest = ticker + 1000000, next = 0;

	for (int i = 0; i < _channels; i++) {
		if (_trackTimer[i] ==  0xFFFFFFFF) // channel ended
			continue;
		if (_trackPos[i] >= _size[i])
			continue;
		next = _tracks[i][_trackPos[i]]; // when the next event should occur
		if (next == 0xF8) // 0xF8 means 240 ticks delay
			next = 240;
		next += _trackTimer[i];
		if (next < closest) {
			curr = i;
			closest = next;
		}
	}

	return curr;
}

inline bool MidiParser_DGDS::validateNextRead(uint i, uint16 *_trackPos) {
	if (_size[i] <= _trackPos[i]) {
		warning("Unexpected end. Music may sound wrong due to game resource corruption");
		return false;
	} else {
		return true;
	}
}

static const byte nMidiParams[] = { 2, 2, 2, 2, 1, 1, 2, 0 };

void MidiParser_DGDS::mixChannels() {
	int totalSize = 0;

	uint16 _trackPos[128];
	uint32 _trackTimer[128];
	byte _prev[128];
	for (int i = 0; i < _channels; i++) {
		_trackTimer[i] = 0;
		_prev[i] = 0;
		_trackPos[i] = 0;
		totalSize += _size[i];
	}

	byte *outData = (byte*)malloc(totalSize * 2);
	_init = outData;

	uint32 ticker = 0;
	byte channelNr, curDelta;
	byte midiCommand = 0, midiParam, globalPrev = 0;
	uint32 newDelta;

	while ((channelNr = midiGetNextChannel(_trackPos, _trackTimer, ticker)) != 0xFF) { // there is still an active channel
		if (!validateNextRead(channelNr, _trackPos))
			goto end;
		curDelta = _tracks[channelNr][_trackPos[channelNr]++];
		_trackTimer[channelNr] += (curDelta == 0xF8 ? 240 : curDelta); // when the command is supposed to occur
		if (curDelta == 0xF8)
			continue;
		newDelta = _trackTimer[channelNr] - ticker;
		ticker += newDelta;

		if (!validateNextRead(channelNr, _trackPos))
			goto end;
		midiCommand = _tracks[channelNr][_trackPos[channelNr]++];
		if (midiCommand != 0xFC) {
			// Write delta
			while (newDelta > 240) {
				*outData++ = 0xF8;
				newDelta -= 240;
			}
			*outData++ = (byte)newDelta;
		}
		// Write command
		switch (midiCommand) {
		case 0xF0: // sysEx
			*outData++ = midiCommand;
			do {
				if (!validateNextRead(channelNr, _trackPos))
					goto end;
				midiParam = _tracks[channelNr][_trackPos[channelNr]++];
				*outData++ = midiParam;
			} while (midiParam != 0xF7);
			break;
		case 0xFC: // end of channel
			_trackTimer[channelNr] = 0xFFFFFFFF;
			break;
		default: // MIDI command
			if (midiCommand & 0x80) {
				if (!validateNextRead(channelNr, _trackPos))
					goto end;
				midiParam = _tracks[channelNr][_trackPos[channelNr]++];
			} else {// running status
				midiParam = midiCommand;
				midiCommand = _prev[channelNr];
			}

			// remember which channel got used for channel remapping
			byte midiChannel = midiCommand & 0xF;
			//_channelUsed[midiChannel] = true;

			if (midiCommand != globalPrev)
				*outData++ = midiCommand;
			*outData++ = midiParam;
			if (nMidiParams[(midiCommand >> 4) - 8] == 2) {
				if (!validateNextRead(channelNr, _trackPos))
					goto end;
				*outData++ = _tracks[channelNr][_trackPos[channelNr]++];
			}
			_prev[channelNr] = midiCommand;
			globalPrev = midiCommand;
			break;
		}
	}

end:
	_last_ = outData;
}


bool MidiParser_DGDS::loadMusic(byte *data, uint32 size_) {
	unloadMusic();

	_init = data;

	uint32 total;

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
	    int type = *pos++;

	    switch(type) {
		case 0:	    debug("Adlib, Soundblaster");   break;
		case 7:	    debug("General MIDI");	    break;
		case 9:	    debug("CMS");		    break;
		case 12:    debug("MT-32");		    break;
		case 18:    debug("PC Speaker");	    break;
		case 19:    debug("Tandy 1000, PS/1");	    break;
		default:    debug("Unknown (%u)", type);    break;
	    }

	    int channels = 0;
	    total = 0;
	    while (pos[0] != 0xFF) {
		pos++;

		if (pos[0] != 0) {
		    debug("%06ld: unknown track arg1 = %d", pos-data, pos[0]);
		}
		pos++;

		uint16 off = read2low(pos) + sci_header;
		uint16 siz = read2low(pos);
		total += siz;

		debug("%06d:%d", off, siz);
		
		byte number = data[off];
		byte voices = data[off+1]&0x0F;
		debug("#%02u: voices: %u", number, voices);

		if (type == 12 && number != 9) {
			_tracks[_numTracks] = data + off;
			_size[_numTracks] = siz;
			_numTracks++;

			if (_numTracks > ARRAYSIZE(_tracks)) {
			    warning("Can only handle %d tracks but was handed %d", (int)ARRAYSIZE(_tracks), _numTracks);
			    return false;
			}
		}

		channels++;
	    }

	    if (type == 12) {
		    _channels = channels;
	    }

	    pos++;

	    debug("- Play parts = %d", channels);
	}
	pos++;

	_ppqn = 1;
	setTempo(16667);

	int tracksRead = 0;
	while (tracksRead < _numTracks) {
		pos = _tracks[tracksRead];
		debug("Header bytes");

		debug("[%06X]  ", *pos);
		debug("[%02X]  ", *pos++);
		debug("[%02X]  ", *pos++);
		_tracks[tracksRead] = pos;
		tracksRead++;
	}

	mixChannels();
	_tracks[0] = _init;
	_numTracks = 1;
	// Note that we assume the original data passed in
	// will persist beyond this call, i.e. we do NOT
	// copy the data to our own buffer. Take warning....
	resetTracking();
	setTrack(0);
	return true;
}

MidiParser_DGDS *createParser_DGDS() { return new MidiParser_DGDS; }

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

	MidiParser_DGDS *parser = createParser_DGDS();
	if (parser->loadMusic(data, size)) {
		parser->setMidiDriver(this);
		parser->sendInitCommands();
		parser->setTimerRate(_driver->getBaseTempo());
		/*
		parser->property(MidiParser::mpCenterPitchWheelOnUnload, 1);
                parser->property(MidiParser::mpSendSustainOffOnNotesOff, 1);*/
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

} // End of namespace Dgds

