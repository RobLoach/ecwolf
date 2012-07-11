// ID_VH.H

#ifndef __ID_VH_H__
#define __ID_VH_H__

#include "id_vl.h"
#include "wl_main.h"
#include "v_font.h"

#define WHITE			15			// graphics mode independant colors
#define BLACK			0
#define FIRSTCOLOR		1
#define SECONDCOLOR		12
#define F_WHITE			15
#define F_BLACK			0
#define F_FIRSTCOLOR	1
#define F_SECONDCOLOR	12

//===========================================================================

#define MAXSHIFTS	1

typedef struct
{
	int16_t height;
	int16_t location[256];
	int8_t width[256];
} fontstruct;


//===========================================================================

extern	int             pa,px,py;

//
// mode independant routines
// coordinates in pixels, rounded to best screen res
// regions marked in double buffer
//

class FTexture;
enum MenuOffset
{
	MENU_NONE = 0,
	MENU_TOP = 1,
	MENU_CENTER = 2,
	MENU_BOTTOM = 3
};
void VWB_Clear(int color, int x1, int y1, int x2, int y2);
void VWB_DrawFill(FTexture *tex, unsigned int ix, unsigned int iy, unsigned int iw, unsigned int ih);
void VWB_DrawGraphic(FTexture *tex, int ix, int iy, double wd, double hd, MenuOffset menu=MENU_NONE, struct FRemapTable *remap=NULL, bool stencil=false, BYTE stencilcolor=0);
void VWB_DrawGraphic(FTexture *tex, int ix, int iy, MenuOffset menu=MENU_NONE, struct FRemapTable *remap=NULL, bool stencil=false, BYTE stencilcolor=0);
void VirtualToRealCoords(double &x, double &y, double &w, double &h, double vwidth, double vheight, bool vbottom, bool handleaspect);
template<class T> void MenuToRealCoords(T &x, T &y, T &w, T &h, MenuOffset offset)
{
	x = screenWidth/2 + (x-160)*scaleFactor;
	switch(offset)
	{
		default:
			y = screenHeight/2 + (y-100)*scaleFactor;
			break;
		case MENU_TOP:
			y *= scaleFactor;
			break;
		case MENU_BOTTOM:
			y = screenHeight + (y-200)*scaleFactor;
			break;
	}
	w *= scaleFactor;
	h *= scaleFactor;
}

void VWB_DrawPropString(FFont *font, const char *string, EColorRange translation=CR_UNTRANSLATED, bool stencil=false, BYTE stencilcolor=0);
void VWB_DrawPropStringWrap(unsigned int wrapWidth, unsigned int wrapHeight, FFont *font, const char *string, EColorRange translation=CR_UNTRANSLATED, bool stencil=false, BYTE stencilcolor=0);

void VWB_Bar (int x, int y, int width, int height, int color);
#define VWB_BarScaledCoord VL_BarScaledCoord
void VWB_Hlin (int x1, int x2, int y, int color);
void VWB_Vlin (int y1, int y2, int x, int color);
#define VWB_HlinScaledCoord VW_Hlin
#define VWB_VlinScaledCoord VW_Vlin

void VH_UpdateScreen();
#define VW_UpdateScreen VH_UpdateScreen

//
// wolfenstein EGA compatability stuff
//


#define VW_Shutdown		    VL_Shutdown
#define VW_Bar			    VL_Bar
#define VW_Plot			    VL_Plot
#define VW_Hlin(x,z,y,c)	VL_Hlin(x,y,(z)-(x)+1,c)
#define VW_Vlin(y,z,x,c)	VL_Vlin(x,y,(z)-(y)+1,c)
#define VW_DrawPic		    VH_DrawPic
#define VW_WaitVBL		    VL_WaitVBL
#define VW_FadeIn()		    VL_FadeIn(0,255,gamepal,30);
#define VW_FadeOut()	    VL_FadeOut(0,255,0,0,0,30);
void	VW_MeasurePropString (FFont *font, const char *string, word &width, word &height, word *finalWidth=NULL);

void    VH_Startup();
bool FizzleFade (SDL_Surface *source, int x1, int y1,
	unsigned width, unsigned height, unsigned frames, bool abortable);

#endif
