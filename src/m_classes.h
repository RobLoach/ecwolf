#ifndef __M_CLASSES_H__
#define __M_CLASSES_H__

#include "tarray.h"
#include "wl_def.h"
#include "v_font.h"
#include "zstring.h"

/*//////////////////////////////////////////////////////////////////////////////
//                         NEW MENU CODE
//
// The new menu code is an object oriented approach to the menu.  Each option
// has its own object which function is determined by the class used.  The base
// class, MenuItem, can be inherited for reusable types or controled through a
// listener which is a function whose prototype is produced with MENU_LISTENER.
//
//                        ABOUT LISTENERS
//
// A listener is a function which has a special prototype which is generated by
// MENU_LISTENER.  Simply pass the name of the listener into the macro to begin
// your function.  Pass the name of the listener into an object to use.  The
// return type is bool so be sure to return true.  (Return false when something
// is supposed to fail, but remember only some item types use the return value
// (read the docs to figure out when it is used.).)
//////////////////////////////////////////////////////////////////////////////*/

class FTexture;
class Menu;

#define MENU_LISTENER(name)				bool name(int which)
#define MENU_LISTENER_PROTOTYPE(name)	bool (*name)(int)
class MenuItem
{
	protected:
		MENU_LISTENER_PROTOTYPE(activateListener);
		bool		enabled;
		int			height;
		uint8_t		highlight; // Makes the font a different color, not to be confused with the item being selected.
		FTexture	*picture;
		int			pictureX;
		int			pictureY;
		char		string[80];
		bool		visible;
		const Menu	*menu;
		FString		activateSound;

		EColorRange	getTextColor() const;

	public:
				MenuItem(const char string[80], MENU_LISTENER_PROTOTYPE(activateListener)=NULL);
		virtual	~MenuItem() {}

		short		getActive() const { return enabled ? (highlight + 1) : 0; }
		int			getHeight() const { return visible ? height : 0; }
		const char	*getString() const { return string; }
		FString		getActivateSound() const { return activateSound; }
		bool		isEnabled() const { return enabled && visible; }
		bool		isHighlighted() const { return highlight != 0; }
		bool		isSelected() const;
		bool		isVisible() const { return visible; }
		void		setActivateListener(MENU_LISTENER_PROTOTYPE(activateListener)) { this->activateListener = activateListener; }
		void		setActivateSound(FString sound) { activateSound = sound; }
		void		setEnabled(bool enabled=true) { this->enabled = enabled; }
		void		setHighlighted(int highlight=1) { this->highlight = highlight; }
		void		setMenu(const Menu *menu) { this->menu = menu; }
		void		setPicture(const char* picture, int x=-1, int y=-1);
		void		setText(const char string[80]);
		void		setVisible(bool visible=true) { this->visible = visible; }

		virtual void	activate();
		virtual void	draw();
		virtual void	left() {}
		virtual void	right() {}
		virtual bool	playActivateSound() { return true; }
};

class LabelMenuItem : public MenuItem
{
	public:
		LabelMenuItem(const char string[36]);

		void draw();
};

class BooleanMenuItem : public MenuItem
{
	protected:
		bool	&value;

	public:
		BooleanMenuItem(const char string[36], bool &value, MENU_LISTENER_PROTOTYPE(activateListener)=NULL);

		void	activate();
		void	draw();
};

class MenuSwitcherMenuItem : public MenuItem
{
	protected:
		Menu	&menu;

	public:
		/**
		 * @param activateListener Executes when activated.  If returns false, menu will not switch.
		 */
		MenuSwitcherMenuItem(const char string[36], Menu &menu, MENU_LISTENER_PROTOTYPE(activateListener)=NULL);

		void	activate();
};

class SliderMenuItem : public MenuItem
{
	protected:
		char		begString[36];
		int			&value;
		const int	width;
		const int	max;

	public:
		SliderMenuItem(int &value, int width, int max, const char begString[36]="", const char endString[36]="", MENU_LISTENER_PROTOTYPE(activateListener)=NULL);

		void	draw();
		void	left();
		void	right();
		bool	playActivateSound() { return false; }
};

class MultipleChoiceMenuItem : public MenuItem
{
	protected:
		int					curOption;
		const unsigned int	numOptions;
		char**				options;

	public:
		/**
		 * @param options Name of the possible options.  Use NULL to indicate a disabled option as to keep the positions correct.
		 */
		MultipleChoiceMenuItem(MENU_LISTENER_PROTOTYPE(changeListener), const char** options, unsigned int numOptions, int curOption=0);
		~MultipleChoiceMenuItem();

		void	activate();
		void	draw();
		void	left();
		void	right();
};

class TextInputMenuItem : public MenuItem
{
	protected:
		bool			clearFirst;
		unsigned int	max;
		MENU_LISTENER_PROTOTYPE(preeditListener);
		FString			value;

	public:
		/**
		 * @param preeditListener Executed before editing the information, if returns false the field will not be edited.
		 * @param posteditListener Execued after editing the information.
		 */
		TextInputMenuItem(const FString &text, unsigned int max, MENU_LISTENER_PROTOTYPE(preeditListener)=NULL, MENU_LISTENER_PROTOTYPE(posteditListener)=NULL, bool clearFirst=false);

		void		activate();
		void		draw();
		const char	*getValue() const { return value; }
		void		setValue(const FString &text) { value = text; }
};

class ControlMenuItem : public MenuItem
{
	protected:
		ControlScheme				&button;
		static int					column;
		static const char* const	keyNames[512];

	public:
		ControlMenuItem(ControlScheme &button);

		void	activate();
		void	draw();
		void	left();
		void	right();
};

class Menu
{
	protected:
		MENU_LISTENER_PROTOTYPE(entryListener);
		bool				animating;
		static bool			close;
		bool				controlHeaders;
		int					curPos;
		FTexture			*headPicture;
		char				headText[36];
		bool				headTextInStripes;
		bool				headPictureIsAlternate;
		int					height;
		const int			indent;
		TArray<MenuItem *>	items;
		const int			x;
		const int			y;
		const int			w;

		unsigned int			itemOffset; // scrolling menus
		static unsigned int		lastIndexDrawn;

		static FTexture			*cursor;

		void	drawGunHalfStep(int x, int y);
		void	eraseGun(int x, int y);
		virtual void handleDelete() {}

	public:
		Menu(int x, int y, int w, int indent, MENU_LISTENER_PROTOTYPE(entryListener)=NULL);
		virtual ~Menu();

		void			addItem(MenuItem *item);
		static bool		areMenusClosed() { return close; }
		void			clear();
		static void		closeMenus(bool close=true);
		unsigned int	countItems() const;
		void			drawMenu() const;
		virtual void	draw() const;
		int				handle();
		int				getCurrentPosition() const { return curPos; }
		/**
		 * @param position If not -1, returns the height to the specified position.
		 */
		int				getHeight(int position=-1) const;
		int				getIndent() const { return indent; }
		MenuItem		*getIndex(int index) const;
		int				getNumItems() const { return items.Size(); }
		int				getWidth() const { return w; }
		int				getX() const { return x; }
		int				getY() const { return y; }
		bool			isAnimating() const { return animating; }
		void			setCurrentPosition(int position);
		void			setHeadPicture(const char* picture, bool isAlt=false);
		void			setHeadText(const char text[36], bool drawInStripes=false);
		void			show();
		/**
		 * Should this menu show the Key, Mse. and Joy headers?
		 */
		void			showControlHeaders(bool show) { controlHeaders = show; }
		MenuItem		*operator[] (int index) { return getIndex(index); }
};

extern Menu	mainMenu;

#endif /* __M_CLASSES_H__ */
