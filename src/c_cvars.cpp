/*
** c_cvars.cpp
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
**
*/

#include "c_cvars.h"
#include "config.h"
#include "wl_def.h"
#include "id_sd.h"
#include "id_in.h"
#include "id_us.h"
#include "wl_main.h"
#include "wl_play.h"

static bool doWriteConfig = false;

Aspect r_ratio = ASPECT_4_3, vid_aspect = ASPECT_NONE;
bool forcegrabmouse = false;
bool vid_fullscreen = false;
bool quitonescape = false;
fixed movebob = FRACUNIT;

void FinalReadConfig()
{
	SDMode  sd;
	SMMode  sm;
	SDSMode sds;

	sd = static_cast<SDMode> (config.GetSetting("SoundDevice")->GetInteger());
	sm = static_cast<SMMode> (config.GetSetting("MusicDevice")->GetInteger());
	sds = static_cast<SDSMode> (config.GetSetting("DigitalSoundDevice")->GetInteger());

	if ((sd == sdm_AdLib || sm == smm_AdLib) && !AdLibPresent
			&& !SoundBlasterPresent)
	{
		sd = sdm_PC;
		sm = smm_Off;
	}

	if ((sds == sds_SoundBlaster && !SoundBlasterPresent))
		sds = sds_Off;

	SD_SetMusicMode(sm);
	SD_SetSoundMode(sd);
	SD_SetDigiDevice(sds);

	if (!MousePresent)
		mouseenabled = false;
	if (!IN_JoyPresent())
		joystickenabled = false;

	doWriteConfig = true;
}

/*
====================
=
= ReadConfig
=
====================
*/

void ReadConfig(void)
{
	config.CreateSetting("ForceGrabMouse", false);
	config.CreateSetting("MouseEnabled", 1);
	config.CreateSetting("JoystickEnabled", 0);
	config.CreateSetting("ViewSize", 19);
	config.CreateSetting("MouseAdjustment", 5);
	config.CreateSetting("SoundDevice", sdm_AdLib);
	config.CreateSetting("MusicDevice", smm_AdLib);
	config.CreateSetting("DigitalSoundDevice", sds_SoundBlaster);
	config.CreateSetting("AlwaysRun", 0);
	config.CreateSetting("MouseYAxisDisabled", 0);
	config.CreateSetting("SoundVolume", MAX_VOLUME);
	config.CreateSetting("MusicVolume", MAX_VOLUME);
	config.CreateSetting("DigitizedVolume", MAX_VOLUME);
	config.CreateSetting("Vid_FullScreen", false);
	config.CreateSetting("Vid_Aspect", ASPECT_NONE);
	config.CreateSetting("ScreenWidth", screenWidth);
	config.CreateSetting("ScreenHeight", screenHeight);
	config.CreateSetting("QuitOnEscape", quitonescape);
	config.CreateSetting("MoveBob", FRACUNIT);
	config.CreateSetting("Gamma", 1.0f);

	char joySettingName[50] = {0};
	char keySettingName[50] = {0};
	char mseSettingName[50] = {0};
	forcegrabmouse = config.GetSetting("ForceGrabMouse")->GetInteger() != 0;
	mouseenabled = config.GetSetting("MouseEnabled")->GetInteger() != 0;
	joystickenabled = config.GetSetting("JoystickEnabled")->GetInteger() != 0;
	for(unsigned int i = 0;controlScheme[i].button != bt_nobutton;i++)
	{
		sprintf(joySettingName, "Joystick_%s", controlScheme[i].name);
		sprintf(keySettingName, "Keybaord_%s", controlScheme[i].name);
		sprintf(mseSettingName, "Mouse_%s", controlScheme[i].name);
		for(unsigned int j = 0;j < 50;j++)
		{
			if(joySettingName[j] == ' ')
				joySettingName[j] = '_';
			if(keySettingName[j] == ' ')
				keySettingName[j] = '_';
			if(mseSettingName[j] == ' ')
				mseSettingName[j] = '_';
		}
		config.CreateSetting(joySettingName, controlScheme[i].joystick);
		config.CreateSetting(keySettingName, controlScheme[i].keyboard);
		config.CreateSetting(mseSettingName, controlScheme[i].mouse);
		controlScheme[i].joystick = config.GetSetting(joySettingName)->GetInteger();
		controlScheme[i].keyboard = config.GetSetting(keySettingName)->GetInteger();
		controlScheme[i].mouse = config.GetSetting(mseSettingName)->GetInteger();
	}
	viewsize = config.GetSetting("ViewSize")->GetInteger();
	mouseadjustment = config.GetSetting("MouseAdjustment")->GetInteger();
	mouseyaxisdisabled = config.GetSetting("MouseYAxisDisabled")->GetInteger() != 0;
	alwaysrun = config.GetSetting("AlwaysRun")->GetInteger() != 0;
	AdlibVolume = config.GetSetting("SoundVolume")->GetInteger();
	SD_UpdatePCSpeakerVolume();
	MusicVolume = config.GetSetting("MusicVolume")->GetInteger();
	SoundVolume = config.GetSetting("DigitizedVolume")->GetInteger();
	vid_fullscreen = config.GetSetting("Vid_FullScreen")->GetInteger() != 0;
	vid_aspect = static_cast<Aspect>(config.GetSetting("Vid_Aspect")->GetInteger());
	screenWidth = config.GetSetting("ScreenWidth")->GetInteger();
	screenHeight = config.GetSetting("ScreenHeight")->GetInteger();
	quitonescape = config.GetSetting("QuitOnEscape")->GetInteger() != 0;
	movebob = config.GetSetting("MoveBob")->GetInteger();
	screenGamma = config.GetSetting("Gamma")->GetFloat();

	char hsName[50];
	char hsScore[50];
	char hsCompleted[50];
	char hsGraphic[50];
	for(unsigned int i = 0;i < MaxScores;i++)
	{
		sprintf(hsName, "HighScore%u_Name", i);
		sprintf(hsScore, "HighScore%u_Score", i);
		sprintf(hsCompleted, "HighScore%u_Completed", i);
		sprintf(hsGraphic, "HighScore%u_Graphic", i);

		config.CreateSetting(hsName, Scores[i].name);
		config.CreateSetting(hsScore, Scores[i].score);
		config.CreateSetting(hsCompleted, Scores[i].completed);
		config.CreateSetting(hsGraphic, Scores[i].graphic);

		strcpy(Scores[i].name, config.GetSetting(hsName)->GetString());
		Scores[i].score = config.GetSetting(hsScore)->GetInteger();
		Scores[i].completed = config.GetSetting(hsCompleted)->GetInteger();
		strncpy(Scores[i].graphic, config.GetSetting(hsGraphic)->GetString(), 8);
		Scores[i].graphic[8] = 0;
	}

	// make sure values are correct

	if(mouseenabled) mouseenabled=true;
	if(joystickenabled) joystickenabled=true;

	if(mouseadjustment<0) mouseadjustment=0;
	else if(mouseadjustment>20) mouseadjustment=20;

	if(viewsize<4) viewsize=4;
	else if(viewsize>21) viewsize=21;
}

/*
====================
=
= WriteConfig
=
====================
*/

void WriteConfig(void)
{
	if(!doWriteConfig)
		return;

	char joySettingName[50] = {0};
	char keySettingName[50] = {0};
	char mseSettingName[50] = {0};
	config.GetSetting("ForceGrabMouse")->SetValue(forcegrabmouse);
	config.GetSetting("MouseEnabled")->SetValue(mouseenabled);
	config.GetSetting("JoystickEnabled")->SetValue(joystickenabled);
	for(unsigned int i = 0;controlScheme[i].button != bt_nobutton;i++)
	{
		sprintf(joySettingName, "Joystick_%s", controlScheme[i].name);
		sprintf(keySettingName, "Keybaord_%s", controlScheme[i].name);
		sprintf(mseSettingName, "Mouse_%s", controlScheme[i].name);
		for(unsigned int j = 0;j < 50;j++)
		{
			if(joySettingName[j] == ' ')
				joySettingName[j] = '_';
			if(keySettingName[j] == ' ')
				keySettingName[j] = '_';
			if(mseSettingName[j] == ' ')
				mseSettingName[j] = '_';
		}
		config.GetSetting(joySettingName)->SetValue(controlScheme[i].joystick);
		config.GetSetting(keySettingName)->SetValue(controlScheme[i].keyboard);
		config.GetSetting(mseSettingName)->SetValue(controlScheme[i].mouse);
	}
	config.GetSetting("ViewSize")->SetValue(viewsize);
	config.GetSetting("MouseAdjustment")->SetValue(mouseadjustment);
	config.GetSetting("MouseYAxisDisabled")->SetValue(mouseyaxisdisabled);
	config.GetSetting("AlwaysRun")->SetValue(alwaysrun);
	config.GetSetting("SoundDevice")->SetValue(SoundMode);
	config.GetSetting("MusicDevice")->SetValue(MusicMode);
	config.GetSetting("DigitalSoundDevice")->SetValue(DigiMode);
	config.GetSetting("SoundVolume")->SetValue(AdlibVolume);
	config.GetSetting("MusicVolume")->SetValue(MusicVolume);
	config.GetSetting("DigitizedVolume")->SetValue(SoundVolume);
	config.GetSetting("Vid_FullScreen")->SetValue(vid_fullscreen);
	config.GetSetting("Vid_Aspect")->SetValue(vid_aspect);
	config.GetSetting("ScreenWidth")->SetValue(screenWidth);
	config.GetSetting("ScreenHeight")->SetValue(screenHeight);
	config.GetSetting("QuitOnEscape")->SetValue(quitonescape);
	config.GetSetting("MoveBob")->SetValue(movebob);
	config.GetSetting("Gamma")->SetValue(screenGamma);

	char hsName[50];
	char hsScore[50];
	char hsCompleted[50];
	char hsGraphic[50];
	for(unsigned int i = 0;i < MaxScores;i++)
	{
		sprintf(hsName, "HighScore%u_Name", i);
		sprintf(hsScore, "HighScore%u_Score", i);
		sprintf(hsCompleted, "HighScore%u_Completed", i);
		sprintf(hsGraphic, "HighScore%u_Graphic", i);

		config.GetSetting(hsName)->SetValue(Scores[i].name);
		config.GetSetting(hsScore)->SetValue(Scores[i].score);
		config.GetSetting(hsCompleted)->SetValue(Scores[i].completed);
		config.GetSetting(hsGraphic)->SetValue(Scores[i].graphic);
	}

	config.SaveConfig();
}
