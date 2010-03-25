# Ray
    #Copyright (C) 2010  Sébastien Boisvert

	#http://DeNovoAssembler.SourceForge.Net/

    #This program is free software: you can redistribute it and/or modify
    #it under the terms of the GNU General Public License as published by
    #the Free Software Foundation, version 3 of the License.

    #This program is distributed in the hope that it will be useful,
    #but WITHOUT ANY WARRANTY; without even the implied warranty of
    #MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    #GNU General Public License for more details.

    #You have received a copy of the GNU General Public License
    #along with this program (gpl-3.0.txt).  
	#see <http://www.gnu.org/licenses/>


CXXFLAGS=  -Wall  -I.  -O3  -DSHOW_MINIGRAPH -DSHOW_PROGRESS -DDEBUG -g  # debug flags.

# the default is to use mpic++ provided in your $PATH
MPICC=mpic++
MPIRUN=mpirun

TARGETS=Ray

all: $(TARGETS)


OBJECTS= Machine.o common_functions.o Loader.o Read.o MyAllocator.o SffLoader.o Parameters.o Vertex.o ReadAnnotation.o CoverageDistribution.o Message.o  Direction.o  PairedRead.o ColorSpaceDecoder.o ColorSpaceLoader.o VertexLinkedList.o BubbleTool.o

%.o: %.cpp
	$(MPICC) $(CXXFLAGS) -c $< -o $@

SimulatePairedReads: simulate_paired_main.o $(OBJECTS)
	$(CC) $(CXXFLAGS) $^ -o $@

SimulateErrors: simulateErrors_main.o $(OBJECTS)
	$(CC) $(CXXFLAGS) $^ -o $@

SimulateFragments: simulate_fragments_main.o $(OBJECTS)
	$(CC) $(CXXFLAGS) $^ -o $@


Ray: ray_main.o $(OBJECTS)
	$(MPICC) $(CXXFLAGS) $^ -o $@

test: test_main.o $(OBJECTS)
	$(MPICC) $(CXXFLAGS) $^ -o $@
	$(MPIRUN) ./test
clean:
	rm -f $(OBJECTS) $(TARGETS)

