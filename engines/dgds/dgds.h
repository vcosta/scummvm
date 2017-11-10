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

#ifndef DGDS_DGDS_H
#define DGDS_DGDS_H

#include "common/error.h"
#include "common/platform.h"

#include "engines/engine.h"

#include "gui/debugger.h"

namespace Dgds {

class DgdsConsole;

struct DgdsGameDescription;

class DgdsEngine : public Engine {
private:
	DgdsConsole *_console;

	Common::Platform _platform;
	const char *_rmfName;

protected:
	virtual Common::Error run();

public:
	DgdsEngine(OSystem *syst, const DgdsGameDescription *gameDesc);
	virtual ~DgdsEngine();

        void playSfx(const char* fileName, byte channel, byte volume);
        void stopSfx(byte channel);
};

class DgdsConsole : public GUI::Debugger {
    public:
	DgdsConsole(DgdsEngine *vm) {}
	virtual ~DgdsConsole(void) {}
};
 
} // End of namespace Dgds

#endif // DGDS_DGDS_H
