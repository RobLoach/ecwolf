#ifndef __WL_ACT_H__
#define __WL_ACT_H__

#include "wl_def.h"

class AActor;

bool CheckMeleeRange(AActor *actor1, AActor *actor2);
void A_Face(AActor *self, AActor *target, angle_t maxturn=0);

#endif
