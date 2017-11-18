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

#ifndef SCI_SOUND_H
#define SCI_SOUND_H

namespace Dgds {
// Music patches in SCI games:
// ===========================
// 1.pat - MT-32 driver music patch
// 2.pat - Yamaha FB01 driver music patch
// 3.pat - Adlib driver music patch
// 4.pat - Casio MT-540 (in earlier SCI0 games)
// 4.pat - GM driver music patch (in later games that support GM)
// 7.pat (newer) / patch.200 (older) - Mac driver music patch / Casio CSM-1
// 9.pat (newer) / patch.005 (older) - Amiga driver music patch
// 98.pat - Unknown, found in later SCI1.1 games. A MIDI format patch
// 101.pat - CMS/PCjr driver music patch.
//           Only later PCjr drivers use this patch, earlier ones don't use a patch
// bank.001 - older SCI0 Amiga instruments
#define SCI_MIDI_CHANNEL_NOTES_OFF 0x7B /* all-notes-off for Bn */


enum {
	MIDI_CHANNELS = 16,
	MIDI_PROP_MASTER_VOLUME = 0
};

class MidiPlayer : public MidiDriver_BASE {
protected:
	MidiDriver *_driver;
	int8 _reverb;

public:
	MidiPlayer() : _driver(0), _reverb(-1) { }

	int open() { return _driver->open(); }
	virtual void close() { _driver->close(); }
	virtual void send(uint32 b) { _driver->send(b); }
	virtual uint32 getBaseTempo() { return _driver->getBaseTempo(); }
	virtual bool hasRhythmChannel() const = 0;
	virtual void setTimerCallback(void *timer_param, Common::TimerManager::TimerProc timer_proc) { _driver->setTimerCallback(timer_param, timer_proc); }

	virtual byte getPlayId() const = 0;
	virtual int getPolyphony() const = 0;
	virtual int getFirstChannel() const { return 0; }
	virtual int getLastChannel() const { return 15; }

	virtual void setVolume(byte volume) {
		if(_driver)
			_driver->property(MIDI_PROP_MASTER_VOLUME, volume);
	}

	virtual int getVolume() {
		return _driver ? _driver->property(MIDI_PROP_MASTER_VOLUME, 0xffff) : 0;
	}

	virtual void onNewSound() {}

	// Returns the current reverb, or -1 when no reverb is active
	int8 getReverb() const { return _reverb; }
	// Sets the current reverb, used mainly in MT-32
	virtual void setReverb(int8 reverb) { _reverb = reverb; }

	virtual void playSwitch(bool play) {
		if (!play) {
			// Send "All Sound Off" on all channels
			for (int i = 0; i < MIDI_CHANNELS; ++i)
				_driver->send(0xb0 + i, SCI_MIDI_CHANNEL_NOTES_OFF, 0);
		}
	}
};

	extern MidiPlayer *MidiPlayer_AmigaMac_create();
} // End of namespace Sci

#endif // SCI_SOUND_H

