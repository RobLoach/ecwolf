/*
** g_conversation.h
**
**---------------------------------------------------------------------------
** Copyright 2014 Braden Obrzut
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
** This ended up being similar to the USDF parser in ZDoom in many ways.
**
*/

#include <climits>

#include "g_conversation.h"
#include "m_classes.h"
#include "m_random.h"
#include "scanner.h"
#include "tarray.h"
#include "w_wad.h"

static FRandom pr_conversation("Conversation");

class ConversationModule
{
public:
	struct ItemCheck;
	struct Choice;
	struct Page;
	struct Conversation;

	enum ConvNamespace
	{
		NS_Strife,
		NS_Noah
	};

	const Conversation *Find(unsigned int id) const;
	void Load(int lump);

	TArray<FString> Include;
	TMap<unsigned int, Conversation> Conversations;
	ConvNamespace Namespace;
	int Lump;

private:
	void ParseConversation(Scanner &sc);
	template<typename T>
	void ParseBlock(Scanner &sc, T &obj, bool (ConversationModule::*handler)(Scanner &, FName, bool, T &));

	bool ParseConvBlock(Scanner &, FName, bool, Conversation &);
	bool ParsePageBlock(Scanner &, FName, bool, Page &);
	bool ParseChoiceBlock(Scanner &, FName, bool, Choice &);
	bool ParseItemCheckBlock(Scanner &, FName, bool, ItemCheck &);
};

struct ConversationModule::ItemCheck
{
	unsigned int Item;
	unsigned int Amount;
};

struct ConversationModule::Choice
{
	TArray<ItemCheck> Cost;
	FString Text;
	FString YesMessage, NoMessage;
	FString Log;
	union
	{
		unsigned int NextPageIndex;
		Page *NextPage;
	};
	unsigned int GiveItem;
	unsigned int Special;
	unsigned int Arg[5];
	bool CloseDialog;
	bool DisplayCost;
};

struct ConversationModule::Page
{
	TArray<Choice> Choices;
	TArray<ItemCheck> IfItem;
	FString Name;
	FString Panel;
	FString Voice;
	FString Dialog;
	FString Hint;
	union
	{
		unsigned int LinkIndex; // Valid while parsing
		Page *Link;
	};
	unsigned int Drop;
};

struct ConversationModule::Conversation
{
	TArray<Page> Pages;
	unsigned int Actor;
	bool RandomStart;
	bool Preserve;

	const Page *Start() const
	{
		if(RandomStart)
			return &Pages[pr_conversation(Pages.Size())];
		return &Pages[0];
	}
};

// ----------------------------------------------------------------------------

const ConversationModule::Conversation *ConversationModule::Find(unsigned int id) const
{
	return Conversations.CheckKey(id);
}

void ConversationModule::Load(int lump)
{
	Lump = lump;

	FMemLump lmp = Wads.ReadLump(lump);
	Scanner sc((const char*)lmp.GetMem(), lmp.GetSize());
	sc.SetScriptIdentifier(Wads.GetLumpFullPath(lump));

	ParseConversation(sc);
}

template<typename T>
void ConversationModule::ParseBlock(Scanner &sc, T &obj, bool (ConversationModule::*handler)(Scanner &, FName, bool, T &))
{
	while(!sc.CheckToken('}'))
	{
		sc.MustGetToken(TK_Identifier);
		FName key(sc->str);
		if(sc.CheckToken('='))
		{
			if(!(this->*handler)(sc, key, true, obj))
				sc.GetNextToken();
			sc.MustGetToken(';');
		}
		else if(sc.CheckToken('{'))
		{
			if(!(this->*handler)(sc, key, false, obj))
			{
				unsigned int level = 1;
				do
				{
					if(sc.CheckToken('{'))
						++level;
					else if(sc.CheckToken('}'))
						--level;
					else
						sc.GetNextToken();
				}
				while(level);
			}
		}
		else
			sc.ScriptMessage(Scanner::ERROR, "Invalid syntax.\n");
	}
}

void ConversationModule::ParseConversation(Scanner &sc)
{
	FString key;

	while(sc.TokensLeft())
	{
		sc.MustGetToken(TK_Identifier);
		FName key(sc->str);
		
		if(sc.CheckToken('='))
		{
			switch(key)
			{
				default:
					sc.GetNextToken();
					break;
				case NAME_Namespace:
					sc.MustGetToken(TK_StringConst);
					if(sc->str.CompareNoCase("Strife") == 0)
					{
						Namespace = NS_Strife;
						sc.ScriptMessage(Scanner::WARNING, "Strife namespace not implemented.");
					}
					else if(sc->str.CompareNoCase("Noah") == 0)
						Namespace = NS_Noah;
					else
						sc.ScriptMessage(Scanner::ERROR, "Unsupported namespace '%s'.", sc->str.GetChars());
					break;
				case NAME_Include:
					sc.MustGetToken(TK_StringConst);
					Include.Push(sc->str);
					break;
			}
			sc.MustGetToken(';');
		}
		else if(sc.CheckToken('{'))
		{
			switch(key)
			{
				default: break;
				case NAME_Conversation:
				{
					Conversation conv;
					conv.Actor = UINT_MAX;
					conv.RandomStart = false;
					conv.Preserve = false;

					ParseBlock(sc, conv, &ConversationModule::ParseConvBlock);
					if(conv.Actor != UINT_MAX)
					{
						Conversation &conv2 = Conversations.Insert(conv.Actor, conv);

						// Resolve page links
						for(unsigned int i = conv2.Pages.Size();i-- > 0;)
						{
							conv2.Pages[i].Link = &conv2.Pages[conv2.Pages[i].LinkIndex - 1];
							for(unsigned int j = conv2.Pages[i].Choices.Size();j-- > 0;)
								conv2.Pages[i].Choices[j].NextPage = &conv2.Pages[conv2.Pages[i].Choices[j].NextPageIndex - 1];
						}
					}
					break;
				}
			}
		}
		else
			sc.ScriptMessage(Scanner::ERROR, "Invalid syntax.\n");
	}
}

bool ConversationModule::ParseConvBlock(Scanner &sc, FName key, bool isValue, Conversation &obj)
{
	if(isValue) switch(key)
	{
		default: return false;
		case NAME_Actor:
			sc.MustGetToken(TK_IntConst);
			obj.Actor = sc->number;
			break;
		case NAME_RandomStart:
			if(Namespace == NS_Noah)
			{
				sc.MustGetToken(TK_BoolConst);
				obj.RandomStart = sc->boolean;
			}
			else return false;
			break;
		case NAME_Preserve:
			if(Namespace == NS_Noah)
			{
				sc.MustGetToken(TK_BoolConst);
				obj.Preserve = sc->boolean;
			}
			else return false;
			break;
	}
	else switch(key)
	{
		default: return false;
		case NAME_Page:
			ParseBlock(sc, obj.Pages[obj.Pages.Push(Page())], &ConversationModule::ParsePageBlock);
			break;
	}
	return true;
}

bool ConversationModule::ParsePageBlock(Scanner &sc, FName key, bool isValue, Page &obj)
{
	if(isValue) switch(key)
	{
		default: return false;
		case NAME_Name:
			sc.MustGetToken(TK_StringConst);
			obj.Name = sc->str;
			break;
		case NAME_Panel:
			sc.MustGetToken(TK_StringConst);
			obj.Panel = sc->str;
			break;
		case NAME_Voice:
			sc.MustGetToken(TK_StringConst);
			obj.Voice = sc->str;
			break;
		case NAME_Dialog:
			sc.MustGetToken(TK_StringConst);
			obj.Dialog = sc->str;
			break;
		case NAME_Drop:
			sc.MustGetToken(TK_IntConst);
			obj.Drop = sc->number;
			break;
		case NAME_Link:
			sc.MustGetToken(TK_IntConst);
			obj.LinkIndex = sc->number;
			break;
		case NAME_Hint:
			if(Namespace != NS_Noah)
				return false;
			sc.MustGetToken(TK_StringConst);
			obj.Hint = sc->str;
			break;
	}
	else switch(key)
	{
		default: return false;
		case NAME_Choice:
		{
			Choice &choice = obj.Choices[obj.Choices.Push(Choice())];
			choice.DisplayCost = false;
			choice.CloseDialog = true;
			ParseBlock(sc, choice, &ConversationModule::ParseChoiceBlock);
			break;
		}
		case NAME_IfItem:
			ParseBlock(sc, obj.IfItem[obj.IfItem.Push(ItemCheck())], &ConversationModule::ParseItemCheckBlock);
			break;
	}
	return true;
}

bool ConversationModule::ParseItemCheckBlock(Scanner &sc, FName key, bool isValue, ItemCheck &obj)
{
	if(isValue) switch(key)
	{
		default: return false;
		case NAME_Item:
			sc.MustGetToken(TK_IntConst);
			obj.Item = sc->number;
			break;
		case NAME_Amount:
			sc.MustGetToken(TK_IntConst);
			obj.Amount = sc->number;
			break;
	}
	else return false;
	return true;
}

bool ConversationModule::ParseChoiceBlock(Scanner &sc, FName key, bool isValue, Choice &obj)
{
	if(isValue) switch(key)
	{
		default: return false;
		case NAME_Text:
			sc.MustGetToken(TK_StringConst);
			obj.Text = sc->str;
			break;
		case NAME_DisplayCost:
			sc.MustGetToken(TK_BoolConst);
			obj.DisplayCost = sc->boolean;
			break;
		case NAME_YesMessage:
			sc.MustGetToken(TK_StringConst);
			obj.YesMessage = sc->str;
			break;
		case NAME_NoMessage:
			sc.MustGetToken(TK_StringConst);
			obj.NoMessage = sc->str;
			break;
		case NAME_Log:
			sc.MustGetToken(TK_StringConst);
			obj.Log = sc->str;
			break;
		case NAME_GiveItem:
			sc.MustGetToken(TK_IntConst);
			obj.GiveItem = sc->number;
			break;
		case NAME_Special:
			sc.MustGetToken(TK_IntConst);
			obj.Special = sc->number;
			break;
		case NAME_Arg0:
		case NAME_Arg1:
		case NAME_Arg2:
		case NAME_Arg3:
		case NAME_Arg4:
			sc.MustGetToken(TK_IntConst);
			obj.Arg[(int)key - (int)NAME_Arg0] = sc->number;
			break;
		case NAME_NextPage:
			sc.MustGetToken(TK_IntConst);
			obj.NextPageIndex = sc->number;
			break;
		case NAME_CloseDialog:
			sc.MustGetToken(TK_BoolConst);
			obj.CloseDialog = sc->boolean;
			break;
	}
	else switch(key)
	{
		default: return false;
		case NAME_Cost:
			ParseBlock(sc, obj.Cost[obj.Cost.Push(ItemCheck())], &ConversationModule::ParseItemCheckBlock);
			break;
	}
	return true;
}

// ----------------------------------------------------------------------------

#include "wl_game.h"
#include "wl_menu.h"
#include "id_us.h"
#include "id_vh.h"
#include "language.h"
#include "v_video.h"
#include "g_mapinfo.h"
#include "id_ca.h"
#include "wl_agent.h"
#include "v_text.h"

namespace Dialog {

static TMap<unsigned int, const ConversationModule::Page *> ConversationPosition;
static TArray<ConversationModule> LoadedModules;
static unsigned int MapModuleStart = 0;

static bool CheckModuleLoaded(int lump)
{
	for(unsigned int i = LoadedModules.Size();i-- > 0;)
	{
		if(LoadedModules[i].Lump == lump)
			return true;
	}
	return false;
}

void LoadGlobalModule(const char* moduleName)
{
	int lump;
	if((lump = Wads.CheckNumForName(moduleName)) != -1 && !CheckModuleLoaded(lump))
	{
		unsigned int module = LoadedModules.Push(ConversationModule());
		LoadedModules[module].Load(lump);
		for(unsigned int i = 0;i < LoadedModules[module].Include.Size();++i)
			LoadGlobalModule(LoadedModules[module].Include[i]);
	}
	MapModuleStart = LoadedModules.Size();
}

void LoadMapModules()
{
	// Unload any old modules
	LoadedModules.Delete(MapModuleStart, LoadedModules.Size()-MapModuleStart);

	// Reset conversations
	ConversationPosition.Clear();
}

const ConversationModule::Page **FindConversation(unsigned int id)
{
	// See if we have an active conversation going
	const ConversationModule::Page **page = ConversationPosition.CheckKey(id);
	if(page)
		return page;

	// If not, open a conversation
	for(unsigned int i = LoadedModules.Size();i-- > 0;)
	{
		if(const ConversationModule::Conversation *conv = LoadedModules[i].Find(id))
		{
			page = &ConversationPosition[id];
			*page = conv->Start();
			return page;
		}
	}

	// No conversation found.
	return NULL;
}

void ShowQuiz(unsigned int id)
{
	class QuizMenu : public Menu
	{
	public:
		QuizMenu() : Menu(30, 96, 290, 24) {}

		void loadQuestion(const ConversationModule::Page *page)
		{
			setHeadText(page->Name);

			question = page->Dialog;
			if(question[0] == '$')
				question = language[question.Mid(1)];

			hint = page->Hint;
			if(hint[0] == '$')
				hint = language[hint.Mid(1)];
			hint.Format("(%s)", hint.GetChars());

			for(unsigned int i = 0;i < page->Choices.Size();++i)
			{
				FString choice = page->Choices[i].Text;
				if(choice[0] == '$')
					choice = language[page->Choices[i].Text.Mid(1)];
				addItem(new MenuItem(choice));
			}
		}

		void drawBackground() const
		{
			DrawPlayScreen();
			VWB_DrawFill(TexMan(levelInfo->GetBorderTexture()), 0, statusbary1, screenWidth, statusbary2-statusbary1+CleanYfac);

			WindowX = 0;
			WindowW = 320;
			PrintY = 4;
			US_CPrint(BigFont, headText, CR_WHITE);

			DrawWindow(14, 21, 292, 134, BKGDCOLOR);
		}

		void draw() const
		{
			drawBackground();

			FBrokenLines *lines = V_BreakLines(BigFont, 280, question);
			unsigned int ly = 26;
			for(FBrokenLines *line = lines;line->Width != -1;++line)
			{
				screen->DrawText(BigFont, CR_WHITE, 26, ly, line->Text,
					DTA_Clean, true,
					TAG_DONE
				);
				ly += BigFont->GetHeight() + 1;
			}
			delete[] lines;

			screen->DrawText(BigFont, CR_WHITE, 26, ly, hint,
				DTA_Clean, true,
				TAG_DONE
			);

			drawMenu();

			if(!isAnimating())
				VWB_DrawGraphic (cursor, getX() - 4, getY() + getHeight(curPos) - 2, MENU_CENTER);
			VW_UpdateScreen ();
		}

	private:
		FString question;
		FString hint;
	};

	const ConversationModule::Page **page = FindConversation(1);
	if(!page)
		return;

	QuizMenu quiz;
	quiz.loadQuestion(*page);
	do
	{
		int answer = quiz.handle();
		if(answer >= 0)
		{
			// TODO: Play sound 41 on correct, 27 on incorrect.  Wait until sound done (which means picking a value for us)
			const ConversationModule::Choice &choice = (*page)->Choices[answer];
			FString response = choice.YesMessage;
			if(response[0] == '$')
				response = language[response.Mid(1)];

			quiz.drawBackground();

			int size = BigFont->StringWidth(response);
			screen->DrawText(BigFont, gameinfo.FontColors[GameInfo::MENU_SELECTION], 160-size/2, 88, response,
				DTA_Clean, true,
				TAG_DONE);

			VW_UpdateScreen();
			VL_WaitVBL(140);

			*page = choice.NextPage;
			if(choice.CloseDialog)
				break;
			else
				quiz.loadQuestion(*page);
		}
		else
		{
			US_ControlPanel(sc_Escape);
			Menu::closeMenus(false);
			if(startgame)
				return;
			VW_FadeOut();
			quiz.draw();
			VW_FadeIn();
		}
	} while(true);

	// Fix screen
	StatusBar->RefreshBackground();
	DrawPlayScreen();
}

}
