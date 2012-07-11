// ID_VL.H

#ifndef __ID_VL_H__
#define __ID_VL_H__

// wolf compatability

void Quit (const char *error,...);

//===========================================================================

extern SDL_Surface *screen, *screenBuffer, *curSurface;

extern  bool	fullscreen, usedoublebuffering;
extern  unsigned screenWidth, screenHeight, screenBits, screenPitch, bufferPitch, curPitch;
extern  unsigned scaleFactor;

extern	bool  screenfaded;
extern	unsigned bordercolor;

extern SDL_Color gamepal[256];

//===========================================================================

//
// VGA hardware routines
//

#define VL_WaitVBL(a) SDL_Delay((a)*8)

void VL_ReadPalette();

void VL_SetVGAPlaneMode (void);
void VL_SetTextMode (void);

void VL_SetBlend(uint8_t red, uint8_t green, uint8_t blue, int amount, bool forceupdate=false);
void VL_SetPalette  (struct PalEntry *palette, bool forceupdate);
void VL_SetPalette  (SDL_Color *palette, bool forceupdate);
void VL_FadeOut     (int start, int end, int red, int green, int blue, int steps);
void VL_FadeIn      (int start, int end, SDL_Color *palette, int steps);

byte *VL_LockSurface(SDL_Surface *surface);
void VL_UnlockSurface(SDL_Surface *surface);

void VL_BarScaledCoord  (int scx, int scy, int scwidth, int scheight, int color);
void inline VL_Bar      (int x, int y, int width, int height, int color)
{
	VL_BarScaledCoord(scaleFactor*x, scaleFactor*y,
		scaleFactor*width, scaleFactor*height, color);
}
void inline VL_ClearScreen(int color)
{
	SDL_FillRect(curSurface, NULL, color);
}

#endif
