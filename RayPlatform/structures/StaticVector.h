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

#ifndef _StaticVector
#define _StaticVector

#include <RayPlatform/communication/Message.h>
#include <RayPlatform/communication/mpi_tags.h>

/**
 * A static vector of Message.
 * This is used for the inbox and the outbox in Ray ranks or mini-ranks
 *
 * clear is basically O(1), it just sets m_size=0
 * \author Sébastien Boisvert
 */
class StaticVector{
	Message*m_messages;
	int m_size;
	int m_maxSize;
	char m_type[100];
public:
	Message*operator[](int i);
	Message*at(int i);

	// Messages are passed by reference or pointer.
	void push_back(Message*a);
	int size();
	void clear();
	void constructor(int size,const char*type,bool show);

	bool hasMessage(MessageTag tag);
};

#endif
