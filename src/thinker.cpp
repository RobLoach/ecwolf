/*
** thinker.cpp
**
**---------------------------------------------------------------------------
** Copyright 2011 Braden Obrzut
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
** Thinkers are given priorities, one of which (TRAVEL) allows us to transfer
** actors between levels without it being collected.  This is similar to ZDoom's
** system, which in turn, is supposedly based off build.
**
*/

#include "thinker.h"
#include "thingdef/thingdef.h"
#include "wl_def.h"

ThinkerList *thinkerList;

void DeinitThinkerList()
{
	delete thinkerList;
	thinkerList = NULL;
}

void InitThinkerList()
{
	thinkerList = new ThinkerList();
	atexit(DeinitThinkerList);
}

ThinkerList::ThinkerList()
{
}

ThinkerList::~ThinkerList()
{
	DestroyAll(static_cast<Priority>(0));
}

void ThinkerList::DestroyAll(Priority start)
{
	for(unsigned int i = start;i < NUM_TYPES;++i)
	{
		Iterator iter = thinkers[i].Head();
		while(iter)
		{
			if(!(iter->Item()->ObjectFlags & OF_EuthanizeMe))
				iter->Item()->Destroy();
			iter = iter->Next();
		}
	}
	GC::FullGC();
}

void ThinkerList::MarkRoots()
{
	for(unsigned int i = 0;i < NUM_TYPES;++i)
	{
		Iterator iter = thinkers[i].Head();
		while(iter)
		{
			if(!(iter->Item()->ObjectFlags & OF_EuthanizeMe))
			{
				GC::Mark(iter->Item());
				break;
			}
			iter = iter->Next();
		}
	}
}

void ThinkerList::Tick()
{
	for(unsigned int i = FIRST_TICKABLE;i < NUM_TYPES;++i)
	{
		Iterator iter = thinkers[i].Head();
		while(iter)
		{
			if(!(iter->Item()->ObjectFlags & OF_EuthanizeMe))
				iter->Item()->Tick();
			iter = iter->Next();
		}
	}
}

void ThinkerList::Register(Thinker *thinker, Priority type)
{
	thinker->thinkerRef = thinkers[type].Push(thinker);
	thinker->thinkerPriority = type;
}

void ThinkerList::Deregister(Thinker *thinker)
{
	thinkers[thinker->thinkerPriority].Remove(thinker->thinkerRef);
}

////////////////////////////////////////////////////////////////////////////////

IMPLEMENT_ABSTRACT_CLASS(Thinker)

Thinker::Thinker(ThinkerList::Priority priority)
{
	thinkerList->Register(this, priority);
}

Thinker::~Thinker()
{
	thinkerList->Deregister(this);
}

size_t Thinker::PropagateMark()
{
	ThinkerList::Iterator iter = thinkerRef->Next();
	while(iter)
	{
		if(!(iter->Item()->ObjectFlags & OF_EuthanizeMe))
		{
			GC::Mark(iter->Item());
			break;
		}
		iter = iter->Next();
	}

	iter = thinkerRef->Prev();
	while(iter)
	{
		if(!(iter->Item()->ObjectFlags & OF_EuthanizeMe))
		{
			GC::Mark(iter->Item());
			break;
		}
		iter = iter->Prev();
	}
	return Super::PropagateMark();
}

void Thinker::SetPriority(ThinkerList::Priority priority)
{
	thinkerList->Deregister(this);
	thinkerList->Register(this, priority);
}
