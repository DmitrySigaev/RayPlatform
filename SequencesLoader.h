/*
 	Ray
    Copyright (C) 2010  Sébastien Boisvert

	http://DeNovoAssembler.SourceForge.Net/

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, version 3 of the License.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You have received a copy of the GNU General Public License
    along with this program (gpl-3.0.txt).  
	see <http://www.gnu.org/licenses/>

*/

#ifndef _SequencesLoader
#define _SequencesLoader
#include<Parameters.h>
#include<DistributionData.h>
#include<vector>
#include<Message.h>
#include<Read.h>
#include<BubbleData.h>
#include<time.h>
using namespace std;

class SequencesLoader{
public:
	/**
 *	load sequences from disk, and distribute them over the network.
 */
	void loadSequences(int rank,int size,vector<Read*>*m_distribution_reads,int*m_distribution_sequence_id,
	bool*m_LOADER_isLeftFile,vector<Message>*m_outbox,int*m_distribution_file_id,
	MyAllocator*m_distributionAllocator,bool*m_LOADER_isRightFile,int*m_LOADER_averageFragmentLength,
	DistributionData*m_disData,int*m_LOADER_numberOfSequencesInLeftFile,MyAllocator*m_outboxAllocator,
	int*m_distribution_currentSequenceId,int*m_LOADER_deviation,bool*m_loadSequenceStep,
	BubbleData*m_bubbleData,
	time_t*m_lastTime,
	Parameters*m_parameters
);

	void flushPairedStock(int threshold,vector<Message>*m_outbox,
	MyAllocator*m_outboxAllocator,DistributionData*m_disData,
int rank);
};
#endif