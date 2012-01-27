/*
 	Ray
    Copyright (C) 2010, 2011, 2012  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Lesser General Public License for more details.

    You have received a copy of the GNU Lesser General Public License
    along with this program (lgpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/

#ifndef _ComputeCore_h
#define _ComputeCore_h

#include <handlers/MessageTagHandler.h>
#include <handlers/SlaveModeHandler.h>
#include <handlers/MasterModeHandler.h>
#include <communication/MessagesHandler.h>
#include <profiling/Profiler.h>
#include <structures/StaticVector.h>
#include <communication/Message.h>
#include <communication/MessageRouter.h>
#include <profiling/TickLogger.h>
#include <scheduling/SwitchMan.h>
#include <memory/RingAllocator.h>
#include <scheduling/VirtualProcessor.h>
#include <communication/VirtualCommunicator.h>
#include <plugins/CorePlugin.h>
#include <plugins/RegisteredPlugin.h>
#include <memory/malloc_types.h>
#include <core/OperatingSystem.h>

#include <stdlib.h>
#include <stdint.h>

/** this class is a compute core
 * to use it, you must set the handlers of your program
 * using setMessageTagObjectHandler(), setSlaveModeObjectHandler(),
 * and setMasterModeObjectHandler().
 *
 * after that, you simply have to call run()
 *
 * \author Sébastien Boisvert
 */
class ComputeCore{
	set<PluginHandle> m_pluginRegistrationsInProgress;
	set<PluginHandle> m_pluginRegistrationsClosed;

	set<SlaveMode> m_allocatedSlaveModes;
	set<MasterMode> m_allocatedMasterModes;
	set<MessageTag> m_allocatedMessageTags;

	map<PluginHandle,RegisteredPlugin> m_plugins;

/** the maximum number of messages with a non-NULL buffers in
 * the outbox */
	int m_maximumAllocatedOutputBuffers;

/** the maximum number of messages in the outbox */
	int m_maximumNumberOfOutboxMessages;

/** the maximum number of inbox messages */
	int m_maximumNumberOfInboxMessages;

	/** the virtual communicator of the MPI rank */
	VirtualCommunicator m_virtualCommunicator;

	/** the virtual processor of the MPI rank */
	VirtualProcessor m_virtualProcessor;

	uint64_t m_startingTimeMicroseconds;

	int m_rank;
	int m_size;
	bool m_showCommunicationEvents;
	bool m_profilerVerbose;
	bool m_runProfiler;

/** the profiler
 * enabled with -run-profiler
 */
	Profiler m_profiler;

/** the switch man */
	SwitchMan m_switchMan;
	TickLogger m_tickLogger;

/** the message router */
	MessageRouter m_router;

	StaticVector m_outbox;
	StaticVector m_inbox;

	/** middleware to handle messages */
	MessagesHandler m_messagesHandler;

/** this object handles messages */
	MessageTagHandler m_messageTagHandler;

/** this object handles master modes */
	MasterModeHandler m_masterModeHandler;

/* this object handles slave modes */
	SlaveModeHandler m_slaveModeHandler;

	// allocator for outgoing messages
	RingAllocator m_outboxAllocator;
	
	// allocator for ingoing messages
	RingAllocator m_inboxAllocator;

	PluginHandle m_currentPluginToAllocate;
	SlaveMode m_currentSlaveModeToAllocate;
	MasterMode m_currentMasterModeToAllocate;
	MessageTag m_currentMessageTagToAllocate;

/** is the program alive ? */
	bool m_alive;

	void runVanilla();
	void runWithProfiler();

	void receiveMessages();
	void sendMessages();
	void processData();
	void processMessages();

	PluginHandle generatePluginHandle();

	bool validationPluginAllocated(PluginHandle plugin);
	bool validationPluginRegistrationNotInProgress(PluginHandle plugin);
	bool validationPluginRegistrationInProgress(PluginHandle plugin);
	bool validationPluginRegistrationNotClosed(PluginHandle plugin);
	bool validationPluginRegistrationClosed(PluginHandle plugin);
	bool validationSlaveModeOwnership(PluginHandle plugin,SlaveMode handle);
	bool validationMasterModeOwnership(PluginHandle plugin,MasterMode handle);
	bool validationMessageTagOwnership(PluginHandle plugin,MessageTag handle);

public:

/** allocate an handle for a plugin **/
	PluginHandle allocatePluginHandle();

/** start the registration process for a plugin **/
	void beginPluginRegistration(PluginHandle plugin);

/** end the registration process for a plugin **/
	void endPluginRegistration(PluginHandle plugin);

/** allocate a slave mode for a handle **/
	SlaveMode allocateSlaveModeHandle(PluginHandle plugin,SlaveMode desiredValue);

/** allocate a master mode **/
	MasterMode allocateMasterModeHandle(PluginHandle plugin,MasterMode desiredValue);

/** allocate a handle for a message tag **/
	MessageTag allocateMessageTagHandle(PluginHandle plugin, MessageTag desiredValue);
	
/** add a slave mode handler */
	void setSlaveModeObjectHandler(PluginHandle plugin,SlaveMode mode,SlaveModeHandler*object);

/** add a master mode handler */
	void setMasterModeObjectHandler(PluginHandle plugin,MasterMode mode,MasterModeHandler*object);

/** add a message tag handler */
	void setMessageTagObjectHandler(PluginHandle plugin,MessageTag tag,MessageTagHandler*object);

/** sets the symbol for a slave mode **/
	void setSlaveModeSymbol(PluginHandle plugin,SlaveMode mode,char*symbol);

/** sets the symbol for a master mode **/
	void setMasterModeSymbol(PluginHandle plugin,MasterMode mode,char*symbol);

/** set the symbol for a message tag **/
	void setMessageTagSymbol(PluginHandle plugin,MessageTag mode,char*symbol);

	/** this is the main method */
	void run();

	/** get the middleware object */
	MessagesHandler*getMessagesHandler();

	void constructor(int*argc,char***argv);

	void enableProfiler();
	void showCommunicationEvents();
	void enableProfilerVerbosity();

	Profiler*getProfiler();

	TickLogger*getTickLogger();
	SwitchMan*getSwitchMan();
	StaticVector*getOutbox();
	StaticVector*getInbox();
	MessageRouter*getRouter();

	RingAllocator*getOutboxAllocator();
	RingAllocator*getInboxAllocator();

	bool*getLife();

	void stop();

	VirtualProcessor*getVirtualProcessor();
	VirtualCommunicator*getVirtualCommunicator();

	int getMaximumNumberOfAllocatedInboxMessages();
	int getMaximumNumberOfAllocatedOutboxMessages();
	void setMaximumNumberOfOutboxBuffers(int maxNumberOfBuffers);
	
	void registerPlugin(CorePlugin*plugin);

	void destructor();

	void setPluginName(PluginHandle plugin,string name);

	void printPlugins();
};

#endif

