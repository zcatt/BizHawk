//---------------------------------------------------------------------------
// NEOPOP : Emulator as in Dreamland
//
// Copyright (c) 2001-2002 by neopop_uk
//---------------------------------------------------------------------------

//---------------------------------------------------------------------------
//	This program is free software; you can redistribute it and/or modify
//	it under the terms of the GNU General Public License as published by
//	the Free Software Foundation; either version 2 of the License, or
//	(at your option) any later version. See also the license.txt file for
//	additional informations.
//---------------------------------------------------------------------------

#include "neopop.h"

#include "Z80_interface.h"
#include "interrupt.h"
#include "mem.h"
#include "gfx.h"
#include "sound.h"
#include "dma.h"
#include "bios.h"
#include "flash.h"

#include <algorithm>

namespace MDFN_IEN_NGP
{

extern uint8 CPUExRAM[16384];

NGPGFX_CLASS *NGPGfx = NULL;

COLOURMODE system_colour = COLOURMODE_AUTO;

uint8 NGPJoyLatch;
uint8 settings_language;
time_t frontend_time;

bool system_comms_read(uint8 *buffer)
{
	return (0);
}

bool system_comms_poll(uint8 *buffer)
{
	return (0);
}

void system_comms_write(uint8 data)
{
	return;
}

void instruction_error(char *vaMessage, ...)
{
	/*char message[1000];
	va_list vl;

	va_start(vl, vaMessage);
	vsprintf(message, vaMessage, vl);
	va_end(vl);

	MDFN_printf("[PC %06X] %s\n", pc, message);*/
}

bool NGPFrameSkip;
int32 ngpc_soundTS = 0;
//static int32 main_timeaccum;
static int32 z80_runtime;

static void Emulate(EmulateSpecStruct *espec)
{
	bool MeowMeow = 0;
	MDFN_Surface surface;
	surface.pixels = espec->pixels;
	surface.pitch32 = 160;

	storeB(0x6f82, espec->Buttons);

	ngpc_soundTS = 0;
	NGPFrameSkip = espec->skip;

	do
	{
		int32 timetime = (uint8)TLCS900h_interpret(); // This is sooo not right, but it's replicating the old behavior(which is necessary
													  // now since I've fixed the TLCS900h core and other places not to truncate cycle counts
													  // internally to 8-bits).  Switch to the #if 0'd block of code once we fix cycle counts in the
													  // TLCS900h core(they're all sorts of messed up), and investigate if certain long
													  // instructions are interruptable(by interrupts) and later resumable, RE Rockman Battle
		// & Fighters voice sample playback.

		//if(timetime > 255)
		// printf("%d\n", timetime);

		// Note: Don't call updateTimers with a time/tick/cycle/whatever count greater than 255.
		MeowMeow |= updateTimers(&surface, timetime);

		z80_runtime += timetime;

		while (z80_runtime > 0)
		{
			int z80rantime = Z80_RunOP();

			if (z80rantime < 0) // Z80 inactive, so take up all run time!
			{
				z80_runtime = 0;
				break;
			}

			z80_runtime -= z80rantime << 1;
		}
	} while (!MeowMeow);

	espec->MasterCycles = ngpc_soundTS;
	espec->SoundBufSize = MDFNNGPCSOUND_Flush(espec->SoundBuf, espec->SoundBufMaxSize);
}

static MDFN_COLD bool Load(const uint8* romdata, int32 romlength)
{
	const uint64 fp_size = romlength;

	if (fp_size > 1024 * 1024 * 8) // 4MiB maximum ROM size, 2* to be a little tolerant of garbage.
		return false;
	//throw MDFN_Error(0, _("NGP/NGPC ROM image is too large."));

	ngpc_rom.length = fp_size;
	ngpc_rom.data = new uint8[ngpc_rom.length];
	memcpy(ngpc_rom.data, romdata, romlength);

	rom_loaded();
	//if (!FLASH_LoadNV())
	//	return false;

	//MDFNMP_Init(1024, 1024 * 1024 * 16 / 1024);

	NGPGfx = new NGPGFX_CLASS();

	//MDFNGameInfo->fps = (uint32)((uint64)6144000 * 65536 * 256 / 515 / 198); // 3072000 * 2 * 10000 / 515 / 198

	MDFNNGPCSOUND_Init();

	//MDFNMP_AddRAM(16384, 0x4000, CPUExRAM);

	SetFRM(); // Set up fast read memory mapping

	bios_install();

	//main_timeaccum = 0;
	z80_runtime = 0;

	reset();

	MDFNNGPC_SetSoundRate(44100);
	return true;
}

/*static void DoSimpleCommand(int cmd)
{
	switch (cmd)
	{
	case MDFN_MSC_POWER:
	case MDFN_MSC_RESET:
		reset();
		break;
	}
}*/

/*static const IDIISG IDII =
	{
		{"up", "UP ↑", 0, IDIT_BUTTON, "down"},
		{"down", "DOWN ↓", 1, IDIT_BUTTON, "up"},
		{"left", "LEFT ←", 2, IDIT_BUTTON, "right"},
		{"right", "RIGHT →", 3, IDIT_BUTTON, "left"},
		{"a", "A", 5, IDIT_BUTTON_CAN_RAPID, NULL},
		{"b", "B", 6, IDIT_BUTTON_CAN_RAPID, NULL},
		{"option", "OPTION", 4, IDIT_BUTTON, NULL},
};*/

/*static const FileExtensionSpecStruct KnownExtensions[] =
	{
		{".ngp", gettext_noop("Neo Geo Pocket ROM Image")},
		{".ngc", gettext_noop("Neo Geo Pocket Color ROM Image")},
		{NULL, NULL}};
}*/
}
using namespace MDFN_IEN_NGP;



/*MDFNGI EmulatedNGP =
	{
		"ngp",
		"Neo Geo Pocket (Color)",
		KnownExtensions,
		MODPRIO_INTERNAL_HIGH,
		NULL,
		PortInfo,
		Load,
		TestMagic,
		NULL,
		NULL,
		CloseGame,

		SetLayerEnableMask,
		"Background Scroll\0Foreground Scroll\0Sprites\0",

		NULL,
		NULL,

		NULL,
		0,

		CheatInfo_Empty,

		false,
		StateAction,
		Emulate,
		NULL,
		SetInput,
		NULL,
		DoSimpleCommand,
		NULL,
		NGPSettings,
		MDFN_MASTERCLOCK_FIXED(6144000),
		0,

		false, // Multires possible?

		160,  // lcm_width
		152,  // lcm_height
		NULL, // Dummy

		160, // Nominal width
		152, // Nominal height

		160, // Framebuffer width
		152, // Framebuffer height

		2, // Number of output sound channels
};*/

int main(void)
{
	return 0;
}

EXPORT int LoadSystem(const uint8* rom, int romlength, int language)
{
	settings_language = language;
	return Load(rom, romlength);
}

EXPORT void SetLayers(int enable) // 1, 2, 4  bg,fg,sprites
{
	NGPGfx->SetLayerEnableMask(enable);
}

EXPORT void FrameAdvance(EmulateSpecStruct *espec)
{
	Emulate(espec);
}

EXPORT void HardReset()
{
	reset();
}