/*
 	rayplatform: a message-passing development framework
    copyright (c) 2010, 2011, 2012, 2013 sébastien boisvert

	http://github.com/sebhtml/rayplatform

    this program is free software: you can redistribute it and/or modify
    it under the terms of the gnu lesser general public license as published by
    the free software foundation, version 3 of the license.

    this program is distributed in the hope that it will be useful,
    but without any warranty; without even the implied warranty of
    merchantability or fitness for a particular purpose.  see the
    gnu lesser general public license for more details.

    you have received a copy of the gnu lesser general public license
    along with this program (lgpl-3.0.txt).
	see <http://www.gnu.org/licenses/>

*/

#include "ComputeCore.h"
#include "OperatingSystem.h"

#include <RayPlatform/profiling/ProcessStatus.h>
#include <RayPlatform/cryptography/crypto.h>
#include <RayPlatform/communication/MessagesHandler.h>

#include <stdlib.h>
#include <string.h> /* for strcpy */
#ifdef CONFIG_ASSERT
#include <assert.h>
#endif
#include <iostream>
using namespace std;

#ifdef CONFIG_SLEEPY_RAY
#include <unistd.h>
#endif

#include <signal.h>

/**
 * Global variable are bad.
 */
bool globalDebugMode = false;

/**
 * \see http://stackoverflow.com/questions/6168636/how-to-trigger-sigusr1-and-sigusr2
 * \see http://stackoverflow.com/questions/231912/what-is-the-difference-between-sigaction-and-signal
 */
void handleSignal(int signalNumber) {

	//cout << "DEBUG Received a signal !!" << endl;

	if(signalNumber == SIGUSR1) {

		globalDebugMode = !globalDebugMode;

		//cout << "DEBUG globalDebugMode <- " << globalDebugMode << endl;
	}
}

//#define CONFIG_DEBUG_SLAVE_SYMBOLS
//#define CONFIG_DEBUG_MASTER_SYMBOLS
//#define CONFIG_DEBUG_TAG_SYMBOLS
//#define CONFIG_DEBUG_processData
//#define CONFIG_DEBUG_CORE

ComputeCore::ComputeCore(){

	m_useActorModelOnly = false;
	m_playground.initialize(this);

}

void ComputeCore::setSlaveModeObjectHandler(PluginHandle plugin,SlaveMode mode,SlaveModeHandlerReference object){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationSlaveModeRange(plugin,mode))
		return;

	if(!validationSlaveModeOwnership(plugin,mode)){
		cout<<"was trying to set object handler for mode "<<mode<<endl;
		return;
	}

	#ifdef CONFIG_DEBUG_CORE
	cout<<"setSlaveModeObjectHandler "<<SLAVE_MODES[mode]<<" to "<<object<<endl;
	#endif

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasSlaveMode(mode));
	#endif

	m_slaveModeExecutor.setObjectHandler(mode,object);

	m_plugins[plugin].addRegisteredSlaveModeHandler(mode);
}

void ComputeCore::setMasterModeObjectHandler(PluginHandle plugin,MasterMode mode,MasterModeHandlerReference object){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"setMasterModeObjectHandler "<<MASTER_MODES[mode]<<" to "<<object<<endl;
	#endif

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMasterMode(mode));
	#endif

	m_masterModeExecutor.setObjectHandler(mode,object);

	m_plugins[plugin].addRegisteredMasterModeHandler(mode);
}

void ComputeCore::setMessageTagObjectHandler(PluginHandle plugin,MessageTag tag,MessageTagHandlerReference object){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"setMessageTagObjectHandler "<<MESSAGE_TAGS[tag]<<" to "<<object<<endl;
	#endif

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMessageTag(tag));
	#endif

	m_messageTagExecutor.setObjectHandler(tag,object);

	m_plugins[plugin].addRegisteredMessageTagHandler(tag);
}


/**
 * runWithProfiler if -run-profiler is provided
 * otherwise, run runVanilla()
 */
void ComputeCore::run(){

	if(m_doChecksum){
		cout<<"[RayPlatform] Rank "<<m_rank<<" will compute a CRC32 checksum for any non-empty message." << endl;
	}

	// ask the router if it is enabled
	// the virtual router will disable itself if there were
	// problems during configuration
	m_routerIsEnabled=m_router.isEnabled();

	configureEngine();

	if(!m_resolvedSymbols)
		resolveSymbols();

	if(m_hasFatalErrors){
		cout<<"Exiting because of previously reported errors."<<endl;
		return;
	}

	m_startingTimeMicroseconds = getMicroseconds();

	if(m_routerIsEnabled)
		m_router.getGraph()->start(m_rank);


	/*
	 * Set up signal handler
	 * \see http://pubs.opengroup.org/onlinepubs/7908799/xsh/sigaction.html
	 * \see http://beej.us/guide/bgipc/output/html/multipage/signals.html
	 */

	struct sigaction newAction;
	newAction.sa_handler = handleSignal;
	sigemptyset(&newAction.sa_mask);
	newAction.sa_flags = 0;

	sigaction(SIGUSR1, &newAction, NULL);

	m_runProfiler = globalDebugMode;

	runWithProfiler();

#if 0
	if(m_runProfiler){
		runWithProfiler();
	}else{
		runVanilla();
	}
#endif

	if(m_routerIsEnabled)
		m_router.getGraph()->printStatus();

}

bool ComputeCore::debugModeIsEnabled() {
	return m_runProfiler;
}

/**
 * the while loop is *the* main loop of Ray for each
 * processor.
 * it is similar to the main loop of a video game, actually, but without a display.
 */
void ComputeCore::runVanilla(){

	/*
	 * This is not used anymore !
	 */
	return;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"Locking inbox for the loop."<<endl;
	#endif

	#ifdef CONFIG_DEBUG_CORE
	cout<<"m_alive= "<<m_alive<<endl;
	#endif

/*
 * The inbox is only unlocked before the receiveMessages() and
 * after sendMessages().
 *
 * The outbox is only unlocked during sendMessages().
 *
 * The inbox is cleared at the end of processData()
 * because some plugins picks up messages in the inbox
 * instead of using the modular plugin architecture with
 * the handlers for message tags.
 *
 * The outbox is cleared by the communication layer when it
 * picks up the messages.
 */
	while(m_alive || (m_routerIsEnabled && !m_router.hasCompletedRelayEvents())){

#ifdef CONFIG_SLEEPY_RAY
		usleep(10);
#endif

		// 1. receive the message (0 or 1 message is received)
		// blazing fast, receives 0 or 1 message, never more, never less, other messages will wait for the next iteration !
		receiveMessages();

		// 2. process the received message, if any
		// consume the one message received, if any, also very fast because it is done with an array mapping tags to function pointers
		processMessages();

		// 3. process data according to current slave and master modes
		// should be fast, but apparently call_RAY_SLAVE_MODE_EXTENSION is slowish sometimes...
		processData();

		// 4. send messages
		// fast, sends at most 17 messages. In most case it is either 0 or 1 message.,..
		sendMessages();
	}

	#ifdef CONFIG_DEBUG_CORE
	cout<<"m_alive= "<<m_alive<<endl;
	#endif

	if(m_miniRanksAreEnabled){
#ifdef CONFIG_USE_LOCKING
		m_bufferedOutbox.lock();
#endif /* CONFIG_USE_LOCKING */

		m_bufferedOutbox.sendKillSignal();

#ifdef CONFIG_USE_LOCKING
		m_bufferedOutbox.unlock();
#endif /* CONFIG_USE_LOCKING */
	}
}

/*
 * This is the main loop of the program.
 * One instance on each MPI rank.
 */
void ComputeCore::runWithProfiler(){
	// define some variables that hold life statistics of this
	// MPI rank
	int ticks=0;
	uint64_t globalTicks = 0;

	int sentMessages=0;
	int sentMessagesInProcessMessages=0;
	int sentMessagesInProcessData=0;
	int receivedMessages=0;
	map<int,int> receivedTags;
	map<int,int> sentTagsInProcessMessages;
	map<int,int> sentTagsInProcessData;

	int resolution=1000;// milliseconds

	uint64_t startingTime=getMilliSeconds();

	uint64_t lastTime=getMilliSeconds();

/*
	uint64_t lastTickWhenSentMessageInProcessMessage=lastTime;
	uint64_t lastTickWhenSentMessageInProcessData=lastTime;
*/

	vector<int> distancesForProcessMessages;
	vector<int> distancesForProcessData;

	bool profilerVerbose = true;

	ProcessStatus status;

	uint64_t t=getMilliSeconds();

	//m_lastTerminalProbeOperation = time(NULL);
	bool profileGranularity = false;

	m_playground.bootActors();

	while(isRankAlive()
			|| (m_routerIsEnabled && !m_router.hasCompletedRelayEvents())) {

		// update the debug mode.
		// this is cheap -- it only needs to copy a boolean (usually a char)
		//
		m_runProfiler = globalDebugMode;

#if 0
		if(debugModeIsEnabled())
			cout << "DEBUG Online" << endl;
#endif

		//runRayPlatformTerminal();

		if(debugModeIsEnabled())
			t=getMilliSeconds();


		/*
		 * Show debug window for first tick, then at each 1000 ms.
		 */
		if(debugModeIsEnabled() && (globalTicks == 0 || (t >= (lastTime+resolution)))){

			double seconds=(t-startingTime)/1000.0;

			int balance=sentMessages-receivedMessages;

			if(debugModeIsEnabled()){

				cout << "[/dev/actor/rank/" << getRank() << "] ";
				cout << "[RayPlatform] epoch ends at ";
				cout << seconds * 1000 << " ms ! (tick # " << globalTicks;
				cout << "), length is ";
				cout << resolution << " ms" << endl;
				m_playground.printStatus();
				m_outboxAllocator.printBufferStatus();

				status.getProcessStatus();
				status.printMemoryMetrics();
				cout << endl;

				printf("Rank %i: %s Time= %.2f s Speed= %i Sent= %i (processMessages: %i, processData: %i) Received= %i Balance= %i\n",
					m_rank,SLAVE_MODES[m_switchMan.getSlaveMode()],
					seconds,ticks,sentMessages,sentMessagesInProcessMessages,sentMessagesInProcessData,
					receivedMessages,balance);
				fflush(stdout);

				if(receivedMessages == 0) {
					cout << "Warning: no messages were received !" << endl;
				}

				if(sentMessagesInProcessData == 0) {
					cout << "Warning: no message were sent !" <<endl;
				}

				if(profileGranularity)
					m_profiler.printGranularities(m_rank);
			}

			m_profiler.clearGranularities();

			if(debugModeIsEnabled() && receivedTags.size() > 0 && profilerVerbose){
				cout<<"Rank "<<m_rank<<" received in receiveMessages:"<<endl;
				for(map<int,int>::iterator i=receivedTags.begin();i!=receivedTags.end();i++){
					int tag=i->first;
					int count=i->second;
					cout<<"Rank "<<m_rank<<"        "<<MESSAGE_TAGS[tag]<<"	"<<count<<endl;
				}
			}

			if(debugModeIsEnabled() && sentTagsInProcessMessages.size() > 0 && profilerVerbose){
				cout<<"Rank "<<m_rank<<" sent in processMessages:"<<endl;
				for(map<int,int>::iterator i=sentTagsInProcessMessages.begin();i!=sentTagsInProcessMessages.end();i++){
					int tag=i->first;
					int count=i->second;
					cout<<"Rank "<<m_rank<<"        "<<MESSAGE_TAGS[tag]<<"	"<<count<<endl;
				}

/*
				int average1=getAverage(&distancesForProcessMessages);
				int deviation1=getStandardDeviation(&distancesForProcessMessages);

				cout<<"Rank "<<m_rank<<" distance between processMessages messages: average= "<<average1<<", stddev= "<<deviation1<<
					", n= "<<distancesForProcessMessages.size()<<endl;

*/
				#ifdef FULL_DISTRIBUTION
				map<int,int> distribution1;
				for(int i=0;i<(int)distancesForProcessMessages.size();i++){
					distribution1[distancesForProcessMessages[i]]++;
				}
				cout<<"Rank "<<m_rank<<" distribution: "<<endl;
				for(map<int,int>::iterator i=distribution1.begin();i!=distribution1.end();i++){
					cout<<i->first<<" "<<i->second<<endl;
				}
				#endif

			}

			distancesForProcessMessages.clear();

			if(debugModeIsEnabled () && sentTagsInProcessData.size() > 0 && profilerVerbose){
				cout<<"Rank "<<m_rank<<" sent in processData:"<<endl;
				for(map<int,int>::iterator i=sentTagsInProcessData.begin();i!=sentTagsInProcessData.end();i++){
					int tag=i->first;
					int count=i->second;
					cout<<"Rank "<<m_rank<<"        "<<MESSAGE_TAGS[tag]<<"	"<<count<<endl;
				}
/*
				int average2=getAverage(&distancesForProcessData);
				int deviation2=getStandardDeviation(&distancesForProcessData);

				cout<<"Rank "<<m_rank<<" distance between processData messages: average= "<<average2<<", stddev= "<<deviation2<<
					", n= "<<distancesForProcessData.size()<<endl;

*/
				#ifdef FULL_DISTRIBUTION
				map<int,int> distribution2;
				for(int i=0;i<(int)distancesForProcessData.size();i++){
					distribution2[distancesForProcessData[i]]++;
				}
				cout<<"Rank "<<m_rank<<" distribution: "<<endl;
				for(map<int,int>::iterator i=distribution2.begin();i!=distribution2.end();i++){
					cout<<i->first<<" "<<i->second<<endl;
				}
				#endif

			}

			distancesForProcessData.clear();

			sentMessages=0;
			sentMessagesInProcessMessages=0;
			sentMessagesInProcessData=0;
			receivedMessages=0;
			receivedTags.clear();
			sentTagsInProcessMessages.clear();
			sentTagsInProcessData.clear();
			ticks=0;

			lastTime=t;
		}

		/* collect some statistics for the profiler */

		// 1. receive the message (0 or 1 message is received)
		receiveMessages();
		receivedMessages+=m_inbox.size();

		for(int i=0;i<(int)m_inbox.size();i++){
			// stript routing information, if any
			uint8_t tag=m_inbox[i]->getTag();
			receivedTags[tag]++;
		}

		// 2. process the received message, if any
		processMessages();

		int messagesSentInProcessMessages=m_outbox.size();
		sentMessagesInProcessMessages += messagesSentInProcessMessages;
		sentMessages += messagesSentInProcessMessages;

/*
		if(messagesSentInProcessMessages > 0){
			int distance=t- lastTickWhenSentMessageInProcessMessage;
			lastTickWhenSentMessageInProcessMessage=t;
			distancesForProcessMessages.push_back(distance);
		}
*/

		// 3. process data according to current slave and master modes

		int currentSlaveMode=m_switchMan.getSlaveMode();

		uint64_t startingTime = getThreadMicroseconds();

		if(!useActorModelOnly()) {
			processData();
		}

		uint64_t endingTime = getThreadMicroseconds();

		int difference = endingTime - startingTime;

		if(profileGranularity)
			m_profiler.addGranularity(currentSlaveMode,difference);

		/* threshold to say something is taking too long */
		/* in microseconds */
		int tooLong=m_profiler.getThreshold();

		if(debugModeIsEnabled() && profileGranularity && difference >= tooLong){
			cout<<"Warning, SlaveMode= "<<SLAVE_MODES[currentSlaveMode]<<" GranularityInMicroseconds= "<<difference<<""<<endl;
			m_profiler.printStack();
		}

		if(profileGranularity)
			m_profiler.resetStack();

		int messagesSentInProcessData = m_outbox.size() - messagesSentInProcessMessages;
		sentMessagesInProcessData += messagesSentInProcessData;
		sentMessages += messagesSentInProcessData;

		for(int i=0;i<messagesSentInProcessMessages;i++){
			if(!debugModeIsEnabled())
				break;
			// stript routing information, if any
			uint8_t tag=m_outbox[i]->getTag();
			sentTagsInProcessMessages[tag]++;
		}

		for(int i=messagesSentInProcessMessages;i<(int)m_outbox.size();i++){
			if(!debugModeIsEnabled())
				break;
			// stript routing information, if any
			uint8_t tag=m_outbox[i]->getTag();
			sentTagsInProcessData[tag]++;
		}

		// 4. send messages
		sendMessages();

		m_outboxAllocator.resetCount();

		/* increment ticks */
		ticks++;
		globalTicks ++;
	}

	//cout << "DEBUG exiting ComputeCore loop" << endl;

	if(m_miniRanksAreEnabled){
#ifdef CONFIG_USE_LOCKING
		m_bufferedOutbox.lock();
#endif /* CONFIG_USE_LOCKING */

		m_bufferedOutbox.sendKillSignal();

#ifdef CONFIG_USE_LOCKING
		m_bufferedOutbox.unlock();
#endif /* CONFIG_USE_LOCKING */
	}


	if(debugModeIsEnabled() && profileGranularity)
		m_profiler.printAllGranularities();
}

void ComputeCore::processMessages(){
	#ifdef CONFIG_ASSERT
	assert(m_inbox.size()>=0&&m_inbox.size()<=1);
	#endif

	if(m_inbox.size()==0)
		return;

	int i = 0;

	Message * importantMessage = m_inbox.at(i);

	// TODO: don't transport routing metadata if routing is disabled !
	// save actor metadata
	//
	// load routing and acting metadata in the reverse order
	// in which they were saved !!!

	// if routing is enabled, we want to strip the routing tags if it
	// is required
	if(m_routerIsEnabled){
		if(m_router.routeIncomingMessages()){
			// if the message has routing tag, we don't need to process it...

/*
 * If a message is routed, the original message needs to be destroyed
 * with fire. Otherwise, a slave mode or a master mode will pick it up,
 * which will cause silly behaviors.
 */
			m_inbox.clear();

			return;
		}
	}

	// dispatch the message
	if(importantMessage->isActorModelMessage(m_size)) {

		// cout << "DEBUG actor message detected ! m_size = " << m_size << endl;

		m_playground.receiveActorMessage(importantMessage);
	}

	if(m_inbox.size() > 0 && m_showCommunicationEvents){
		uint64_t theTime=getMicroseconds();
		uint64_t microseconds=theTime - m_startingTimeMicroseconds;
		for(int i=0;i<(int)m_inbox.size();i++){
			cout<<"[Communication] "<<microseconds<<" microseconds, RECEIVE ";
			m_inbox[i]->print();
			cout<<endl;
		}
	}



	Message*message= importantMessage;
	MessageTag messageTag=message->getTag();

	#ifdef CONFIG_ASSERT2
	assert(messageTag!=INVALID_HANDLE);
	assert(m_allocatedMessageTags.count(messageTag)>0);
	string symbol=MESSAGE_TAGS[messageTag];
	assert(m_messageTagSymbols.count(symbol));
	assert(m_messageTagSymbols[symbol]==messageTag);
	#endif

	// check if the tag is in the list of slave switches
	m_switchMan.openSlaveModeLocally(messageTag,m_rank);

	m_messageTagExecutor.callHandler(messageTag,message);
}

void ComputeCore::sendMessages(){

/*
 * Don't do anything when there are no messages
 * at all.
 * This statement needs to be after the ifdef CONFIG_ASSERT above
 * otherwise the no resetCount call is performed on the
 * allocator, which will produce a run-time error.
 */
	if(m_outbox.size()==0)
		return;

	int numberOfMessages = m_outbox.size();

// first, allocate new a new buffer for each message.

	bool forceMemoryReallocation = true;

	for(int i = 0 ; i < (int) m_outbox.size() ; ++i) {
		Message * aMessage = m_outbox.at(i);

		// we need space for routing information
		// also, if this is a control message sent to all, we need
		// to allocate new buffers.
		// There is no problem at rewritting buffers that have non-null buffers,
		// but it is useless if the buffer is used once.
		// So numberOfMessages==m_size is just an optimization.
		if(forceMemoryReallocation
				|| ( aMessage->getBuffer()==NULL||numberOfMessages==m_size)) {

			#ifdef CONFIG_ASSERT
			assert(forceMemoryReallocation || aMessage->getCount()==0||numberOfMessages==m_size);
			#endif

			char * buffer=(char*)m_outboxAllocator.allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

			#ifdef CONFIG_ASSERT
			assert(buffer!=NULL);
			#endif

			// copy the old stuff too
			if(aMessage->getBuffer()!=NULL) {

				memcpy(buffer, aMessage->getBuffer(), aMessage->getNumberOfBytes() * sizeof(char));
			}

			aMessage->setBuffer(buffer);
		}
	}


// run some assertions.

/*
 * What follows is all the crap for checking some assertions and
 * profiling events.
 * The code that actually sends something is at the end
 * of this method.
 */
	// assert that we did not overflow the ring
	#ifdef CONFIG_ASSERT
	if(m_outboxAllocator.getCount() > m_maximumAllocatedOutboxBuffers){
		cout<<"Rank "<<m_rank<<" Error, allocated "<<m_outboxAllocator.getCount()<<" buffers, but maximum is ";
		cout<<m_maximumAllocatedOutboxBuffers<<endl;
		cout<<" outboxSize= "<<m_outbox.size()<<endl;
		cout<<"This means that too many messages were created in this time slice."<<endl;
	}

	assert(m_outboxAllocator.getCount()<=m_maximumAllocatedOutboxBuffers);

	int messagesToSend=m_outbox.size();
	if(messagesToSend>m_maximumNumberOfOutboxMessages){
		cout<<"Fatal: "<<messagesToSend<<" messages to send, but max is "<<m_maximumNumberOfOutboxMessages<<endl;
		cout<<"tags=";
		for(int i=0;i<(int)m_outbox.size();i++){
			MessageTag tag=m_outbox[i]->getTag();
			cout<<" "<<MESSAGE_TAGS[tag]<<" handle= "<<tag<<endl;
		}
		cout<<endl;
	}

	assert(messagesToSend<=m_maximumNumberOfOutboxMessages);
	if(messagesToSend>m_maximumNumberOfOutboxMessages){
		uint8_t tag=m_outbox[0]->getTag();
		cout<<"Tag="<<tag<<" n="<<messagesToSend<<" max="<<m_maximumNumberOfOutboxMessages<<endl;
	}

	#endif

///////////////////////----------------


	// old useless code
#ifdef OLD_ACTOR_META_CODE
	// register actor meta-data

	for(int i = 0 ; i < (int) m_outbox.size() ; ++i) {
		Message * message = m_outbox.at(i);

		// we need a buffer to store the actor metadata
		if(message->getBuffer() == NULL) {
			void * buffer = m_outboxAllocator.allocate(0);
			message->setBuffer(buffer);
		}

		/*
		cout << "DEBUG Before saving ";
		message->printActorMetaData();
		cout << endl;
		*/

		// save actor metadata
		// routed message already have this
		/*if(!m_router.isRoutingTag(message->getTag()))
			message->saveActorMetaData();
			*/

		/*
		cout << "DEBUG After saving ";
		message->printActorMetaData();
		cout << endl;
		*/


	}
#endif


	// route messages if the router is enabled
	if(m_routerIsEnabled){
		// if message routing is enabled,
		// generate routing tags.
		m_router.routeOutcomingMessages();
	}

	// save metadata
	for(int i=0;i<(int)m_outbox.size();i++){

		Message * message = m_outbox.at(i);

		// TODO: only save routing metadata if routing is activated !
		// this will save 8 bytes per transport operation

		// save the minirank number because the mini-rank subsystem needs that

		if(m_miniRanksAreEnabled) {
			message->setMiniRanks(message->getSource(), message->getDestination());
		}

		message->saveMetaData();

#ifdef CONFIG_ASSERT
		testMessage(message);
#endif
	}

	// add checksums if necessary
	if(m_doChecksum){
		addMessageChecksums();
	}


	// parameters.showCommunicationEvents()
	if( m_showCommunicationEvents && m_outbox.size() > 0){
		uint64_t microseconds=getMicroseconds() - m_startingTimeMicroseconds;
		for(int i=0;i<(int)m_outbox.size();i++){
			cout<<"[Communication] "<<microseconds<<" microseconds, SEND ";
			m_outbox[i]->print();
			cout<<endl;
		}
	}

	for(int i=0;i<(int)m_outbox.size();i++){
		m_tickLogger.logSendMessage(INVALID_HANDLE);

#ifdef GITHUB_ISSUE_220
		Message * message = m_outbox.at(i);

		if(message->getTag() == 17 && message->getDestination() == 2) {

			cout << "DEBUG before sending inside mini-rank" << endl;
			message->print();
		}
#endif
	}


	#ifdef CONFIG_ASSERT

	for(int i=0;i<(int)m_outbox.size();i++){

		Message * message = m_outbox.at(i);

#ifdef CONFIG_ASSERT
		testMessage(message);
#endif

//#define INVESTIGATION_2013_11_04
#ifdef INVESTIGATION_2013_11_04
		if(message->getTag() == 17) {

			cout << "DEBUG INVESTIGATION_2013_11_04 ";
			cout << (void*) message->getBufferBytes();
			cout << endl;
		}
#endif


	}

#endif

	// finally, send the messages

	if(!m_miniRanksAreEnabled){
/*
 * Implement the old communication mode too.
 */
		m_messagesHandler->sendMessages(&m_outbox,&m_outboxAllocator);
	}else{

		m_messagesHandler->sendMessagesForComputeCore(&m_outbox,&m_bufferedOutbox);
	}

	m_outbox.clear();
}

void ComputeCore::addMessageChecksums(){

	int count=m_outbox.size();

	for(int i=0;i<count;i++){

		char * data=m_outbox.at(i)->getBufferBytes();
		int numberOfBytes = m_outbox.at(i)->getNumberOfBytes();

		/*
		// don't compute checksum for empty messages
		if(bytes == 0){
			continue;
		}
		*/

		uint8_t*bytes=(uint8_t*)data;

		uint32_t crc32=computeCyclicRedundancyCode32(bytes,numberOfBytes);

		memcpy(bytes + numberOfBytes, &crc32, sizeof(crc32));

		m_outbox.at(i)->setNumberOfBytes(numberOfBytes + sizeof(crc32));
	}

}

void ComputeCore::verifyMessageChecksums(){

	int count=m_inbox.size();
	for(int i=0;i<count;i++){

		Message * message = m_inbox.at(i);

		uint32_t crc32 = 0;
		char * data = message->getBufferBytes();

#ifdef CONFIG_ASSERT
		assert(message->getNumberOfBytes() >= (int) sizeof(crc32));
#endif

		message->setNumberOfBytes(message->getNumberOfBytes() - sizeof(crc32));

#ifdef CONFIG_ASSERT
		assert(message->getNumberOfBytes() >= 0);
#endif

		// the last Message Unit contains the crc32.
		// note that the CRC32 feature only works for Message Unit at least
		// 32 bits.
		int numberOfBytes = m_inbox.at(i)->getNumberOfBytes();

		uint8_t*bytes=(uint8_t*)data;

		// compute the checksum, excluding the reference checksum
		// from the data
		crc32 = computeCyclicRedundancyCode32((uint8_t*)bytes, numberOfBytes);

		uint32_t expectedChecksum = 0;

		memcpy(&expectedChecksum, data + numberOfBytes,
			sizeof(expectedChecksum));

		#ifdef CONFIG_SHOW_CHECKSUM
		cout<<" Expected checksum (CRC32): "<<hex<<expectedChecksum<<endl;
		cout<<" Actual checksum (CRC32): "<<hex<<crc32<<dec<<endl;
		#endif

		if(crc32 != expectedChecksum){
			Message*message=m_inbox.at(i);
			cout<<"Error: RayPlatform detected a message corruption !"<<endl;
			cout<<" Tag: " << message->getTag() << endl;
			cout<<" Source: "<<message->getSource()<<endl;
			cout<<" Destination: "<<message->getDestination()<<endl;
			cout<<" Bytes (excluding checksum): "<< numberOfBytes << endl;
			cout<<" Expected checksum (CRC32): "<<hex<<expectedChecksum<<endl;
			cout<<" Actual checksum (CRC32): "<<hex<<crc32<<dec<<endl;
		}

	}

}

/**
 * receivedMessages receives 0 or 1 messages.
 * If more messages are available to be pumped, they will wait until the
 * next ComputeCore cycle.
 */
void ComputeCore::receiveMessages(){

/*
 * clear the inbox for the next iteration
 */
	m_inbox.clear();

	if(!m_miniRanksAreEnabled){
/*
 * Implement the old communication model.
 */
		m_messagesHandler->receiveMessages(&m_inbox,&m_inboxAllocator);
	}else{

		m_messagesHandler->receiveMessagesForComputeCore(&m_inbox,&m_inboxAllocator,&m_bufferedInbox);

	}

	if(m_inbox.size() == 0)
		return;

	// load metadata.

	//cout << "DEBUG loading message metadata." << endl;

	Message * importantMessage = m_inbox.at(0);

	// verify checksums and remove them
	// messages are line onions, you have to peel them from the outside to the inside
	if(m_doChecksum){
		verifyMessageChecksums();
	}

	importantMessage->loadMetaData();
	importantMessage->saveMetaData();

#ifdef GITHUB_ISSUE_220
	if(importantMessage->getTag() == 17 && getRank() == 2) {
		cout << "DEBUG ComputeCore receives message!" << endl;
		importantMessage->print();
	}
#endif

#ifdef CONFIG_ASSERT
	testMessage(importantMessage);

	//testMessage(importantMessage);
#endif

	for(int i=0;i<(int)m_inbox.size();i++){
		m_tickLogger.logReceivedMessage(INVALID_HANDLE);
	}

	#ifdef CONFIG_ASSERT
	int receivedMessages=m_inbox.size();

	if(receivedMessages>m_maximumNumberOfInboxMessages)
		cout<<"[ComputeCore::receiveMessages] inbox has "<<receivedMessages<<" but maximum is "<<m_maximumNumberOfInboxMessages<<endl;

	assert(receivedMessages<=m_maximumNumberOfInboxMessages);
	#endif

	// remove the header now !
	importantMessage->loadMetaData();
}

/** process data my calling current slave and master methods */
void ComputeCore::processData(){

	// call the master method first
	MasterMode master=m_switchMan.getMasterMode();

	#ifdef RayPlatform_ASSERT
	assert(master!=INVALID_HANDLE);
	assert(m_allocatedMasterModes.count(master)>0);
	string masterSymbol=MASTER_MODES[master];
	assert(m_masterModeSymbols.count(masterSymbol)>0);
	assert(m_masterModeSymbols[masterSymbol]==master);
	#endif

	#ifdef CONFIG_DEBUG_processData
	cout<<"master mode -> "<<MASTER_MODES[master]<<" handle is "<<master<<endl;
	#endif

	m_masterModeExecutor.callHandler(master);
	m_tickLogger.logMasterTick(master);

	// then call the slave method
	SlaveMode slave=m_switchMan.getSlaveMode();

	#ifdef RayPlatform_ASSERT
	assert(slave!=INVALID_HANDLE);
	assert(m_allocatedSlaveModes.count(slave)>0);
	string slaveSymbol=SLAVE_MODES[slave];
	assert(m_slaveModeSymbols.count(slaveSymbol)>0);
	assert(m_slaveModeSymbols[slaveSymbol]==slave);
	#endif

	#ifdef CONFIG_DEBUG_processData
	cout<<"slave mode -> "<<SLAVE_MODES[slave]<<" handle is "<<slave<<endl;
	#endif

	m_slaveModeExecutor.callHandler(slave);
	m_tickLogger.logSlaveTick(slave);

/*
 * Clear the inbox for the next iteration.
 * This is only useful if the RayPlatform runs with mini-ranks.
 */
	m_inbox.clear();
}

void ComputeCore::constructor(int argc,char**argv,int miniRankNumber,int numberOfMiniRanks,int miniRanksPerRank,
		MessagesHandler*messagesHandler){

	#ifdef CONFIG_ASSERT
	assert(miniRankNumber<numberOfMiniRanks);
	assert(miniRanksPerRank<=numberOfMiniRanks);
	assert(numberOfMiniRanks>=1);
	assert(miniRanksPerRank>=1);
	assert(miniRankNumber>=0);
	#endif

	bool useMiniRanks=miniRanksPerRank>1;

	m_numberOfMiniRanksPerRank=miniRanksPerRank;
	m_messagesHandler=messagesHandler;

	m_destroyed=false;

	m_doChecksum=false;

	m_miniRanksAreEnabled=useMiniRanks;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"[ComputeCore::constructor] m_miniRanksAreEnabled= "<<m_miniRanksAreEnabled<<endl;
	#endif

	// checksum calculation is only tested
	// for cases with sizeof(Message Unit)>=4 bytes

	m_routerIsEnabled=false;

	m_argumentCount=argc;
	m_argumentValues=argv;

	m_resolvedSymbols=false;

	m_hasFatalErrors=false;

	m_firstRegistration=true;

	m_hasFirstMode=false;

	srand(portableProcessId() * getMicroseconds());

	m_alive=true;

	m_runProfiler=false;
	m_showCommunicationEvents=false;
	m_profilerVerbose=false;

	m_rank=miniRankNumber;
	m_size=numberOfMiniRanks;

	for(int i=0;i<MAXIMUM_NUMBER_OF_MASTER_HANDLERS;i++){
		strcpy(MASTER_MODES[i],"UnnamedMasterMode");
	}
	for(int i=0;i<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS;i++){
		strcpy(SLAVE_MODES[i],"UnnamedSlaveMode");
	}
	for(int i=0;i<MAXIMUM_NUMBER_OF_TAG_HANDLERS;i++){
		strcpy(MESSAGE_TAGS[i],"UnnamedMessageTag");
	}

	m_currentSlaveModeToAllocate=0;
	m_currentMasterModeToAllocate=0;
	m_currentMessageTagToAllocate=0;
}

void ComputeCore::configureEngine() {

	// set the number of buffers to use
	int minimumNumberOfBuffers=128;

	int availableBuffers=minimumNumberOfBuffers;

	/**
	 * Instead of limiting the buffers, just use a sane amount already.
	 *
	 * The VirtualCommunicator layer will use one group per tag-rank tuple anyway.
	 * So using more buffers does not change anything.
	 *
	 * At 4096 ranks, this will use 32768000 bytes, or roughly 32 MiB.
	 *
	 * ***************************
	 *
	 * On the IBM Blue Gene/Q in Toronto, using 1024 nodes provides 16 TiB of memory.
	 * At the moment, the highly symmetric identity mapping in the virtual
	 * memory layer on BGQ  prohibits Ray at low memory ranges.
	 *
	 * This is because most of Ray processes uses < 800 MiB, but a few rogue
	 * processes sit sometimes at 2.5 GiB. Rank 0 is one of them since it is the
	 * master of the tribe. If 0 dies, so does everyone.
	 *
	 * Access to bgq-fen0 is provided by SciNet. The hardware is owned by
	 * SOSCIP | Southern Ontario Smart Computing Innovation Platform
	 *
	 * \see https://support.scinet.utoronto.ca/wiki/index.php/BGQ
	 * \see http://soscip.org/
	 *
	 * ***************************
	 *
	 * On the Cray XE6 at Cray, Inc. (collaboration with Cray, Inc. with Carlos Sosa), there was
	 * a segmentation fault on 2048 ranks.
	 *
	 * This was most likely due to this lack of buffers. Because of this depletion,
	 * buffers were reused.
	 */
	bool useMoreBuffers = true;

	// even a message with a NULL buffer requires a buffer for routing
	if(useMoreBuffers || m_routerIsEnabled || m_miniRanksAreEnabled)
		availableBuffers=m_size*2;

	// this will occur when using the virtual router with a few processes
	if(availableBuffers<minimumNumberOfBuffers)
		availableBuffers=minimumNumberOfBuffers;

	m_switchMan.constructor(m_rank,m_size);

	m_virtualCommunicator.constructor(m_rank,m_size,&m_outboxAllocator,&m_inbox,&m_outbox);

	m_keyValueStore.initialize(m_rank, m_size, &m_outboxAllocator, &m_inbox, &m_outbox);

	/***********************************************************************************/
	/** initialize the VirtualProcessor */
	m_virtualProcessor.constructor(&m_outbox,&m_inbox,&m_outboxAllocator,
		&m_virtualCommunicator);

	m_maximumAllocatedInboxBuffers=1;
	m_maximumAllocatedOutboxBuffers=availableBuffers;

	m_maximumNumberOfInboxMessages=1;
	m_maximumNumberOfOutboxMessages=availableBuffers;

	// broadcasting messages
	// this case will occur when the number of requested outbox buffers is < the number of processor cores.
	if(m_maximumNumberOfOutboxMessages < m_size){
		m_maximumNumberOfOutboxMessages=m_size;
	}

	getInbox()->constructor(m_maximumNumberOfInboxMessages,"RAY_MALLOC_TYPE_INBOX_VECTOR",false);
	getOutbox()->constructor(m_maximumNumberOfOutboxMessages,"RAY_MALLOC_TYPE_OUTBOX_VECTOR",false);

	if(m_miniRanksAreEnabled){

		getBufferedInbox()->constructor(m_maximumNumberOfOutboxMessages);
		getBufferedOutbox()->constructor(m_maximumNumberOfOutboxMessages);
	}

	#ifdef CONFIG_DEBUG_CORE
	cout<<"[ComputeCore] the inbox capacity is "<<m_maximumNumberOfInboxMessages<<" message"<<endl;
	cout<<"[ComputeCore] the outbox capacity is "<<m_maximumNumberOfOutboxMessages<<" message"<<endl;
	#endif

	int maximumMessageSizeInBytes = MAXIMUM_MESSAGE_SIZE_IN_BYTES;

	bool useActorModel = true;

	// add a message unit to store the checksum or the routing information
	// with 64-bit integers as Message Unit, this is 4008 bytes or 501 Message Unit maximum
	// also add 8 bytes for actor metadata
	// finally, add 4 bytes for the checksum, if necessary.

	if(m_miniRanksAreEnabled || m_doChecksum || m_routerIsEnabled || useActorModel){

		int headerSize = 0;

		headerSize = MESSAGE_META_DATA_SIZE;

		/*
		// actor model
		headerSize += ( 2 * sizeof(int) * sizeof(char) );

		// routing
		headerSize += ( 2 * sizeof(int) * sizeof(char));

		// checksum
		headerSize += ( 1 * sizeof(uint32_t) * sizeof(char) );
*/

/*
		cout << "DEBUG MAXIMUM_MESSAGE_SIZE_IN_BYTES " << MAXIMUM_MESSAGE_SIZE_IN_BYTES;
		cout << " headerSize " << headerSize << endl;
		*/

		// align this on 8 bytes because there is still something in the code
		// that is incorrect apparently.
		//
		// TODO: find why this needs to be aligned on 8 bytes. This is related to 
		// sizeof(MessageUnit) being 8 bytes.
		//
		// => it is likely because of the underlying system that evolved to
		// use 64 bits per message unit.

		bool align = true;
		//align = false;

		int base = 8;
		int remainder = headerSize % base;

		if(align && remainder != 0) {

			int toAdd = base - remainder;

			headerSize += toAdd;
		}

		//cout << " headerSize (with padding) " << headerSize << endl;

		maximumMessageSizeInBytes += headerSize;
	}

	//cout << "DEBUG maximumMessageSizeInBytes " << maximumMessageSizeInBytes << endl;

#ifdef CONFIG_ASSERT
	assert(maximumMessageSizeInBytes >= MAXIMUM_MESSAGE_SIZE_IN_BYTES);
	assert(maximumMessageSizeInBytes >= MAXIMUM_MESSAGE_SIZE_IN_BYTES + 20);// we need 20 bytes for storing message header
#endif

	m_inboxAllocator.constructor(m_maximumAllocatedInboxBuffers,
		maximumMessageSizeInBytes,
		"RAY_MALLOC_TYPE_INBOX_ALLOCATOR",false);

	m_outboxAllocator.constructor(m_maximumAllocatedOutboxBuffers,
		maximumMessageSizeInBytes,
		"RAY_MALLOC_TYPE_OUTBOX_ALLOCATOR",false);

	if(m_miniRanksAreEnabled){

		#if 0
		cout<<"Building buffered inbox allocator."<<endl;
		#endif

		m_bufferedInboxAllocator.constructor(m_maximumAllocatedOutboxBuffers,
			maximumMessageSizeInBytes,
			"RAY_MALLOC_TYPE_INBOX_ALLOCATOR/Buffered",false);

		m_bufferedOutboxAllocator.constructor(m_maximumAllocatedOutboxBuffers,
			maximumMessageSizeInBytes,
			"RAY_MALLOC_TYPE_OUTBOX_ALLOCATOR/Buffered",false);

	}

	#ifdef CONFIG_DEBUG_CORE
	cout<<"[ComputeCore] allocated "<<m_maximumAllocatedInboxBuffers<<" buffers of size "<<maximumMessageSizeInBytes<<" for inbox messages"<<endl;
	cout<<"[ComputeCore] allocated "<<m_maximumAllocatedOutboxBuffers<<" buffers of size "<<maximumMessageSizeInBytes<<" for outbox messages"<<endl;
	#endif

}

void ComputeCore::enableCheckSums(){
	m_doChecksum=true;
}

void ComputeCore::setMiniRank(int miniRank,int numberOfMiniRanks){

	m_rank=miniRank;
	m_size=numberOfMiniRanks;
}

void ComputeCore::enableProfiler(){
	m_runProfiler=true;
}

void ComputeCore::showCommunicationEvents(){
	m_showCommunicationEvents=true;
}

void ComputeCore::enableProfilerVerbosity(){
	m_profilerVerbose=true;
}

Profiler*ComputeCore::getProfiler(){
	return &m_profiler;
}

TickLogger*ComputeCore::getTickLogger(){
	return &m_tickLogger;
}

SwitchMan*ComputeCore::getSwitchMan(){
	return &m_switchMan;
}

StaticVector*ComputeCore::getOutbox(){
	return &m_outbox;
}

StaticVector*ComputeCore::getInbox(){
	return &m_inbox;
}

MessageRouter*ComputeCore::getRouter(){
	return &m_router;
}

RingAllocator*ComputeCore::getOutboxAllocator(){
	return &m_outboxAllocator;
}

RingAllocator*ComputeCore::getInboxAllocator(){
	return &m_inboxAllocator;
}

bool*ComputeCore::getLife(){
	return &m_alive;
}

VirtualProcessor*ComputeCore::getVirtualProcessor(){
	return &m_virtualProcessor;
}

VirtualCommunicator*ComputeCore::getVirtualCommunicator(){
	return &m_virtualCommunicator;
}

void ComputeCore::registerPlugin(CorePlugin*plugin){

	if(m_firstRegistration){

		m_firstRegistration=false;

		// register some built-in plugins

		registerPlugin(&m_switchMan);
		registerPlugin(&m_keyValueStore);

	}

	m_listOfPlugins.push_back(plugin);

	plugin->registerPlugin(this);
}

void ComputeCore::resolveSymbols(){
	if(m_resolvedSymbols)
		return;

	registerDummyPlugin();

	for(int i=0;i<(int)m_listOfPlugins.size();i++){
		CorePlugin*plugin=m_listOfPlugins[i];
		plugin->resolveSymbols(this);
	}

	m_resolvedSymbols=true;
}

void ComputeCore::registerDummyPlugin(){
	ComputeCore*core=this;
	PluginHandle plugin=core->allocatePluginHandle();

	core->setPluginName(plugin,"DummySun");
	core->setPluginDescription(plugin,"Dummy plugin, does nothing.");
	core->setPluginAuthors(plugin,"Sébastien Boisvert");
	core->setPluginLicense(plugin,"GNU Lesser General License version 3");

	RAY_MPI_TAG_DUMMY=core->allocateMessageTagHandle(plugin);
	core->setMessageTagSymbol(plugin,RAY_MPI_TAG_DUMMY,"RAY_MPI_TAG_DUMMY");

}

void ComputeCore::destructor(){

	#if 0
	cout<<"ComputeCore::destructor"<<endl;
	#endif

	if(m_destroyed)
		return;

	m_destroyed=true;

	#ifdef CONFIG_DEBUG_CORE
	cout<<"Rank "<<m_rank<<" is cleaning the inbox allocator."<<endl;
	#endif

	m_inboxAllocator.clear();

	if(m_miniRanksAreEnabled)
		m_bufferedInboxAllocator.clear();

	#ifdef CONFIG_DEBUG_CORE
	cout<<"Rank "<<m_rank<<" is cleaning the outbox allocator."<<endl;
	#endif

	m_outboxAllocator.clear();

	if(m_miniRanksAreEnabled)
		m_bufferedOutboxAllocator.clear();

	if(m_miniRanksAreEnabled){
		m_bufferedInbox.destructor();
		m_bufferedOutbox.destructor();
	}
}

void ComputeCore::stop(){
	m_alive=false;
}

void ComputeCore::setSlaveModeSymbol(PluginHandle plugin,SlaveMode mode,const char*symbol){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationSlaveModeRange(plugin,mode))
		return;

	if(!validationSlaveModeOwnership(plugin,mode)){
		cout<<"was trying to set symbol "<<symbol<<" to mode "<<mode<<endl;
		return;
	}

	if(!validationSlaveModeSymbolAvailable(plugin,symbol)){
		return;
	}

	if(!validationSlaveModeSymbolNotRegistered(plugin,mode))
		return;

	if(!validationSlaveModeRange(plugin,mode))
		return;

	#ifdef CONFIG_ASSERT
	assert(mode>=0);
	assert(mode<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS);
	#endif

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasSlaveMode(mode));
	#endif

	strcpy(SLAVE_MODES[mode],symbol);

	m_plugins[plugin].addRegisteredSlaveModeSymbol(mode);

	m_registeredSlaveModeSymbols.insert(mode);

	m_slaveModeSymbols[symbol]=mode;

	#ifdef CONFIG_DEBUG_SLAVE_SYMBOLS
	cout<<"Registered slave mode "<<mode<<" to symbol "<<symbol<<endl;
	#endif
}

void ComputeCore::setMasterModeSymbol(PluginHandle plugin,MasterMode mode,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	if(!validationMasterModeSymbolAvailable(plugin,symbol)){
		return;
	}

	if(!validationMasterModeSymbolNotRegistered(plugin,mode))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	#ifdef CONFIG_ASSERT
	assert(mode>=0);
	assert(mode<MAXIMUM_NUMBER_OF_MASTER_HANDLERS);
	#endif

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMasterMode(mode));
	#endif

	strcpy(MASTER_MODES[mode],symbol);

	m_plugins[plugin].addRegisteredMasterModeSymbol(mode);

	m_registeredMasterModeSymbols.insert(mode);

	m_masterModeSymbols[symbol]=mode;
}

void ComputeCore::setMessageTagSymbol(PluginHandle plugin,MessageTag tag,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	if(!validationMessageTagSymbolAvailable(plugin,symbol))
		return;

	if(!validationMessageTagSymbolNotRegistered(plugin,tag))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	#ifdef CONFIG_ASSERT
	assert(tag>=0);
	assert(tag<MAXIMUM_NUMBER_OF_TAG_HANDLERS);
	#endif

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	assert(m_plugins[plugin].hasMessageTag(tag));
	#endif

	strcpy(MESSAGE_TAGS[tag],symbol);

	m_plugins[plugin].addRegisteredMessageTagSymbol(tag);

	m_registeredMessageTagSymbols.insert(tag);

	m_messageTagSymbols[symbol]=tag;

	if(debugModeIsEnabled()) {

		cout << "DEBUG----- <MessageTag><Name>";
		cout << symbol;
		cout << "</Name><Value>";
		cout << tag;
		cout << "</Value></MessageTag>";
		cout << endl;
	}
}

PluginHandle ComputeCore::allocatePluginHandle(){
	PluginHandle handle=generatePluginHandle();

	while(m_plugins.count(handle)>0){
		handle=generatePluginHandle();
	}

	RegisteredPlugin plugin;

	m_plugins[handle]=plugin;

	return handle;
}

SlaveMode ComputeCore::allocateSlaveModeHandle(PluginHandle plugin){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;


	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	#endif

	SlaveMode handle=m_currentSlaveModeToAllocate;

	if(!(0<=handle && handle<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS))
		handle=m_currentSlaveModeToAllocate;

	while(m_allocatedSlaveModes.count(handle)>0){
		handle=m_currentSlaveModeToAllocate++;
	}

	if(!validationSlaveModeRange(plugin,handle))
		return INVALID_HANDLE;

	#ifdef CONFIG_ASSERT
	assert(m_allocatedSlaveModes.count(handle)==0);
	#endif

	m_allocatedSlaveModes.insert(handle);

	m_plugins[plugin].addAllocatedSlaveMode(handle);

	#ifdef CONFIG_ASSERT
	assert(m_allocatedSlaveModes.count(handle)>0);
	assert(m_plugins[plugin].hasSlaveMode(handle));
	#endif

	return handle;
}

MasterMode ComputeCore::allocateMasterModeHandle(PluginHandle plugin){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	#endif

	MasterMode handle=m_currentMasterModeToAllocate;

	if(!(0<=handle && handle<MAXIMUM_NUMBER_OF_MASTER_HANDLERS))
		handle=m_currentMasterModeToAllocate;

	while(m_allocatedMasterModes.count(handle)>0){
		handle=m_currentMasterModeToAllocate++;
	}

	if(!validationMasterModeRange(plugin,handle))
		return INVALID_HANDLE;

	#ifdef CONFIG_ASSERT
	assert(m_allocatedMasterModes.count(handle)==0);
	#endif

	m_allocatedMasterModes.insert(handle);

	m_plugins[plugin].addAllocatedMasterMode(handle);

	#ifdef CONFIG_ASSERT
	assert(m_allocatedMasterModes.count(handle)>0);
	assert(m_plugins[plugin].hasMasterMode(handle));
	#endif

	return handle;
}

MessageTag ComputeCore::allocateMessageTagHandle(PluginHandle plugin){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	#ifdef CONFIG_ASSERT
	assert(m_plugins.count(plugin)>0);
	#endif

	MessageTag handle=m_currentMessageTagToAllocate;

	if(!(0<=handle && handle < MAXIMUM_NUMBER_OF_TAG_HANDLERS))
		handle=m_currentMessageTagToAllocate;

	while(m_allocatedMessageTags.count(handle)>0){
		handle=m_currentMessageTagToAllocate++;
	}

	if(!validationMessageTagRange(plugin,handle))
		return INVALID_HANDLE;

	#ifdef CONFIG_ASSERT
	assert(m_allocatedMessageTags.count(handle)==0);
	#endif

	m_allocatedMessageTags.insert(handle);

	m_plugins[plugin].addAllocatedMessageTag(handle);

	#ifdef CONFIG_ASSERT
	assert(m_allocatedMessageTags.count(handle)>0);
	assert(m_plugins[plugin].hasMessageTag(handle));
	#endif

	return handle;
}

PluginHandle ComputeCore::generatePluginHandle(){
	uint64_t randomNumber=rand();

	return uniform_hashing_function_1_64_64(randomNumber);
}

bool ComputeCore::validationPluginAllocated(PluginHandle plugin){
	if(!m_plugins.count(plugin)>0){
		cout<<"Error, plugin "<<plugin<<" is not allocated"<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeOwnership(PluginHandle plugin,SlaveMode handle){
	if(!m_plugins[plugin].hasSlaveMode(handle)){
		cout<<"Error, plugin "<<m_plugins[plugin].getPluginName();
		setFatalError();
		cout<<" ("<<plugin<<") has no ownership on slave mode "<<handle<<endl;
		return false;
	}

	return true;
}

bool ComputeCore::validationMasterModeOwnership(PluginHandle plugin,MasterMode handle){
	if(!m_plugins[plugin].hasMasterMode(handle)){
		cout<<"Error, plugin "<<m_plugins[plugin].getPluginName();
		cout<<" ("<<plugin<<") has no ownership on master mode "<<handle<<endl;
		cout<<" public symbol is "<<MASTER_MODES[handle]<<endl;
		setFatalError();

		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagOwnership(PluginHandle plugin,MessageTag handle){
	if(!m_plugins[plugin].hasMessageTag(handle)){
		cout<<"Error, plugin "<<m_plugins[plugin].getPluginName();
		cout<<" ("<<plugin<<") has no ownership on message tag mode "<<handle<<endl;
		setFatalError();
		return false;
	}

	return true;
}

void ComputeCore::setPluginName(PluginHandle plugin,const char*name){
	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginName(name);
}

void ComputeCore::printPlugins(string directory){

/*
	ostringstream list;
	list<<directory<<"/list.txt";

	ofstream f1(list.str().c_str());

	for(map<PluginHandle,RegisteredPlugin>::iterator i=m_plugins.begin();
		i!=m_plugins.end();i++){

		f1<<i->first<<"	plugin_"<<i->second.getPluginName()<<endl;
	}

	f1.close();
*/

	for(map<PluginHandle,RegisteredPlugin>::iterator i=m_plugins.begin();
		i!=m_plugins.end();i++){

		ostringstream file;
		file<<directory<<"/plugin_"<<i->second.getPluginName()<<".txt";

		ofstream f2(file.str().c_str());
		i->second.print(&f2);
		f2.close();
	}

}

SlaveMode ComputeCore::getSlaveModeFromSymbol(PluginHandle plugin,const char*symbol){
	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	if(!validationSlaveModeSymbolRegistered(plugin,symbol))
		return INVALID_HANDLE;

	string key=symbol;

	if(m_slaveModeSymbols.count(key)>0){
		SlaveMode handle=m_slaveModeSymbols[key];

		m_plugins[plugin].addResolvedSlaveMode(handle);

		#ifdef CONFIG_DEBUG_SLAVE_SYMBOLS
		cout<<"Plugin "<<m_plugins[plugin].getPluginName()<<" resolved symbol "<<symbol<<" to slave mode "<<handle<<endl;
		#endif

		return handle;
	}

	cout<<"Invalid handle returned!"<<endl;
	return INVALID_HANDLE;
}

MasterMode ComputeCore::getMasterModeFromSymbol(PluginHandle plugin,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	if(!validationMasterModeSymbolRegistered(plugin,symbol))
		return INVALID_HANDLE;

	string key=symbol;

	if(m_masterModeSymbols.count(key)>0){
		MasterMode handle=m_masterModeSymbols[key];

		m_plugins[plugin].addResolvedMasterMode(handle);

		#ifdef CONFIG_DEBUG_MASTER_SYMBOLS
		cout<<"symbol "<<symbol<<" is resolved as master mode "<<handle<<endl;
		#endif

		return handle;
	}

	return INVALID_HANDLE;
}

MessageTag ComputeCore::getMessageTagFromSymbol(PluginHandle plugin,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return INVALID_HANDLE;

	if(!validationMessageTagSymbolRegistered(plugin,symbol))
		return INVALID_HANDLE;

	string key=symbol;

	if(m_messageTagSymbols.count(key)>0){
		MessageTag handle=m_messageTagSymbols[key];

		m_plugins[plugin].addResolvedMessageTag(handle);

		#ifdef CONFIG_DEBUG_TAG_SYMBOLS
		cout<<" symbol "<<symbol<<" is resolved as message tag handle "<<handle<<endl;
		#endif

		return handle;
	}

	return INVALID_HANDLE;
}

bool ComputeCore::validationMessageTagSymbolAvailable(PluginHandle plugin,const char*symbol){
	string key=symbol;

	if(m_messageTagSymbols.count(key)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol "<<symbol<<" because it is already registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeSymbolAvailable(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_slaveModeSymbols.count(key)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol "<<symbol<<" because it is already registered."<<endl;
		setFatalError();
		return false;
	}

	return true;

}

bool ComputeCore::validationMasterModeSymbolAvailable(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_masterModeSymbols.count(key)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol "<<symbol<<" because it is already registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagSymbolRegistered(PluginHandle plugin,const char*symbol){
	string key=symbol;

	if(m_messageTagSymbols.count(key)==0){
		cout<<"Error, plugin "<<plugin<<" (name: "<<m_plugins[plugin].getPluginName()<<") can not fetch symbol "<<symbol<<" because it is not registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeSymbolRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_slaveModeSymbols.count(key)==0){
		cout<<"Error, plugin "<<plugin<<" (name: "<<m_plugins[plugin].getPluginName()<<") can not fetch symbol "<<symbol<<" because it is not registered."<<endl;
		setFatalError();
		return false;
	}

	return true;

}

bool ComputeCore::validationMasterModeSymbolRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_masterModeSymbols.count(key)==0){
		cout<<"Error, plugin "<<plugin<<" (name: "<<m_plugins[plugin].getPluginName()<<") can not fetch symbol "<<symbol<<" because it is not registered."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagSymbolNotRegistered(PluginHandle plugin,MessageTag handle){

	if(m_registeredMessageTagSymbols.count(handle)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol for message tag "<<handle <<" because it already has a symbol."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeSymbolNotRegistered(PluginHandle plugin,SlaveMode handle){

	if(m_registeredSlaveModeSymbols.count(handle)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol for slave mode "<<handle <<" because it already has a symbol."<<endl;
		setFatalError();
		return false;
	}

	return true;

}

bool ComputeCore::validationMasterModeSymbolNotRegistered(PluginHandle plugin,MasterMode handle){

	if(m_registeredMasterModeSymbols.count(handle)>0){
		cout<<"Error, plugin "<<plugin<<" can not register symbol for master mode "<<handle <<" because it already has a symbol."<<endl;
		setFatalError();
		return false;
	}

	return true;
}

void ComputeCore::setPluginDescription(PluginHandle plugin,const char*a){

	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginDescription(a);
}

void ComputeCore::setMasterModeToMessageTagSwitch(PluginHandle plugin,MasterMode mode,MessageTag tag){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	m_switchMan.addMasterSwitch(mode,tag);

	m_plugins[plugin].addRegisteredMasterModeToMessageTagSwitch(mode);
}

void ComputeCore::setPluginAuthors(PluginHandle plugin,const char*text){
	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginAuthors(text);
}

void ComputeCore::setPluginLicense(PluginHandle plugin,const char*text){
	if(!validationPluginAllocated(plugin))
		return;

	m_plugins[plugin].setPluginLicense(text);

}

void ComputeCore::setMessageTagToSlaveModeSwitch(PluginHandle plugin,MessageTag tag,SlaveMode mode){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	m_switchMan.addSlaveSwitch(tag,mode);

	m_plugins[plugin].addRegisteredMessageTagToSlaveModeSwitch(tag);

}

void ComputeCore::setMessageTagReplyMessageTag(PluginHandle plugin,MessageTag tag,MessageTag reply){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagRange(plugin,reply))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	m_virtualCommunicator.setReplyType(tag,reply);

	m_plugins[plugin].addRegisteredMessageTagReplyMessageTag(tag);
}

void ComputeCore::setMessageTagSize(PluginHandle plugin,MessageTag tag,int size){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMessageTagRange(plugin,tag))
		return;

	if(!validationMessageTagOwnership(plugin,tag))
		return;

	m_virtualCommunicator.setElementsPerQuery(tag,size);

	m_plugins[plugin].addRegisteredMessageTagSize(tag);
}

void ComputeCore::setMasterModeNextMasterMode(PluginHandle plugin,MasterMode current,MasterMode next){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,current))
		return;

	if(!validationMasterModeRange(plugin,next))
		return;

	if(!validationMasterModeOwnership(plugin,current))
		return;

	m_switchMan.addNextMasterMode(current,next);

	// configure the switch man
	//
	// this is where steps can be added or removed.

	m_plugins[plugin].addRegisteredMasterModeNextMasterMode(current);
}

bool ComputeCore::isPublicMasterMode(PluginHandle plugin,MasterMode mode){
	return m_publicMasterModes.count(mode)>0;
}

void ComputeCore::setMasterModePublicAccess(PluginHandle plugin,MasterMode mode){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!validationMasterModeOwnership(plugin,mode))
		return;

	m_publicMasterModes.insert(mode);
}

void ComputeCore::setFirstMasterMode(PluginHandle plugin,MasterMode mode){
	if(!validationPluginAllocated(plugin))
		return;

	if(!validationMasterModeRange(plugin,mode))
		return;

	if(!isPublicMasterMode(plugin,mode) && !validationMasterModeOwnership(plugin,mode))
		return;

	if(m_hasFirstMode)
		cout<<"Error, already has a first master mode."<<endl;

	m_hasFirstMode=true;

	/** the computation will start there **/
	if(m_rank == MASTER_RANK){
		m_switchMan.setMasterMode(mode);
	}

	m_plugins[plugin].addRegisteredFirstMasterMode(mode);
}

string ComputeCore::getRayPlatformVersion(){

#ifndef RAYPLATFORM_VERSION
#define RAYPLATFORM_VERSION "Unknown-RayPlatform-Version"
#endif

	return RAYPLATFORM_VERSION;
}

bool ComputeCore::validationMasterModeRange(PluginHandle plugin,MasterMode mode){
	if(!(0<= mode && mode < MAXIMUM_NUMBER_OF_MASTER_HANDLERS)){
		cout<<"Error, master mode "<<mode<<" is not in the allowed range."<<endl;
		cout<<"MAXIMUM_NUMBER_OF_MASTER_HANDLERS= "<<MAXIMUM_NUMBER_OF_MASTER_HANDLERS<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationSlaveModeRange(PluginHandle plugin,SlaveMode mode){
	if(!(0<= mode && mode < MAXIMUM_NUMBER_OF_SLAVE_HANDLERS)){
		cout<<"Error, slave mode "<<mode<<" is not in the allowed range."<<endl;
		cout<<"MAXIMUM_NUMBER_OF_SLAVE_HANDLERS= "<<MAXIMUM_NUMBER_OF_SLAVE_HANDLERS<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationMessageTagRange(PluginHandle plugin,MessageTag tag){
	if(!(0<= tag && tag < MAXIMUM_NUMBER_OF_TAG_HANDLERS)){
		cout<<"Error, message tag "<<tag<<" is not in the allowed range."<<endl;
		cout<<"MAXIMUM_NUMBER_OF_TAG_HANDLERS= "<<MAXIMUM_NUMBER_OF_TAG_HANDLERS<<endl;
		setFatalError();
		return false;
	}

	return true;
}

void ComputeCore::setFatalError(){
	cout<<"A fatal error occurred !"<<endl;
	m_hasFatalErrors=true;
}

void ComputeCore::sendEmptyMessageToAll(MessageTag tag){
	m_switchMan.sendToAll(&m_outbox,m_rank,tag);
}

void ComputeCore::setObjectSymbol(PluginHandle plugin,void*object,const char* symbol){

	if(!validationPluginAllocated(plugin))
		return;

	if(!validationObjectSymbolNotRegistered(plugin,symbol))
		return;

	int handle=m_objects.size();

	m_objects.push_back(object);

	m_objectSymbols[symbol]=handle;

	m_plugins[plugin].addAllocatedObject(handle);
	m_plugins[plugin].addRegisteredObjectSymbol(handle);
}

void* ComputeCore::getObjectFromSymbol(PluginHandle plugin,const char*symbol){

	if(!validationPluginAllocated(plugin))
		return NULL;

	if(!validationObjectSymbolRegistered(plugin,symbol))
		return NULL;

	int handle=m_objectSymbols[symbol];

	void*object=m_objects[handle];

	m_plugins[plugin].addResolvedObject(handle);

	return object;
}

bool ComputeCore::validationObjectSymbolRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_objectSymbols.count(key)==0){
		cout<<"object symbol not registered, symbol: "<<symbol<<endl;
		setFatalError();
		return false;
	}

	return true;
}

bool ComputeCore::validationObjectSymbolNotRegistered(PluginHandle plugin,const char*symbol){

	string key=symbol;

	if(m_objectSymbols.count(key)>0){
		cout<<"object symbol already registered, symbol: "<<symbol<<endl;
		setFatalError();
		return false;
	}

	return true;
}

int ComputeCore::getNumberOfArguments(){
	return m_argumentCount;
}

char**ComputeCore::getArgumentValues(){
	return m_argumentValues;
}

bool ComputeCore::hasFinished(){
	bool alive=*(getLife());

	return !alive;
}

int ComputeCore::getRank() const{
	return m_rank;
}

int ComputeCore::getSize() const{
	return m_size;
}

MessageQueue*ComputeCore::getBufferedInbox(){
	return &m_bufferedInbox;
}

MessageQueue*ComputeCore::getBufferedOutbox(){
	return &m_bufferedOutbox;
}

RingAllocator*ComputeCore::getBufferedOutboxAllocator(){
	return &m_bufferedOutboxAllocator;
}

RingAllocator*ComputeCore::getBufferedInboxAllocator(){
	return &m_bufferedInboxAllocator;
}

int ComputeCore::getMiniRanksPerRank(){
	return m_numberOfMiniRanksPerRank;
}

MessagesHandler*ComputeCore::getMessagesHandler(){
	return m_messagesHandler;
}

void ComputeCore::closeSlaveModeLocally() {

	getSwitchMan()->closeSlaveModeLocally(getOutbox(), getRank());
}

KeyValueStore & ComputeCore::getKeyValueStore() {

	return m_keyValueStore;
}

#if 0
void ComputeCore::runRayPlatformTerminal() {

	time_t seconds = time(NULL);

	int delay = seconds - m_lastTerminalProbeOperation;
	int minimumDelay = 60;

	if(delay > minimumDelay) {

	}
}
#endif

Playground * ComputeCore::getPlayground() {

	return & m_playground;
}

void ComputeCore::spawnActor(Actor * actor) {

	getPlayground()->spawnActor(actor);
}

void ComputeCore::send(Message * message) {

	m_outbox.push_back(message);
}

bool ComputeCore::isRankAlive() const {

	if(m_playground.hasAliveActors())
		return true;

	// if only the actor model is used, the rank is basically
	// dead if all its actors are dead !
	if(useActorModelOnly())
		return false;

	// otherwise, we use the life indicator for the rank

	return m_alive;
}

bool ComputeCore::useActorModelOnly() const {

	return m_useActorModelOnly;
}

void ComputeCore::setActorModelOnly() {

	m_useActorModelOnly = true;
}

void ComputeCore::spawn(Actor * actor) {

	spawnActor( actor );
}

#ifdef CONFIG_ASSERT
void ComputeCore::testMessage(Message * message) {

	message->runAssertions(getSize(), m_routerIsEnabled,
			m_miniRanksAreEnabled);
}
#endif
