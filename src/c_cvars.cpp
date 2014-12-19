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
#include "am_map.h"
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

bool alwaysrun;
bool mouseenabled, mouseyaxisdisabled, joystickenabled;


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

	AM_UpdateFlags();

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
	int uniScreenWidth = 0, uniScreenHeight = 0;
	SettingsData * sd = NULL;

	config.CreateSetting("ForceGrabMouse", false);
	config.CreateSetting("MouseEnabled", 1);
	config.CreateSetting("JoystickEnabled", 0);
	config.CreateSetting("ViewSize", 19);
	config.CreateSetting("MouseXAdjustment", 5);
	config.CreateSetting("MouseYAdjustment", 5);
	config.CreateSetting("PanXAdjustment", 5);
	config.CreateSetting("PanYAdjustment", 5);
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
	config.CreateSetting("FullScreenWidth", fullScreenWidth);
	config.CreateSetting("FullScreenHeight", fullScreenHeight);
	config.CreateSetting("WindowedScreenWidth", windowedScreenWidth);
	config.CreateSetting("WindowedScreenHeight", windowedScreenHeight);
	config.CreateSetting("QuitOnEscape", quitonescape);
	config.CreateSetting("MoveBob", FRACUNIT);
	config.CreateSetting("Gamma", 1.0f);
	config.CreateSetting("AM_Rotate", 0);
	config.CreateSetting("AM_DrawTexturedWalls", true);
	config.CreateSetting("AM_DrawFloors", false);
	config.CreateSetting("AM_Overlay", 0);
	config.CreateSetting("AM_OverlayTextured", false);
	config.CreateSetting("AM_Pause", true);
	config.CreateSetting("AM_ShowRatios", false);

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
	mousexadjustment = config.GetSetting("MouseXAdjustment")->GetInteger();
	mouseyadjustment = config.GetSetting("MouseYAdjustment")->GetInteger();
	panxadjustment = config.GetSetting("PanXAdjustment")->GetInteger();
	panyadjustment = config.GetSetting("PanYAdjustment")->GetInteger();
	mouseyaxisdisabled = config.GetSetting("MouseYAxisDisabled")->GetInteger() != 0;
	alwaysrun = config.GetSetting("AlwaysRun")->GetInteger() != 0;
	AdlibVolume = config.GetSetting("SoundVolume")->GetInteger();
	SD_UpdatePCSpeakerVolume();
	MusicVolume = config.GetSetting("MusicVolume")->GetInteger();
	SoundVolume = config.GetSetting("DigitizedVolume")->GetInteger();
	vid_fullscreen = config.GetSetting("Vid_FullScreen")->GetInteger() != 0;
	vid_aspect = static_cast<Aspect>(config.GetSetting("Vid_Aspect")->GetInteger());
	fullScreenWidth = config.GetSetting("FullScreenWidth")->GetInteger();
	fullScreenHeight = config.GetSetting("FullScreenHeight")->GetInteger();
	windowedScreenWidth = config.GetSetting("WindowedScreenWidth")->GetInteger();
	windowedScreenHeight = config.GetSetting("WindowedScreenHeight")->GetInteger();
	if ((sd = config.GetSetting("ScreenWidth")) != NULL)
	{
		uniScreenWidth = sd->GetInteger();
		config.DeleteSetting("ScreenWidth");
	}

	if ((sd = config.GetSetting("ScreenHeight")) != NULL)
	{
		uniScreenHeight = sd->GetInteger();
		config.DeleteSetting("ScreenHeight");
	}
	quitonescape = config.GetSetting("QuitOnEscape")->GetInteger() != 0;
	movebob = config.GetSetting("MoveBob")->GetInteger();
	screenGamma = static_cast<float>(config.GetSetting("Gamma")->GetFloat());
	am_rotate = config.GetSetting("AM_Rotate")->GetInteger();
	am_drawtexturedwalls = config.GetSetting("AM_DrawTexturedWalls")->GetInteger() != 0;
	am_drawfloors = config.GetSetting("AM_DrawFloors")->GetInteger() != 0;
	am_overlay = config.GetSetting("AM_Overlay")->GetInteger();
	am_overlaytextured = config.GetSetting("AM_OverlayTextured")->GetInteger() != 0;
	am_pause = config.GetSetting("AM_Pause")->GetInteger() != 0;
	am_showratios = config.GetSetting("AM_ShowRatios")->GetInteger() != 0;

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
		if(config.GetSetting(hsCompleted)->GetType() == SettingsData::ST_STR)
			Scores[i].completed = config.GetSetting(hsCompleted)->GetString();
		else
			Scores[i].completed.Format("%d", config.GetSetting(hsCompleted)->GetInteger());
		strncpy(Scores[i].graphic, config.GetSetting(hsGraphic)->GetString(), 8);
		Scores[i].graphic[8] = 0;
	}

	// make sure values are correct

	if(mouseenabled) mouseenabled=true;
	if(joystickenabled) joystickenabled=true;

	if (mousexadjustment<0) mousexadjustment = 0;
	else if (mousexadjustment>20) mousexadjustment = 20;

	if (mouseyadjustment<0) mouseyadjustment = 0;
	else if (mouseyadjustment>20) mouseyadjustment = 20;

	if (panxadjustment<0) panxadjustment = 0;
	else if (panxadjustment>20) panxadjustment = 20;

	if (panyadjustment<0) panyadjustment = 0;
	else if (panyadjustment>20) panyadjustment = 20;

	if(viewsize<4) viewsize=4;
	else if(viewsize>21) viewsize=21;

	// Carry over the unified screenWidth/screenHeight from previous versions
	// Overwrite the full*/windowed* variables, because they're (most likely) defaulted anyways
	if(uniScreenWidth != 0)
	{
		fullScreenWidth = uniScreenWidth;
		windowedScreenWidth = uniScreenWidth;
	}

	if(uniScreenHeight != 0)
	{
		fullScreenHeight = uniScreenHeight;
		windowedScreenHeight = uniScreenHeight;
	}

	// Set screenHeight, screenWidth
	if(vid_fullscreen)
	{
		screenHeight = fullScreenHeight;
		screenWidth = fullScreenWidth;
	}
	else
	{
		screenHeight = windowedScreenHeight;
		screenWidth = windowedScreenWidth;
	}
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
	config.GetSetting("MouseXAdjustment")->SetValue(mousexadjustment);
	config.GetSetting("MouseYAdjustment")->SetValue(mouseyadjustment);
	config.GetSetting("PanXAdjustment")->SetValue(panxadjustment);
	config.GetSetting("PanYAdjustment")->SetValue(panyadjustment);
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
	config.GetSetting("FullScreenWidth")->SetValue(fullScreenWidth);
	config.GetSetting("FullScreenHeight")->SetValue(fullScreenHeight);
	config.GetSetting("WindowedScreenWidth")->SetValue(windowedScreenWidth);
	config.GetSetting("WindowedScreenHeight")->SetValue(windowedScreenHeight);
	config.GetSetting("QuitOnEscape")->SetValue(quitonescape);
	config.GetSetting("MoveBob")->SetValue(movebob);
	config.GetSetting("Gamma")->SetValue(screenGamma);
	config.GetSetting("AM_Rotate")->SetValue(am_rotate);
	config.GetSetting("AM_DrawTexturedWalls")->SetValue(am_drawtexturedwalls);
	config.GetSetting("AM_DrawFloors")->SetValue(am_drawfloors);
	config.GetSetting("AM_Overlay")->SetValue(am_overlay);
	config.GetSetting("AM_OverlayTextured")->SetValue(am_overlaytextured);
	config.GetSetting("AM_Pause")->SetValue(am_pause);
	config.GetSetting("AM_ShowRatios")->SetValue(am_showratios);

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
