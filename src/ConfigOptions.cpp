
/*
Copyright (C) 2001 StrmnNrmn

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/


// This file contains many variables that can be used to
// change the overall operation of the emu. They are here for
// convienience, so that they can quickly be changed when
// developing. Generally they will be changed by the ini file 
// settings.

#include "windef.h"

BOOL g_bRDPEnableGfx			= TRUE;		// Show graphics
BOOL g_bUseDynarec				= TRUE;		// Use dynamic recompilation
BOOL g_bApplyPatches			= TRUE;		// Apply os-hooks
BOOL g_bCRCCheck				= TRUE;		// Apply a crc-esque check to each texture each frame
BOOL g_bEepromPresent			= TRUE;		// Needs to be per-game
BOOL g_bSpeedSync				= FALSE;		// Synchronise speed
BOOL g_bShowDebug				= TRUE;		// Show the debug console?
BOOL g_bIncTexRectEdge			= FALSE;

BOOL g_bWarnMemoryErrors		= FALSE;	// Warn on all memory access errors?
											// If false, the exception handler is
											// taken without any warnings on the debug console
BOOL g_bTrapExceptions			= TRUE;		// If set, this causes exceptions in the audio
											// plugin, cpu thread and graphics processing to
											// be handled nicely. I turn this off for 
											// development to see when things mess up
BOOL g_bRunAutomatically		= TRUE;		// Run roms automatically after loading
BOOL g_bSkipFirstRSP			= FALSE;	// For zelda ucode

DWORD g_dwDisplayWidth			= 320;		// Display resolution
DWORD g_dwDisplayHeight			= 240;
BOOL  g_bDisplayFullscreen		= TRUE;
