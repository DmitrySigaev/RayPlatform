/*
 	Ray
    Copyright (C) 2011  Sébastien Boisvert

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

 	Funding:

Sébastien Boisvert has a scholarship from the Canadian Institutes of Health Research (Master's award: 200910MDR-215249-172830 and Doctoral award: 200902CGM-204212-172830).

*/

#ifndef _ExtensionElement
#define _ExtensionElement

#include <PairedRead.h>
#include <MyAllocator.h>

class ExtensionElement{
	int m_position;
	char m_strand;
	bool m_hasPairedRead;
	char*m_readSequence;
	PairedRead m_pairedRead;
public:
	void setStartingPosition(int a);
	void setStrand(char a);
	void setPairedRead(PairedRead a);
	bool hasPairedRead();
	void setSequence(const char*a,MyAllocator*b);
	int getPosition();
	char getStrand();
	PairedRead getPairedRead();
	char*getSequence();
};

#endif