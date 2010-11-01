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
    along with this program (LICENSE).  
	see <http://www.gnu.org/licenses/>

*/


#ifndef _SplayTree
#define _SplayTree

#include<vector>
#include<stdlib.h>
#include<common_functions.h>
#include<SplayNode.h>
#include<iostream>
#include<MyAllocator.h>
#include<stack>
using namespace std;


/*
 * the splay tree is a binary tree. If a key is asked often, the key will be near the root, otherwise, it will be deeper.
 * this eases the processing because sequencing errors are kept far away from the root.
 *
 */
// see http://www.eli.sdsu.edu/courses/fall95/cs660/notes/splay/Splay.html
// see http://www.cs.nyu.edu/algvis/java/SplayTree.html
// see ftp://ftp.cs.cmu.edu/user/sleator/splaying/SplayTree.java

template<class KEY,class VALUE>
class SplayTree{
	SplayNode<KEY,VALUE>*m_root;
	VERTEX_TYPE m_size;
	bool m_inserted;
	void splay(KEY key);
	MyAllocator*m_allocator;
	bool m_frozen;
public:
	SplayTree();
	void constructor(MyAllocator*allocator);
	void constructor();
	~SplayTree();
	// freeze the splay tree.
	void freeze();
	void remove(KEY key);
	SplayNode<KEY,VALUE>*insert(KEY key);
	SplayNode<KEY,VALUE>*find(KEY key);
	void print();
	SplayNode<KEY,VALUE>*getRoot();
	VERTEX_TYPE size();
	bool inserted();
	void clear();
};

template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::clear(){
	m_root=NULL;
}

template<class KEY,class VALUE>
VERTEX_TYPE SplayTree<KEY,VALUE>::size(){
	return m_size;
}

template<class KEY,class VALUE>
bool SplayTree<KEY,VALUE>::inserted(){
	return m_inserted;
}

template<class KEY,class VALUE>
SplayTree<KEY,VALUE>::SplayTree(){
	constructor();
}

template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::constructor(){
	m_frozen=false;
	m_root=NULL;
	m_size=0;
	m_inserted=false;
	m_allocator=NULL;
}

template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::constructor(MyAllocator*allocator){
	m_allocator=allocator;
}

template<class KEY,class VALUE>
SplayTree<KEY,VALUE>::~SplayTree(){
	clear();
}

/*
 *  currently not implemented.
 */
template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::remove(KEY key){
}

template<class KEY,class VALUE>
SplayNode<KEY,VALUE>*SplayTree<KEY,VALUE>::getRoot(){
	return m_root;
}

/*
 * find x, if it is not there, call the last vertex visited during the search y, and splay y
 * then, add x as root, and put y as child of x (left or right)
 */
template<class KEY,class VALUE>
SplayNode<KEY,VALUE>*SplayTree<KEY,VALUE>::insert(KEY key){
	m_inserted=false;
	if(m_root==NULL){
		m_root=(SplayNode<KEY,VALUE>*)m_allocator->allocate(sizeof(SplayNode<KEY,VALUE>));
		m_root->init(key);
		m_inserted=true;
		m_size++;
		return m_root;
	}
	splay(key);
	if(m_root->getKey()==key)
		return m_root;
	SplayNode<KEY,VALUE>*n=(SplayNode<KEY,VALUE>*)m_allocator->allocate(sizeof(SplayNode<KEY,VALUE>));
	n->init(key);
	
	if(key<m_root->getKey()){
		n->m_left=m_root->m_left;
		n->m_right=m_root;
		m_root->m_left=NULL;
	}else{
		n->m_right=m_root->m_right;
		n->m_left=m_root;
		m_root->m_right=NULL;
	}
	m_root=n;
	m_size++;
	m_inserted=true;
	return m_root;
}

/*
 * binarySearch(x), if it is not there, return NULL
 * splay(x)
 * return the root
 */
template<class KEY,class VALUE>
SplayNode<KEY,VALUE>*SplayTree<KEY,VALUE>::find(KEY key){
	if(m_frozen){
		SplayNode<KEY,VALUE>*t=m_root;
		while(t!=NULL){
			if(t->m_key==key){
				return t;
			}else if(key<t->m_key){
				t=t->m_left;
			}else{
				t=t->m_right;
			}
		}
		return NULL;
	}

	if(m_root==NULL)
		return NULL;
	splay(key);
	if(m_root->getKey()!=key)
		return NULL;
	return m_root;
}

template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::print(){
	cout<<"digraph{"<<endl;
	stack<SplayNode<KEY,VALUE>*> vertices;
	stack<int> depths;
	if(m_root!=NULL){
		vertices.push(m_root);
		depths.push(0);
	}
	while(vertices.size()>0){
		SplayNode<KEY,VALUE>*u=vertices.top();
		vertices.pop();
		int d=depths.top();
		depths.pop();
		cout<<"\""<<u->getKey()<<"\""<<endl;

		if(u->getLeft()!=NULL){
			vertices.push(u->getLeft());
			cout<<"\""<<u->getKey()<<"\""<<" -> "<<"\""<<u->getLeft()->getKey()<<"\""<<endl;
			depths.push(d+1);
		}

		if(u->getRight()!=NULL){
			vertices.push(u->getRight());
			cout<<"\""<<u->getKey()<<"\""<<" -> "<<"\""<<u->getRight()->getKey()<<"\""<<endl;
			depths.push(d+1);
		}
	}
	cout<<"}"<<endl;
}

template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::freeze(){
	#ifdef SHOW_FREEZE
	cout<<"Freezing splay tree."<<endl;
	#endif
	m_frozen=true;
}

template<class KEY,class VALUE>
void SplayTree<KEY,VALUE>::splay(KEY key){
	SplayNode<KEY,VALUE> header;
	SplayNode<KEY,VALUE>*l;
	SplayNode<KEY,VALUE>*r;
	SplayNode<KEY,VALUE>*t;
	SplayNode<KEY,VALUE>*y;
	l=r=&header;
	t=m_root;
	header.m_left=header.m_right=NULL;
	while(1){
		if(key<t->m_key){
			if(t->m_left==NULL)
				break;
			if(key<t->m_left->m_key){
				y=t->m_left;
				t->m_left=y->m_right;
				y->m_right=t;
				t=y;
				if(t->m_left==NULL)
					break;
			}
			r->m_left=t;
			r=t;
			t=t->m_left;
		}else if(t->m_key<key){
			if(t->m_right==NULL)
				break;
			if(t->m_right->m_key<key){
				y=t->m_right;
				t->m_right=y->m_left;
				y->m_left=t;
				t=y;
				if(t->m_right==NULL)
					break;
			}
			l->m_right=t;
			l=t;
			t=t->m_right;
		}else{
			break;
		}
	}
	l->m_right=t->m_left;
	r->m_left=t->m_right;
	t->m_left=header.m_right;
	t->m_right=header.m_left;
	m_root=t;
}

#endif