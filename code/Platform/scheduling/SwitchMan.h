/*
 	Ray
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (COPYING).  
	see <http://www.gnu.org/licenses/>
*/

#ifndef _SwitchMan_H
#define _SwitchMan_H

#include <map>
#include <core/master_modes.h>
#include <structures/StaticVector.h>
#include <communication/Message.h>
#include <core/slave_modes.h>
#include <core/master_modes.h>
using namespace std;

/**
 * the switchman controls the workflow on all ranks
 * he is the one who decides when something must be done.
 * \author Sébastien Boisvert
 * \date 2012-01-02
 */
class SwitchMan{
/** the current slave mode of the rank */
	RaySlaveMode m_slaveMode;

/** the current master mode of the rank */
	RayMasterMode m_masterMode;
	
	RayMasterMode m_lastMasterMode;

	/** number of cores */
	int m_size;

/** a counter to check progression */
	int m_counter;

/** a list of switches describing the order of the master modes */
	map<RayMasterMode,RayMasterMode> m_switches;

/** a table to convert a tag to a slave mode */
	map<Tag,RaySlaveMode> m_tagToSlaveModeTable;

/** a table containing mapping from master modes to MPI tags */
	map<RayMasterMode,Tag> m_masterModeToTagTable;

/** run some assertions */
	void runAssertions();

public:

/** the switchman is constructed with a number of cores */
	void constructor(int numberOfRanks);

/** reset */
	void reset();

/** returns true if all ranks finished their current slave mode */
	bool allRanksAreReady();

/** according to scripting/master_mode_order.txt, returns
 * the next master mode to set */
	RayMasterMode getNextMasterMode(RayMasterMode currentMode);

/**
 * add a master mode associated to another */
	void addNextMasterMode(RayMasterMode a,RayMasterMode b);

/** add a slave switch */
	void addSlaveSwitch(Tag tag,RaySlaveMode slaveMode);

/** add a master switch */
	void addMasterSwitch(RayMasterMode masterMode,Tag tag);

	/** remotely open a slave mode */
	void openSlaveMode(Tag tag,StaticVector*outbox,Rank source,Rank destination);

	/** open a slave mode locally */
	void openSlaveModeLocally(Tag tag,Rank rank);

	/** close a slave mode remotely */
	void closeSlaveMode(Rank source);

	/** locally close a slave mode */
	void closeSlaveModeLocally(StaticVector*outbox,Rank source);

	/** begin a master mode */
	// TODO: add an option SERIAL or PARALLEL */
	void openMasterMode(StaticVector*outbox,Rank source);

	/** close a master mode */
	void closeMasterMode();

	/** send an empty message */
	void sendEmptyMessage(StaticVector*outbox,Rank source,Rank destination,Tag tag);

/** sends a message with a full list of parameters */
	void sendMessage(uint64_t*buffer,int count,StaticVector*outbox,Rank source,Rank destination,Tag tag);

/** get the current slave mode */
	RaySlaveMode getSlaveMode();

/** get the current slave mode pointer */
	int*getSlaveModePointer();

/** change the slave mode manually */
	void setSlaveMode(RaySlaveMode mode);

/** get the master mode */
	RayMasterMode getMasterMode();

/** get the master mode pointer */
	int*getMasterModePointer();

/** changes the master mode manually */
	void setMasterMode(RayMasterMode mode);

/** send a message to all MPI ranks */
	void sendToAll(StaticVector*outbox,Rank source,Tag tag);

/** send a message to all MPI ranks, possibly with data */
	void sendMessageToAll(uint64_t*buffer,int count,StaticVector*outbox,Rank source,Tag tag);
};

#endif