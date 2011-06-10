# Ray assembler

Ray is a parallel de novo genome assembler that utilises the message-passing interface everywhere.
Ray is implemented using peer-to-peer communication with the message-passing interface.


# Website

http://denovoassembler.sf.net


# Code repository

	https://github.com/sebhtml/ray

# Mailing list

denovoassembler-users <AT> lists.sourceforge.net

# Installation




	tar xjf Ray-x.y.z.tar.bz2
	cd Ray-x.y.z
	make
	make install PREFIX=build9
	ls build9


## Change the compiler

make MPICXX=/software/openmpi-1.4.3/bin/mpic++

## Compilation options

make MAXKMERLENGTH=128 HAVE_LIBZ=n HAVE_LIBBZ2=n HAVE_CLOCK_GETTIME=n INTEL_COMPILER=n ASSERT=n FORCE_PACKING=y

see the Makefile for more.


# Run Ray

To run Ray on paired reads:

	mpirun -np 25 Ray -p lib1left.fasta lib1right.fasta -p lib2left.fasta lib2right.fasta -o prefix
	ls prefix.Contigs.fasta
	ls prefix.Scaffolds.fasta
	ls prefix.*

# Publications

http://denovoassembler.sf.net/publications.html


# Message-passing interface

- http://cw.squyres.com/
- http://www.parawiki.org/index.php/Message_Passing_Interface#Peer_to_Peer_Communication

# Funding

Doctoral Award to S.B., Canadian Institutes of Health Research (CIHR)

# Authors


## Coding, maintenance, implementing ideas:

	Sébastien Boisvert, Doctoral student, Université Laval


## Mentoring:

	Prof. François Laviolette, Université Laval
	Prof. Jacques Corbeil, Université Laval


## Design insights for parallel architecture:

	Élénie Godzaridis

	