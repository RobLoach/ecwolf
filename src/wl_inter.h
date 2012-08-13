#ifndef __WL_INTER_H__
#define __WL_INTER_H__

/*
=============================================================================

								WL_INTER

=============================================================================
*/

extern struct LRstruct
{
	unsigned int killratio;
	unsigned int secretsratio;
	unsigned int treasureratio;
	unsigned int numLevels;
	unsigned int time;
} LevelRatios;

void DrawHighScores(void);
void CheckHighScore (int32_t score,word other);
void Victory (bool fromIntermission);
void LevelCompleted (void);
void ClearSplitVWB (void);

void PreloadGraphics(bool showPsych);

#endif
