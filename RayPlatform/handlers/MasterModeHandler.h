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

#ifndef _MasterModeHandler_h
#define _MasterModeHandler_h

#include <RayPlatform/core/types.h>
#include <RayPlatform/core/master_modes.h>

#ifdef CONFIG_MINI_RANKS

#define MasterModeHandlerReference MasterModeHandler*

#define __CreateMasterModeAdapter ____CreateMasterModeAdapterImplementation
#define __DeclareMasterModeAdapter ____CreateMasterModeAdapterDeclaration

/* this is a macro to create the header code for an adapter */
#define ____CreateMasterModeAdapterDeclaration(corePlugin,handle) \
class Adapter_ ## handle : public MasterModeHandler{ \
	corePlugin *m_object; \
public: \
	void setObject(corePlugin *object); \
	void call(); \
};

/* this is a macro to create the cpp code for an adapter */
#define ____CreateMasterModeAdapterImplementation(corePlugin,handle)\
void Adapter_ ## handle ::setObject( corePlugin *object){ \
	m_object=object; \
} \
 \
void Adapter_ ## handle ::call(){ \
	m_object->call_ ## handle(); \
}

/**
 * base class for handling master modes 
 *
 * \author Sébastien Boisvert
 *
 * with help from Élénie Godzaridis for the design
 *
 * \date 2012-08-08 replaced this with function pointers
 */
class MasterModeHandler{

public:

	virtual void call() = 0;

	virtual ~MasterModeHandler(){}
};

#else

/*
 * Without mini-ranks.
 */

#define __DeclareMasterModeAdapter(corePlugin, handle)

/* this is a macro to create the cpp code for an adapter */
#define __CreateMasterModeAdapter( corePlugin,handle )\
void __GetAdapter( corePlugin, handle ) () { \
	__GetPlugin( corePlugin ) -> __GetMethod( handle ) () ; \
} 

/**
 * base class for handling master modes 
 * \author Sébastien Boisvert
 * with help from Élénie Godzaridis for the design
 * \date 2012-08-08 replaced this with function pointers
 */
typedef void (*MasterModeHandler) () /* */ ;

#define MasterModeHandlerReference MasterModeHandler

#endif

#define __ConfigureMasterModeHandler(pluginName,handlerHandle) \
	handlerHandle= m_core->allocateMasterModeHandle(m_plugin); \
	m_core->setMasterModeObjectHandler(m_plugin,handlerHandle, \
		__GetAdapter(pluginName,handlerHandle)); \
	m_core->setMasterModeSymbol(m_plugin, handlerHandle, # handlerHandle); \
	__BindAdapter(pluginName, handlerHandle);



#endif
