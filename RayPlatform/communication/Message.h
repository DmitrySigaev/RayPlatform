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

#ifndef _Message_H
#define _Message_H

#include <RayPlatform/core/types.h>

#include <stdint.h>

/**
 * In Ray, every message is a Message.
 * the inbox and the outbox are arrays of Message's
 * All the code in Ray utilise Message to communicate information.
 * MPI_Datatype is always MPI_UNSIGNED_LONG_LONG
 * m_count is >=0 and <= MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(uint64_t)
 *  (default is 4000/8 = 500 ).
 * \author Sébastien Boisvert
 */
class Message{
private:

	/** the message body, contains data
 * 	if NULL, m_count must be 0 */
	MessageUnit*m_buffer;

	/** the number of uint64_t that the m_buffer contains 
 * 	can be 0 regardless of m_buffer value
 * 	*/
	int m_count;

	/** the Message-passing interface rank destination 
 * 	Must be >=0 and <= MPI_Comm_size()-1 */
	Rank m_destination;

	/**
 * 	Ray message-passing interface message tags are named RAY_MPI_TAG_<something>
 * 	see mpi_tag_macros.h 
 */
	MessageTag m_tag;

	/** the message-passing interface rank source 
 * 	Must be >=0 and <= MPI_Comm_size()-1 */
	Rank m_source;

	int m_sourceActor;
	int m_destinationActor;

	void initialize();
public:
	Message();
	~Message();
	Message(MessageUnit*b,int c,Rank dest,MessageTag tag,Rank source);
	MessageUnit*getBuffer();
	int getCount() const;
/**
 * Returns the destination MPI rank
 */
	Rank getDestination() const;

/**
 * Returns the message tag (RAY_MPI_TAG_something)
 */
	MessageTag getTag() const;
/**
 * Gets the source MPI rank
 */
	Rank getSource() const;

	void print();

	void setBuffer(MessageUnit*buffer);

	void setTag(MessageTag tag);

	void setSource(Rank source);

	void setDestination(Rank destination);

	void setCount(int count);

	// actor model endpoints


	bool isActorModelMessage() const;

	int getDestinationActor() const;
	int getSourceActor() const;
	void setSourceActor(int sourceActor);
	void setDestinationActor(int destinationActor);
	void saveActorMetaData();
	void loadActorMetaData();
	int getMetaDataSize() const;
	void printActorMetaData();

};

#endif
