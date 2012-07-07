/*
** thinker.h
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
** A thinker is an object which is to be informed of tick events.
**
*/

#ifndef __THINKER_H__
#define __THINKER_H__

#include "wl_def.h"
#include "name.h"
#include "linkedlist.h"
#include "actor.h"
#include "dobject.h"

class Thinker;
extern class ThinkerList
{
	public:
		enum Priority
		{
			TRAVEL,	// Doesn't think, preserved for later use

			VICTORY,// Think even if the victory flag is set
			WORLD,	// High priority world manipulations
			NORMAL,	// General purpose thinker

			NUM_TYPES,
			FIRST_TICKABLE = VICTORY
		};

		ThinkerList();
		~ThinkerList();

		typedef LinkedList<Thinker *>::Node *Iterator;

		Iterator GetHead(Priority list) { return thinkers[list].Head(); }
		void	DestroyAll(Priority start=FIRST_TICKABLE);
		void	Serialize(FArchive &arc);
		void	Tick();

		void	MarkRoots();
	protected:
		friend class Thinker;
		void	Register(Thinker *thinker, Priority type=NORMAL);
		void	Deregister(Thinker *thinker);

	private:
		// nxetThinker allows us to skip over thinkers that we were about to
		// think, but end up being destroyed.
		Iterator				nextThinker;
		LinkedList<Thinker *>	thinkers[NUM_TYPES];
} *thinkerList;

class Thinker : public DObject
{
	DECLARE_CLASS(Thinker, DObject)

	public:
		Thinker(ThinkerList::Priority priority=ThinkerList::NORMAL);

		void			Destroy();
		template<class T>
		bool			IsThinkerType() { return IsA(T::__StaticClass); }
		void			SetPriority(ThinkerList::Priority priority);
		virtual void	Tick()=0;
		size_t			PropagateMark();

	private:
		friend class ThinkerList;

		ThinkerList::Priority		thinkerPriority;
		LinkedList<Thinker *>::Node	*thinkerRef;
};

#endif
