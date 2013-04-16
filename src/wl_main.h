#ifndef __WL_MAIN_H__
#define __WL_MAIN_H__

#include "id_vl.h"

/*
=============================================================================

							WL_MAIN DEFINITIONS

=============================================================================
*/

extern  bool     loadedgame;
extern  fixed    focallength;
extern  fixed    focallengthy;
extern  fixed    r_depthvisibility;
extern  int      viewscreenx, viewscreeny;
extern  int      viewwidth;
extern  int      viewheight;
extern  int      statusbarx;
extern  int      statusbary;
extern  short    centerx;
extern  short    centerxwide;
extern  int32_t  heightnumerator;
extern  fixed    scale;
extern  fixed    pspritexscale;
extern  fixed    pspriteyscale;
extern  fixed    yaspect;
extern  int      mouseadjustment;
extern  int      shootdelta;
extern  unsigned screenofs;

extern  bool     startgame;

//
// Command line parameter variables
//
extern  int      param_difficulty;
extern  const char* param_tedlevel;
extern  int      param_joystickindex;
extern  int      param_joystickhat;
extern  int      param_samplerate;
extern  int      param_audiobuffer;

void            NewGame (int difficulty,const class FString &map,const class ClassDef *playerClass=NULL);
void            CalcProjection (int32_t focal);
int				CheckRatio (int width, int height);
void            NewViewSize (int width, unsigned int scrWidth=screenWidth, unsigned int scrHeight=screenHeight);
void            ShutdownId (void);

#endif
