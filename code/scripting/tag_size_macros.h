/*
 	Ray
    Copyright (C) 2012  Sébastien Boisvert

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


// this file contains the number of elements per tag

MACRO_LIST_ITEM( RAY_MPI_TAG_GET_CONTIG_CHUNK,		MAXIMUM_MESSAGE_SIZE_IN_BYTES/sizeof(uint64_t) )
MACRO_LIST_ITEM( RAY_MPI_TAG_REQUEST_VERTEX_READS, 		max(5,KMER_U64_ARRAY_SIZE+1) )
MACRO_LIST_ITEM( RAY_MPI_TAG_GET_READ_MATE, 		4 )
MACRO_LIST_ITEM( RAY_MPI_TAG_REQUEST_VERTEX_COVERAGE,	KMER_U64_ARRAY_SIZE )
MACRO_LIST_ITEM( RAY_MPI_TAG_ATTACH_SEQUENCE,		KMER_U64_ARRAY_SIZE+4 )
MACRO_LIST_ITEM( RAY_MPI_TAG_GET_VERTEX_EDGES_COMPACT,	max(2,KMER_U64_ARRAY_SIZE))
MACRO_LIST_ITEM( RAY_MPI_TAG_HAS_PAIRED_READ,		1 )
MACRO_LIST_ITEM( RAY_MPI_TAG_GET_READ_MARKERS,		3+2*KMER_U64_ARRAY_SIZE )
MACRO_LIST_ITEM( RAY_MPI_TAG_GET_PATH_LENGTH,		1 )
MACRO_LIST_ITEM( RAY_MPI_TAG_GET_COVERAGE_AND_DIRECTION,	max(KMER_U64_ARRAY_SIZE,5) )
MACRO_LIST_ITEM( RAY_MPI_TAG_SCAFFOLDING_LINKS, 		7 )
MACRO_LIST_ITEM( RAY_MPI_TAG_CONTIG_INFO,			2)
MACRO_LIST_ITEM( RAY_MPI_TAG_ASK_READ_LENGTH, 		3 )
MACRO_LIST_ITEM( RAY_MPI_TAG_ASK_VERTEX_PATHS_SIZE, KMER_U64_ARRAY_SIZE )
MACRO_LIST_ITEM( RAY_MPI_TAG_ASK_VERTEX_PATH, (KMER_U64_ARRAY_SIZE + 2) )
MACRO_LIST_ITEM( RAY_MPI_TAG_GET_PATH_VERTEX, max(2,KMER_U64_ARRAY_SIZE) )

