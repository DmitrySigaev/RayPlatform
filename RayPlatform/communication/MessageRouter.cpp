/*
 	RayPlatform: a message-passing development framework
    Copyright (C) 2010, 2011, 2012 Sébastien Boisvert

	http://github.com/sebhtml/RayPlatform

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

//#define CONFIG_ROUTING_VERBOSITY

/**
 * \brief Message router implementation
 *
 * \author Sébastien Boisvert 2011-11-04
 * \reviewedBy Elénie Godzaridis 2011-11-05
*/

#include "MessageRouter.h"

#include <RayPlatform/core/OperatingSystem.h>
#include <RayPlatform/core/ComputeCore.h>

#include <time.h> /* for time */
#include <string.h> /* for memcpy */
#include <assert.h>
using namespace std;

/*
 * According to the MPI standard, MPI_TAG_UB is >= 32767 (2^15-1)
 * Therefore, the tag must be >=0 and <= 32767
 * In most applications, there will be not that much tag
 * values.
 * 2^14 = 16384
 * So that MPI tag in applications using RayPlatform can be
 * from 0 to 16383.
 * Internally, RayPlatform adds 16384 to the application tag.
 * A routing tag is >= 16384 and the corresponding real tag is
 * tag - 16384.
 */
#define __ROUTING_TAG_BASE 16384

#define __ROUTING_SOURCE 0
#define __ROUTING_DESTINATION 1
#define __ROUTING_OFFSET(count) (count-1)

/*
#define ASSERT
*/

/**
 * route outcoming messages
 */
void MessageRouter::routeOutcomingMessages(){
	int numberOfMessages=m_outbox->size();

	if(numberOfMessages==0)
		return;

	for(int i=0;i<numberOfMessages;i++){
		Message*aMessage=m_outbox->at(i);

		MessageTag communicationTag=aMessage->getTag();

		#ifdef CONFIG_ROUTING_VERBOSITY
		uint8_t printableTag=communicationTag;
		cout<<"[routeOutcomingMessages] tag= "<<MESSAGE_TAGS[printableTag]<<" value="<<communicationTag<<endl;
		#endif

		// - first, the message may have been already routed when it was received (also
		// in a routed version). In this case, nothing must be done.
		if(isRoutingTag(communicationTag)){
			#ifdef CONFIG_ROUTING_VERBOSITY
			cout<<"["<<__func__<<"] Message has already a routing tag."<<endl;
			aMessage->displayMetaData();
			#endif
			continue;
		}

		// at this point, the message has no routing information yet.
		Rank trueSource=aMessage->getSource();
		Rank trueDestination=aMessage->getDestination();

		// if it is reachable, no further routing is required
		if(m_graph.isConnected(trueSource,trueDestination)){

			#ifdef CONFIG_ROUTING_VERBOSITY
			cout<<"["<<__func__<<"] Rank "<<trueSource<<" can reach "<<trueDestination<<" without routing";
			cout << " payload size: " << aMessage->getCount() << endl;
			#endif

			continue;
		}

		// re-route the message by re-writing the tag
		MessageTag routingTag=getRoutingTag(communicationTag);
		aMessage->setTag(routingTag);


		// routing information is stored in 64 bits
		// avoid touching this because getCount return the number of MessageUnit whereas
		// getNumberOfBytes returns bytes. If there are 4 bytes, getCount returns 0 and
		// setCount will obviously erase the 4 bytes in the count.
		//
		// MessageUnit is there fore backward compatibility with old Ray code.
		//
		// New code that uses the actor model should use  setNumberOfBytes,
		// setBuffer, and setTag.
		// source and destination are set by the send command (plugin method)
		/* int newCount=aMessage->getCount();
		aMessage->setCount(newCount);
		*/

		#ifdef CONFIG_ASSERT
		MessageUnit*buffer=aMessage->getBuffer();
		assert(buffer!=NULL);
		#endif

		aMessage->setRoutingSource(trueSource);
		aMessage->setRoutingDestination(trueDestination);

		/*
		setSourceInBuffer(buffer,newCount,trueSource);
		setDestinationInBuffer(buffer,newCount,trueDestination);
		*/

		#ifdef CONFIG_ASSERT
		assert(aMessage->getRoutingSource() ==trueSource);
		assert(aMessage->getRoutingDestination() ==trueDestination);
		assert(trueSource >= 0);
		assert(trueDestination >= 0);
		assert(trueSource < m_size);
		assert(trueDestination < m_size);
		#endif

		Rank nextRank=m_graph.getNextRankInRoute(trueSource,trueDestination,m_rank);
		aMessage->setDestination(nextRank);

#ifdef CONFIG_ASSERT
		assert(nextRank >= 0);
		assert(nextRank < m_size);
#endif

		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"["<<__func__<<"] relayed message, trueSource="<<trueSource;
		cout << "count= " << aMessage->getCount();
		cout <<" trueDestination="<<trueDestination<<" to intermediateSource "<<nextRank;
		aMessage->displayMetaData();
		cout <<endl;
		#endif
	}

	// check that all messages are routable
	#ifdef CONFIG_ASSERT
	for(int i=0;i<numberOfMessages;i++){
		Message*aMessage=m_outbox->at(i);
		if(!m_graph.isConnected(aMessage->getSource(),aMessage->getDestination()))
			cout<<aMessage->getSource()<<" and "<<aMessage->getDestination()<<" are not connected !"<<endl;
		assert(m_graph.isConnected(aMessage->getSource(),aMessage->getDestination()));
	}
	#endif
}

/**
 * route incoming messages
 * \returns true if rerouted something.
 */
bool MessageRouter::routeIncomingMessages(){

	int numberOfMessages=m_inbox->size();

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"["<<__func__<<"] inbox.size= "<<numberOfMessages<<endl;
	#endif

	// we have no message
	if(numberOfMessages==0)
		return false;

	// otherwise, we have exactly one precious message.

	Message*aMessage=m_inbox->at(0);

	MessageTag tag=aMessage->getTag();


	// if the message has no routing tag, then we can safely receive it as is
	if(!isRoutingTag(tag)){
		// nothing to do
		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"["<<__func__<<"] message has no routing tag, nothing to do"<<endl;
		#endif

		return false;
	}

	//cout << "DEBUG message was sent by " << aMessage->getSource();

	//aMessage->displayMetaData();

	// at this point, we know there is a routing activity to perform.
	//MessageUnit*buffer=aMessage->getBuffer();

	#ifdef CONFIG_ROUTING_VERBOSITY
	int count=aMessage->getCount();
	uint8_t printableTag=tag;
	cout<<"[routeIncomingMessages] tag= "<<MESSAGE_TAGS[printableTag]<<" value="<<tag;
	cout << " count= " << count <<endl;
	#endif

	/*
	Rank trueSource=getSourceFromBuffer(buffer,count);
	Rank trueDestination=getDestinationFromBuffer(buffer,count);
	*/

	Rank trueSource = aMessage->getRoutingSource();
	Rank trueDestination = aMessage->getRoutingDestination();

#ifdef CONFIG_ASSERT
	assert(trueSource >= 0);
	assert(trueDestination >= 0);
#endif

	//cout << "DEBUG after loading metadata: trueDestination= ";
	//cout << trueDestination << " trueSource " << trueSource << endl;

	// this is the final destination
	// we have received the message
	// we need to restore the original information now.
	if(trueDestination==m_rank){
		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"["<<__func__<<"] message has reached destination, must strip routing information"<<endl;
		#endif

		// we must update the original source and original tag
		aMessage->setSource(trueSource);

		// the original destination is already OK
		#ifdef CONFIG_ASSERT
		assert(aMessage->getDestination()==m_rank);
		#endif

		MessageTag trueTag=getMessageTagFromRoutingTag(tag);
		aMessage->setTag(trueTag);

		#ifdef CONFIG_ROUTING_VERBOSITY
		cout<<"[routeIncomingMessages] real tag= "<<trueTag<<endl;
		#endif

		// remove the routing stuff
		//int newCount=aMessage->getCount();

		#ifdef CONFIG_ASSERT
		//assert(newCount>=0);
		#endif

		//aMessage->setCount(newCount);

		// set the buffer to NULL if there is no data
		//if(newCount==0)
			//aMessage->setBuffer(NULL);

		return false;
	}

	#ifdef CONFIG_ASSERT
	assert(m_rank!=trueDestination);
	#endif

#ifdef CONFIG_ASSERT
	if(!((trueDestination >= 0 && trueDestination < m_size))) {
		cout << "Error trueDestination " << trueDestination << endl;
	}

	assert(trueDestination >= 0 && trueDestination < m_size);
	assert(trueSource >= 0 && trueSource < m_size);
	assert(m_rank >= 0 && m_rank < m_size);
#endif

	// at this point, we know that we need to forward
	// the message to another peer
	Rank nextRank=m_graph.getNextRankInRoute(trueSource,trueDestination,m_rank);

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"["<<__func__<<"] message has been sent to the next one, trueSource="<<trueSource<<" trueDestination= "<<trueDestination;
	cout<<" Previous= "<<aMessage->getSource()<<" Current= "<<m_rank<<" Next= "<<nextRank<<endl;
	#endif

#ifdef CONFIG_ASSERT
	assert(trueSource >= 0);
	assert(trueDestination >= 0);
	assert(trueSource < m_size);
	assert(trueDestination < m_size);
	assert(nextRank >= 0);
	assert(nextRank < m_size);
#endif

	// we forward the message
	relayMessage(aMessage,nextRank);

	// propagate the information about routing
	return true;
}

/**
 * forward a message to follow a route
 */
void MessageRouter::relayMessage(Message*message,Rank destination){

	int count=message->getNumberOfBytes();

	// routed messages always have a payload
	// but the metadata is saved later.
	#ifdef CONFIG_ASSERT
	assert(count>=0);
	#endif

	MessageUnit*outgoingMessage=(MessageUnit*)m_outboxAllocator->allocate(MAXIMUM_MESSAGE_SIZE_IN_BYTES);

	// copy the data into the new buffer
	memcpy(outgoingMessage, message->getBuffer(), count*sizeof(char));
	message->setBuffer(outgoingMessage);

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"[relayMessage] TrueSource="<< message->getRoutingSource();
	cout<<" TrueDestination="<< message->getRoutingDestination();
	cout<<" RelaySource="<<m_rank<<" RelayDestination="<<destination<<endl;
	#endif

	// re-route the message
	message->setSource(m_rank);
	message->setDestination(destination);

	#ifdef CONFIG_ASSERT
	assert(m_graph.isConnected(m_rank,destination));
	#endif

	// since we used the old message to create this new one,
	// all the metadata should still be there.
	m_outbox->push_back(message);
}


/**
 * a tag is a routing tag is its routing bit is set to 1
 */
bool MessageRouter::isEnabled(){
	return m_enabled;
}

MessageRouter::MessageRouter(){
	m_enabled=false;
}

void MessageRouter::enable(StaticVector*inbox,StaticVector*outbox,RingAllocator*outboxAllocator,Rank rank,
	string prefix,int numberOfRanks,string type,int degree){

	m_graph.buildGraph(numberOfRanks,type,rank==MASTER_RANK,degree);

	m_size=numberOfRanks;

	cout<<endl;

	cout<<"[MessageRouter] Enabled message routing"<<endl;

	m_inbox=inbox;
	m_outbox=outbox;
	m_outboxAllocator=outboxAllocator;
	m_rank=rank;
	m_enabled=true;
	m_deletionTime=0;

	m_prefix=prefix;
}

void MessageRouter::writeFiles(){
	if(m_rank==MASTER_RANK)
		m_graph.writeFiles(m_prefix);
}

bool MessageRouter::hasCompletedRelayEvents(){

	int duration=16;

	if(m_deletionTime==0){
		m_deletionTime=time(NULL);
		cout<<"[MessageRouter] Rank "<<m_rank<<" will die in "<<duration<<" seconds,";
		cout<<" will not route anything after that point."<<endl;
	}

	time_t now=time(NULL);

	return now >= (m_deletionTime+duration);
}

/*
 * just add a magic number
 */
MessageTag MessageRouter::getRoutingTag(MessageTag tag){
	#ifdef CONFIG_ASSERT
	assert(tag>=0);
	assert(tag<32768);
	#endif

	return tag+__ROUTING_TAG_BASE;
}

ConnectionGraph*MessageRouter::getGraph(){
	return &m_graph;
}

bool MessageRouter::isRoutingTag(MessageTag tag){
	return tag>=__ROUTING_TAG_BASE;
}

MessageTag MessageRouter::getMessageTagFromRoutingTag(MessageTag tag){

	#ifdef CONFIG_ASSERT
	assert(isRoutingTag(tag));
	assert(tag>=__ROUTING_TAG_BASE);
	#endif

	tag-=__ROUTING_TAG_BASE;

	#ifdef CONFIG_ASSERT
	assert(tag>=0);
	#endif

	return tag;
}

Rank MessageRouter::getSourceFromBuffer(MessageUnit*buffer,int count){
	#ifdef CONFIG_ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	Rank rank=routingInformation[__ROUTING_SOURCE];

	#ifdef CONFIG_ASSERT
	assert(rank>=0);
	assert(rank<m_size);
	#endif

	return rank;
}

Rank MessageRouter::getDestinationFromBuffer(MessageUnit*buffer,int count){

	#ifdef CONFIG_ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	#endif

	int offset = __ROUTING_OFFSET(count);

	uint32_t*routingInformation=(uint32_t*)(buffer+ offset);

	Rank rank=routingInformation[__ROUTING_DESTINATION];

	/*
	int * bufferAsInteger = (int*)buffer;
	cout << "DEBUG getDestinationFromBuffer count " << count;
	cout << " offset " << offset ;
	cout << " destination " << rank << endl;

	for(int i = 0 ; i < 4 ; ++i) {
		cout << "DEBUG buffer [ " << i << "] -> ";
		cout << bufferAsInteger[i] << endl;
	}
	*/

	#ifdef CONFIG_ASSERT
	assert(rank>=0);
	assert(rank<m_size);
	#endif

	return rank;
}

void MessageRouter::setSourceInBuffer(MessageUnit*buffer,int count,Rank source){

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"[setSourceInBuffer] buffer="<<buffer<<" source="<<source<<endl;
	#endif

	#ifdef CONFIG_ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	assert(source>=0);
	assert(source<m_size);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	routingInformation[__ROUTING_SOURCE]=source;
}

void MessageRouter::setDestinationInBuffer(MessageUnit*buffer,int count,Rank destination){

	#ifdef CONFIG_ROUTING_VERBOSITY
	cout<<"[setDestinationInBuffer] buffer="<<buffer<<" destination="<<destination<<endl;
	#endif

	#ifdef CONFIG_ASSERT
	assert(count>=1);
	assert(buffer!=NULL);
	assert(destination>=0);
	assert(destination<m_size);
	#endif

	uint32_t*routingInformation=(uint32_t*)(buffer+__ROUTING_OFFSET(count));

	routingInformation[__ROUTING_DESTINATION]=destination;
}


