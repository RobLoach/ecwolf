//
//      ID Engine
//      ID_SD.h - Sound Manager Header
//      Version for Wolfenstein
//      By Jason Blochowiak
//

#ifndef __ID_SD__
#define __ID_SD__

#include "wl_def.h"
#include "sndinfo.h"

#define alOut(n,b) 		YM3812Write(oplChip, n, b, AdlibVolume)
#define alOutMusic(n,b)	YM3812Write(oplChip, n, b, MusicVolume)

typedef enum
{
	sdm_Off,
	sdm_PC,sdm_AdLib,
} SDMode;

typedef enum
{
	smm_Off,smm_AdLib
} SMMode;

typedef enum
{
	sds_Off,sds_PC,sds_SoundBlaster
} SDSMode;

#pragma pack(push, 1)
typedef struct
{
	longword        length;
	word            priority;
} SoundCommon;

#define ORIG_SOUNDCOMMON_SIZE 6

//      PC Sound stuff
#define pcTimer         0x42
#define pcTAccess       0x43
#define pcSpeaker       0x61

#define pcSpkBits       3

typedef struct
{
	SoundCommon     common;
	byte            data[1];
} PCSound;

//      Register addresses
// Operator stuff
#define alChar          0x20
#define alScale         0x40
#define alAttack        0x60
#define alSus           0x80
#define alWave          0xe0
// Channel stuff
#define alFreqL         0xa0
#define alFreqH         0xb0
#define alFeedCon       0xc0
// Global stuff
#define alEffects       0xbd

typedef struct
{
	byte    mChar,cChar,
			mScale,cScale,
			mAttack,cAttack,
			mSus,cSus,
			mWave,cWave,
			nConn,

			// These are only for Muse - these bytes are really unused
			voice,
			mode;
	byte    unused[3];
} Instrument;

#define ORIG_INSTRUMENT_SIZE 16

typedef struct
{
	SoundCommon     common;
	Instrument      inst;
	byte            block;
	byte            data[1];
} AdLibSound;

#define ORIG_ADLIBSOUND_SIZE (ORIG_SOUNDCOMMON_SIZE + ORIG_INSTRUMENT_SIZE + 2)

//
//      Sequencing stuff
//
#define sqMaxTracks     10

typedef struct
{
	word    length;
	word    values[1];
} MusicGroup;

typedef struct
{
	int valid;
	fixed globalsoundx, globalsoundy;
} globalsoundpos;
#pragma pack(pop)

extern globalsoundpos channelSoundPos[];

// Global variables
extern  bool			AdLibPresent,
						SoundBlasterPresent,
						SoundPositioned;
extern  SDMode          SoundMode;
extern  SDSMode         DigiMode;
extern  SMMode          MusicMode;
static const int MAX_VOLUME = 20;
static inline double MULTIPLY_VOLUME(const int &v)
{
	return (double(v)+0.3)/(MAX_VOLUME+0.3);
}
extern	int				AdlibVolume;
extern	int				MusicVolume;
extern	int				SoundVolume;
extern bool SD_UpdatePCSpeakerVolume(int which=0);

enum SoundChannel
{
	SD_GENERIC = -1,
	SD_WEAPONS,
	SD_BOSSWEAPONS
};

#define GetTimeCount()  ((SDL_GetTicks()*7)/100)

inline void Delay(int wolfticks)
{
	if(wolfticks>0) SDL_Delay(wolfticks * 100 / 7);
}

// Function prototypes
extern  void    SD_Startup(void),
				SD_Shutdown(void);

extern  void    SD_PositionSound(int leftvol,int rightvol);
extern  bool	SD_PlaySound(const char* sound,SoundChannel chan=SD_GENERIC);
extern  void    SD_SetPosition(int channel, int leftvol,int rightvol);
extern  void    SD_StopSound(void),
				SD_WaitSoundDone(void);

extern  void    SD_StartMusic(const char* chunk);
extern  void    SD_ContinueMusic(const char* chunk, int startoffs);
extern  void    SD_MusicOn(void),
				SD_FadeOutMusic(void);
extern  int     SD_MusicOff(void);

extern  bool	SD_MusicPlaying(void);
extern  bool	SD_SetSoundMode(SDMode mode);
extern  bool	SD_SetMusicMode(SMMode mode);
extern  bool    SD_SoundPlaying(void);

extern  void    SD_SetDigiDevice(SDSMode);
extern  byte*	SD_PrepareSound(int which);
extern  int     SD_PlayDigitized(const SoundData &which,int leftpos,int rightpos,SoundChannel chan=SD_GENERIC);
extern  void    SD_StopDigitized(void);

#endif
