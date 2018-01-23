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


// Stuff to handle controllers

#include "main.h"
#include "windef.h"


#include "Controller.h"
#include "CPU.h"
#include "DBGConsole.h"
#include "Debug.h"
#include "Memory.h"
#include "ROM.h"

#include "ultra_os.h"


#define ANALOGUE_SENS 80

#define CONT_GET_STATUS      0x00
#define CONT_READ_CONTROLLER 0x01
#define CONT_READ_MEMPACK    0x02
#define CONT_WRITE_MEMPACK   0x03
#define CONT_READ_EEPROM     0x04
#define CONT_WRITE_EEPROM    0x05
#define CONT_RESET           0xff

static OSContPad g_Pads[4];

BOOL g_bControllerPresent[4] = { TRUE, TRUE, TRUE, TRUE };
BOOL g_bMemPackPresent[4] = {TRUE, FALSE, FALSE, FALSE}; //{ TRUE, TRUE, TRUE, TRUE };

static void Cont_Eeprom(u8 * pbPIRAM);
static void Cont_LoadEeprom();
static void Cont_SaveEeprom();
static void Cont_LoadMempack();
static void Cont_SaveMempack();
static DWORD Cont_StatusEeprom(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD ucWrite, DWORD ucRead);

static DWORD Cont_ReadEeprom(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD ucWrite, DWORD ucRead);
static DWORD Cont_WriteEeprom(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD ucWrite, DWORD ucRead);
static DWORD Cont_ReadMemPack(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD iChannel, DWORD ucWrite, DWORD ucRead);
static DWORD Cont_WriteMemPack(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD iChannel, DWORD ucWrite, DWORD ucRead);



static DWORD Cont_Command(u8 * pbPIRAM, DWORD i, DWORD iError, u8 ucCmd, DWORD iChannel, DWORD ucWrite, DWORD ucRead);
static void Cont_InitMemPack();
static u8 Cont_DataCrc(u8 * pBuf);
static void Cont_IDCheckSum(WORD * pBuf, WORD * pPos, WORD * pNot);


static u8 * g_pEepromData = NULL;
DWORD g_dwEepromSize = 2048;

static u8 g_MemPack[4][(0x400+1) * 32];



static BOOL s_bEepromUsed = FALSE;
static BOOL s_bMempackUsed = FALSE;

HRESULT Controller_Initialise()
{
	s_bEepromUsed = FALSE;
	s_bMempackUsed = FALSE;

	g_dwEepromSize = g_ROM.dwEepromSize;
	g_pEepromData = new u8[g_dwEepromSize];
	if (g_pEepromData == NULL)
		return E_OUTOFMEMORY;

	Cont_InitMemPack();

	return S_OK;
}

void Controller_Finalise()
{
	if (s_bEepromUsed)
	{
		// Write Eeprom to rom directory
		Cont_SaveEeprom();

	}
	if (s_bMempackUsed)
	{
		// Write Eeprom to rom directory
		Cont_SaveMempack();
	}

	if (g_pEepromData != NULL)
	{
		delete [] g_pEepromData;
		g_pEepromData = NULL;
	}
}

inline DWORD SwapEndian(DWORD x)
{
	return ((x >> 24)&0x000000FF) |
		   ((x >> 8 )&0x0000FF00) |
		   ((x << 8 )&0x00FF0000) |
		   ((x << 24)&0xFF000000);
}

char *fs_full_vmu_name(const char *strNameVMU, const char *strFileName)
{
    // Dateiname zusammensetzen und Datei öffnen
    char *path = (char *)malloc(strlen(strNameVMU) + strlen(strFileName) + 6 + 1);
    sprintf(path, "/vmu/%s/%s", strNameVMU, strFileName);
    return path;
}

// Öffnet eine VMU-Datei entweder zum Lesen oder zum Schreiben
file_t fs_open_vmu(const char *strNameVMU, const char *strFileName, int mode)
{
    // Vollen Pfadnamen ermitteln
    char *path = fs_full_vmu_name(strNameVMU, strFileName);
            
    // Ordnerinhalt der VMU öffnen
    char vmuPath[7 + 1];
    sprintf(vmuPath, "/vmu/%s", strNameVMU);
	file_t d = fs_open(vmuPath, O_RDONLY | O_DIR);

    // Falls der Ordnerinhalt der VMU nicht geöffnet werden kann, dann existiert auch die Datei nicht
	if(!d)
        return 0;
        
    // Alle Dateien im Ordner prüfen, ob der Dateiname mit der gesuchten Datei übereinstimmt
    dirent_t *de;
	while((de = fs_readdir(d)))
    {
        // Überprüfen, ob die Datei gefunden wurde
        if(stricmp(de->name, strFileName) == 0)
        {
            // Datei gefunden, Ordner schließen
            fs_close(d);
            
            // Beim Schreiben die aktuelle Datei zuerst löschen
            if((mode & O_MODE_MASK) == O_WRONLY)
                fs_unlink(path);
            
            // Datei öffnen und Status zurückgeben
            file_t f = fs_open(path, mode);
            free(path);
            return f;
        }
    }

    // Datei nicht gefunden
    //    -zum Speichern bedeutet dies, dass die alte Datei nicht gelöscht werden muss Beim Schreiben die aktuelle Datei zuerst löschen
    //    -zum Lesen bedeutet dies, dass die Datei nicht geöffnet werden konnte
    if((mode & O_MODE_MASK) == O_WRONLY)
    {
        file_t f = fs_open(path, mode);
        free(path);
        return f;
    }

    free(path);
    return 0;
}

void Cont_LoadEeprom()
{
/*    printf("EEPROM\n");
	// Paket-Datei öffnen
	file_t f = fs_open_vmu("a1", "mario.sav", O_RDONLY);
	
	// Abbrechen, falls die Datei nicht geöffnet werden kann
	if(!f)
    {
        printf("EEPROM-Datei kann nicht geöffnet werden\n");
        return;
    }
    
    // Paket einlesen
    DWORD size = fs_total(f);
    uint8 *data = (uint8 *)malloc(size);
    fs_read(f, data, size);
    fs_close(f);

    // Paket dekompilieren
    vmu_pkg_t pkg;
    vmu_pkg_parse(data, &pkg);
    
    // Stimmt die gespeicherte Größe nicht mit der wirklichen Größe überein, dann abbrechen
    if(pkg.data_len != g_dwEepromSize)
    {
        free(data);
        printf("Falsche Größenangabe in EEPROM-Datei (%d - %d)\n", pkg.data_len, g_dwEepromSize);
        return;
    }
    printf("%d\n", g_dwEepromSize);
    printf("%s\n", pkg.desc_short);
    printf("%s\n", pkg.desc_long);
    
	// EEPROM aus Paket einlesen
	for(int i = 0; i < g_dwEepromSize; i++)
        g_pEepromData[i] = pkg.data[i^0x3];


    // EEPROM schreiben
    dbgio_write_buffer(g_pEepromData, g_dwEepromSize);

    // Daten wieder freigeben
    free(data);*/
}


void Cont_SaveEeprom()
{
    // Dateiname zum Speichern ermitteln
 /*  	TCHAR szEepromFileName[MAX_PATH+1];
    sprintf(szEepromFileName, "/vmu/a1/%s.sav", g_ROM.szFileName);
	printf("Saving eeprom to %s\n", szEepromFileName);
	
	// Paket-Datei öffnen
	fs_unlink(szEepromFileName);
	file_t f = fs_open(szEepromFileName, O_WRONLY);
	
	// Abbrechen, falls die Datei nicht geöffnet werden kann
	if(!f)
    {
        printf("EEPROM-Datei kann nicht geöffnet werden\n");
        return;
    }
    
    // Paket für das SaveGame erstellen (damit es vom DreamCast-Explorer verwaltet werden kann)
	vmu_pkg_t pkg;
	strcpy(pkg.desc_short, g_ROM.szBootName);
	sprintf(pkg.desc_long, "%s EEPROM", g_ROM.szGameName);
	strcpy(pkg.app_id, "DaedalusDC");
	pkg.icon_cnt = 0;
	pkg.icon_anim_speed = 0;
	pkg.eyecatch_type = VMUPKG_EC_NONE;
	pkg.data_len = g_dwEepromSize;

    // Paket mit Daten füllen
    uint8 *data = (uint8 *)malloc(g_dwEepromSize);
    pkg.data = data;
	for( int i = 0; i < g_dwEepromSize; i++ )
		data[i] = g_pEepromData[i^0x3];
    
    // Paket kompilieren
    int pkg_size;
    uint8 *pkg_out;
    vmu_pkg_build(&pkg, &pkg_out, &pkg_size);
    
    // Paket in die Datei schreiben 
	fs_write(f, pkg_out, pkg_size);
	fs_close(f);
	
	// Reservierten Speicher wieder freigeben
	free(pkg_out);
	free(data);*/
}

void Cont_SaveMempack()
{
	TCHAR szMempackFileName[MAX_PATH+1];
	FILE * fp;
	
	/*
	Dump_GetSaveDirectory(szMempackFileName, g_ROM.szFileName, TEXT(".mpk"));
	*/

	DBGConsole_Msg(0, "Saving mempack to [C%s]", szMempackFileName);

	fp = fopen(szMempackFileName, "wb");
	if (fp != NULL)
	{
		// Don't do last 32 bytes (like nemu)
		fwrite(&g_MemPack[0][0], (0x400) * 32, 1, fp);
		fclose(fp);
	}
}

void Cont_LoadMempack()
{
	TCHAR szMempackFileName[MAX_PATH+1];
	FILE * fp;

    /*
	Dump_GetSaveDirectory(szMempackFileName, g_ROM.szFileName, TEXT(".mpk"));
	*/

	DBGConsole_Msg(0, "Loading mempack from [C%s]", szMempackFileName);

	fp = fopen(szMempackFileName, "rb");
	if (fp != NULL)
	{
		// Don't do last 32 bytes (like nemu)
		fread(&g_MemPack[0][0], (0x400) * 32, 1, fp);
		fclose(fp);
	}
}

HRESULT Input_GetState( OSContPad pPad[4] )
{
    // Speichert den aktuellen Kontroller
    int iCont = 0;
    
    // Eingaben für jeden Kontroller zurücksetzen
    for( int cont = 0; cont < 4; cont++ )
	{
		pPad[cont].button = 0;
		pPad[cont].stick_x = 0;
		pPad[cont].stick_y = 0;
	}
    
    // Für jeden angeschlossenen Kontroller ausführen
	MAPLE_FOREACH_BEGIN( MAPLE_FUNC_CONTROLLER, cont_state_t, st )
        // Abbrechen, falls am ersten Kontroller Start, Trigger links und Trigger rechts gedrückt sind
		if( iCont == 0 && st->buttons & CONT_START && st->ltrig > 250 && st->rtrig > 250 )
		    g_bCPURunning = false;

        // Achsen an den Emu weiterreichen (Wertebereich -128 bis 128 in Wertebereich -80 bis 80 umwandeln
        pPad[iCont].stick_x = (int)((float)st->joyx * 0.625f);
        pPad[iCont].stick_y = (int)((float)-st->joyy * 0.625f);
        
        // Buttons an den Emu weiterreichen
        if( st->buttons & CONT_START )
            pPad[iCont].button |= START_BUTTON;
        if( st->buttons & CONT_A )
            pPad[iCont].button |= A_BUTTON;
        if( st->buttons & CONT_B )
            pPad[iCont].button |= B_BUTTON;

        // Trigger an den Emu weiterreichen
        if( st->buttons & CONT_X )
            pPad[iCont].button |= Z_TRIG;
        if( st->rtrig > 16 )
            pPad[iCont].button |= R_TRIG;
        if( st->ltrig > 16 )
            pPad[iCont].button |= L_TRIG;
            
        // Steuerkreuz an den Emu weiterreichen
        if( st->buttons & CONT_DPAD_UP )
            pPad[iCont].button |= U_JPAD;
        if( st->buttons & CONT_DPAD_DOWN )
            pPad[iCont].button |= D_JPAD;
        if( st->buttons & CONT_DPAD_LEFT )
            pPad[iCont].button |= L_JPAD;
        if( st->buttons & CONT_DPAD_RIGHT )
            pPad[iCont].button |= R_JPAD;


		// Mit nächstem Kontroller fortfahren
		iCont++;
	MAPLE_FOREACH_END()
	
    return S_OK;
}



/*
MarioKart: GetStatus (controller)
0xff010300 : 0xffffffff  |  0xff010300 : 0x050001ff
0xff010300 : 0xffffffff  |  0xff018300 : 0xffffffff
0xff010300 : 0xffffffff  |  0xff018300 : 0xffffffff
0xff010300 : 0xffffffff  |  0xff018300 : 0xffffffff
0xfe000000 : 0x00000000  |  0xfe000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000

Mario: EepGetStatus:
0x00000000 : 0xff010300
0xffffffff : 0xfe000000
0x00000000 : 0x00000000
0x00000000 : 0x00000000
0x00000000 : 0x00000000
0x00000000 : 0x00000000
0x00000000 : 0x00000000
0x00000000 : 0x00000001


Mario: ReadEeprom
0x00000000 : 0x02080400
0x00000000 : 0x00000000
0xfe0000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x00000001

Mario: WriteEeprom
0x00000000 : 0x0a01050e
0x00000000 : 0x00000000
0xfe0000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x000000ff
0x000000ff : 0x00000001

Wetrix
Channel: 0
Controller: Command is WRITE_MEMPACK
WriteMemPack: Channel 0, i is 4
Controller: Writing block 0x0400 (crc: 0x01)
Controller: data crc is 0x00
Controller: Code 0xfe - Finished
0xff230103 : 0x80010000  |  0xff230103 : 0x80010000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x0000ff00  |  0x00000000 : 0x0000ff00
0xfe000000 : 0x00000000  |  0xfe000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000


Channel: 0
Controller: Command is READ_MEMPACK
ReadMemPack: Channel 0, i is 4
Controller: Reading from block 0x0003 (crc: 0x0a)
Controller: data crc is 0x04
Returning, setting i to 40
Controller: Executing GET_STATUS
Before | After:
0xff032102 : 0x006affff  |  0xff032102 : 0x006a0000
0xffffffff : 0xffffffff  |  0x00000000 : 0x00000000
0xffffffff : 0xffffffff  |  0x00000000 : 0x00000000
0xffffffff : 0xffffffff  |  0x00000000 : 0x00000000
0xffffffff : 0xffffff88  |  0x00000000 : 0xfff2fb88
0xfe000000 : 0x00000000  |  0x4e000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000

Zelda
Channel: 0
0xff230103 : 0x8001fefe  |  0xff230103 : 0x8001fefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefffe  |  0xfefefefe : 0xfefe1efe
0xfefffe00 : 0x00000000  |  0xfefffe00 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000
Channel: 1
0x00ff2301 : 0x038001fe  |  0x00ff2381 : 0x038001fe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefeff  |  0xfefefefe : 0xfefefeff
0xfefffe00 : 0x00000000  |  0xfefffe00 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000
Channel: 2
0x0000ff23 : 0x01038001  |  0x0000ff23 : 0x81038001
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfffefe00 : 0x00000000  |  0xfffefe00 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000
Channel: 3
0x000000ff : 0x23010380  |  0x000000ff : 0x23810380
0x01fefefe : 0xfefefefe  |  0x01fefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefffe00 : 0x00000000  |  0xfefffe00 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000


Snowboard Kids
Channel: 0
0xff230103 : 0x8001fefe  |  0xff230103 : 0x8001fefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefeff00  |  0xfefefefe : 0xfefe1e00
0xfe000000 : 0x00000000  |  0xfe000000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000
Channel: 1
0x00ff2301 : 0x038001fe  |  0x00ff2381 : 0x038001fe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefeff  |  0xfefefefe : 0xfefefeff
0x01fe0000 : 0x00000000  |  0x01fe0000 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000
Channel: 2
0x0000ff23 : 0x01038001  |  0x0000ff23 : 0x81038001
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xff01fe00 : 0x00000000  |  0xff01fe00 : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000
Channel: 3
0x000000ff : 0x23010380  |  0x000000ff : 0x23810380
0x01fefefe : 0xfefefefe  |  0x01fefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfeff01fe : 0x00000000  |  0xfeff01fe : 0x00000000
0x00000000 : 0x00000000  |  0x00000000 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000





Zelda: ReadMemPack 
0xff032102 : 0x8001fefe  |  0xff032102 : 0x8001fefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefffe  |  0xfefefefe : 0xfefee1fe
0xfee1fe00 : 0x00050001  |  0xfee1fe00 : 0x00050001
0x0000ff00 : 0x00000000  |  0x0000ff00 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000

Zelda: ReadMemPack | GetStatus
0x00ff0321 : 0x028001fe  |  0x00ff0321 : 0x028001fe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefeff  |  0xfefefefe : 0xfefefee1
0xfee14e00 : 0x00050001  |  0xfee14e00 : 0x00050001
0x0000ff00 : 0x00000000  |  0x0000ff00 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000

Zelda: ReadMemPack | ReadController
0x0000ff03 : 0x21028001  |  0x0000ff03 : 0x21028001
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfffe4e00 : 0x00050001  |  0xe1fe4e00 : 0x00050001
0x0000ff00 : 0x00000000  |  0x0000ff00 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000

Zelda: ReadMemPack | ReadController
0x000000ff : 0x03210280  |  0x000000ff : 0x03210280
0x01fefefe : 0xfefefefe  |  0x01fefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefefefe : 0xfefefefe  |  0xfefefefe : 0xfefefefe
0xfefffe00 : 0x00050001  |  0xfee1fe00 : 0x00050001
0x0000ff00 : 0x00000000  |  0x0000ff00 : 0x00000000
0x00000000 : 0x00000001  |  0x00000000 : 0x00000000




*/

void Controller_Check(void)
{
	u8 * pbPIRAM = (u8 *)g_pMemoryBuffers[MEM_PI_RAM];
	BOOL bDone;
	DWORD i;
	DWORD iError;
	DWORD iChannel;
	HRESULT hr;


#ifdef DAEDALUS_LOG
	u8 before[64];
	memcpy(before, pbPIRAM, 64);
	DWORD x;
	DPF(DEBUG_MEMORY_PIF, "");
	DPF(DEBUG_MEMORY_PIF, "");
	DPF(DEBUG_MEMORY_PIF, "*********************************************");
	DPF(DEBUG_MEMORY_PIF, "**                                         **");
#endif
	
	iChannel = 0;
	while (iChannel < 6 && pbPIRAM[iChannel ^ 0x3] == 0)
	{
		iChannel++;
	}	
	if (pbPIRAM[iChannel ^ 0x3] != 0xff && iChannel < 4)
	{
		//DPF(DEBUG_MEMORY_PIF, "Channel Unk");
		//DBGConsole_Msg(0, "Unknown Channel/Command");
	}

	DPF(DEBUG_MEMORY_PIF, "Channel: %d", iChannel);

	// Clear to indicate success - we might set this again in the handler code
	pbPIRAM[63 ^ 0x3] = 0x00;

    hr = Input_GetState( g_Pads );
	if (SUCCEEDED(hr))
	{
		// Modify controller values:
		//g_Pads[0] = pad;
	}


	i = iChannel;
	bDone = FALSE;
	do
	{
		u8 ucCode;
		u8 ucWrite;
		u8 ucRead;
		u8 ucCmd;
		ucCode = pbPIRAM[(i + 0) ^ 0x3];

		switch (ucCode)
		{
		// Fill/Padding?
		case 0x00: case 0xFF:
			//dwPrevious = ucCode;
			i++;
			break;

		// Done
		case 0xFE:
			bDone = TRUE;
			DPF(DEBUG_MEMORY_PIF, "Controller: Code 0x%02x - Finished", ucCode);
			break;

		default:
			// Assume this is the Write value - code is 0x00, read is next value
			ucWrite = ucCode;
			ucRead = pbPIRAM[(i + 1) ^ 0x3];

			// Set up error pointer and skip read/write bytes of input
			iError = i + 1;
			i += 2;

			if (ucWrite < 1)
			{
				DBGConsole_Msg(0, "Controller: 0 bytes of write input - no command!");
				bDone = TRUE;
			}
			else
			{
				// Read command
				ucCmd = pbPIRAM[(i + 0) ^ 0x3];
				i++;
				ucWrite--;
				
				//DPF(DEBUG_MEMORY_PIF, "Controller: Code 0x%02x, Write 0x%02x, Read 0x%02x", ucCode, ucWrite, ucRead);
				
				i = Cont_Command(pbPIRAM, i, iError, ucCmd, iChannel, ucWrite, ucRead);
			}
			break;
		}

	} while (i < 64 && !bDone);

//finish:
#ifdef DAEDALUS_LOG
	DPF(DEBUG_MEMORY_PIF, "Before | After:");

	for (x = 0; x < 64; x+=8)
	{
		DPF(DEBUG_MEMORY_PIF, "0x%02x%02x%02x%02x : 0x%02x%02x%02x%02x  |  0x%02x%02x%02x%02x : 0x%02x%02x%02x%02x",
			before[(x + 0) ^ 0x3], before[(x + 1) ^ 0x3], before[(x + 2) ^ 0x3], before[(x + 3) ^ 0x3],
			before[(x + 4) ^ 0x3], before[(x + 5) ^ 0x3], before[(x + 6) ^ 0x3], before[(x + 7) ^ 0x3],
			pbPIRAM[(x + 0) ^ 0x3], pbPIRAM[(x + 1) ^ 0x3], pbPIRAM[(x + 2) ^ 0x3], pbPIRAM[(x + 3) ^ 0x3],
			pbPIRAM[(x + 4) ^ 0x3], pbPIRAM[(x + 5) ^ 0x3], pbPIRAM[(x + 6) ^ 0x3], pbPIRAM[(x + 7) ^ 0x3]);
	}
	DPF(DEBUG_MEMORY_PIF, "");
	DPF(DEBUG_MEMORY_PIF, "");
	DPF(DEBUG_MEMORY_PIF, "**                                         **");
	DPF(DEBUG_MEMORY_PIF, "*********************************************");
#endif

}

static void Cont_PIRAMWrite4BitsHi(u8 * pbPIRAM, DWORD i, u8 val)
{
	pbPIRAM[(i + 0) ^ 0x3] &= 0x0F;
	pbPIRAM[(i + 0) ^ 0x3] |= (val<<4);
}

static void Cont_PIRAMWrite8Bits(u8 * pbPIRAM, DWORD i, u8 val)
{
	pbPIRAM[(i + 0) ^ 0x3] = val;
}

static void Cont_PIRAMWrite16Bits(u8 * pbPIRAM, DWORD i, u16 val)
{
	pbPIRAM[(i + 0) ^ 0x3] = (u8)(val   );	// Lo
	pbPIRAM[(i + 1) ^ 0x3] = (u8)(val>>8);	// Hi
}
static void Cont_PIRAMWrite16Bits_Swapped(u8 * pbPIRAM, DWORD i, u16 val)
{
	pbPIRAM[(i + 0) ^ 0x3] = (u8)(val>>8);	// Hi
	pbPIRAM[(i + 1) ^ 0x3] = (u8)(val   );	// Lo
}


// i points to start of command
DWORD Cont_Command(u8 * pbPIRAM, DWORD i, DWORD iError, u8 ucCmd, DWORD iChannel, DWORD ucWrite, DWORD ucRead)
{
	DWORD dwController;
	DWORD dwRetVal;

	// Figure this out from the current offset...hack
	dwController = i/8;

	// i Currently points to data to write to

	switch (ucCmd)
	{
	case CONT_GET_STATUS:		// Status
		if (iChannel == 0)
		{
			DPF(DEBUG_MEMORY_PIF, "Controller: Executing GET_STATUS");
			// This is controller status
			if (g_bControllerPresent[dwController])
			{
				if (ucRead > 3)
				{
					// Transfer error...
					Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
				}
				else
				{

					Cont_PIRAMWrite16Bits(pbPIRAM, i, CONT_ABSOLUTE|CONT_JOYPORT);

					if (g_bMemPackPresent[dwController])
						Cont_PIRAMWrite8Bits(pbPIRAM, i+2, CONT_CARD_ON);	// Is the mempack plugged in?
					else
						Cont_PIRAMWrite8Bits(pbPIRAM, i+2, CONT_CARD_PULL);	// Is the mempack plugged in?
				}
				
			} else {
				// Not connected
				Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
			}
			i += ucWrite + ucRead;	
		}
		else if (iChannel == 4)
		{
		   // This is eeprom status?
			DPF(DEBUG_MEMORY_PIF, "Controller: Executing GET_EEPROM_STATUS?");

			dwRetVal = Cont_StatusEeprom(pbPIRAM, i, iError, ucWrite, ucRead);
			if (dwRetVal == ~0)
				i = 63;
			else
				i = dwRetVal;

		}
		else
		{
			//DPF(DEBUG_MEMORY_PIF, "Controller: UnkStatus, Channel = %d", iChannel);
			//DBGConsole_Msg(0, "UnkStatus, Channel = %d", iChannel);
			i += ucWrite + ucRead;	
		}	
		break;


	case CONT_READ_CONTROLLER:		// Controller
		{
			DPF(DEBUG_MEMORY_PIF, "Controller: Executing READ_CONTROLLER");
			// This is controller status
			if (g_bControllerPresent[dwController])
			{
				if (ucRead > 4)
				{
					// Transfer error...
					Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
				}
				else
				{
					// Hack - we need to only write the number of bytes asked for!
					Cont_PIRAMWrite16Bits_Swapped(pbPIRAM, i, g_Pads[dwController].button);
					Cont_PIRAMWrite8Bits(pbPIRAM, i+2, g_Pads[dwController].stick_x);
					Cont_PIRAMWrite8Bits(pbPIRAM, i+3, g_Pads[dwController].stick_y);
				}
				
			} else {
				// Not connected			
				Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
			}
		}
		i += ucWrite + ucRead;		

		break;
	case CONT_READ_MEMPACK:
		{
			DPF(DEBUG_MEMORY_PIF, "Controller: Command is READ_MEMPACK");
			if (g_bControllerPresent[iChannel])
			{
				dwRetVal = Cont_ReadMemPack(pbPIRAM, i, iError, iChannel, ucWrite, ucRead);
				if (dwRetVal == ~0)
					i = 63;
				else
					i = dwRetVal;
			}
			else
			{
				Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
				i += ucWrite + ucRead;
			}
		}
		break;
	case CONT_WRITE_MEMPACK:
		{
			DPF(DEBUG_MEMORY_PIF, "Controller: Command is WRITE_MEMPACK");
			if (g_bControllerPresent[iChannel])
			{
				dwRetVal = Cont_WriteMemPack(pbPIRAM, i, iError, iChannel, ucWrite, ucRead);
				if (dwRetVal == ~0)
					i = 63;
				else
					i = dwRetVal;
			}
			else
			{
				Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
				i += ucWrite + ucRead;
			}
		}
		break;
		

	case CONT_READ_EEPROM:
		dwRetVal = Cont_ReadEeprom(pbPIRAM, i, iError, ucWrite, ucRead);
		if (dwRetVal == ~0)
			i = 63;
		else
			i = dwRetVal;
		break;
	case CONT_WRITE_EEPROM:
		dwRetVal = Cont_WriteEeprom(pbPIRAM, i, iError, ucWrite, ucRead);
		if (dwRetVal == ~0)
			i = 63;
		else
			i = dwRetVal;
		break;

	case CONT_RESET:

		DPF(DEBUG_MEMORY_PIF, "Controller: Command is RESET");
		i += ucWrite + ucRead;
		break;

	default:
		DBGConsole_Msg(DEBUG_MEMORY_PIF, "Controller: UnkCommand is %d", ucCmd);
		DPF(DEBUG_MEMORY_PIF, "Controller: UnkCommand is %d", ucCmd);
		DPF(DEBUG_MEMORY_PIF, "Controller: i is now %d", i);
		//pbPIRAM[iError ^ 0x3] |= ((/*CONT_OVERRUN_ERROR|*/CONT_NO_RESPONSE_ERROR) << 4);
		{
			for (DWORD j = 0; j < ucRead; j++)
			{
				if (i + j < 64)
					pbPIRAM[(i + j) ^ 0x3] = 0x00;
			}
			//CPU_Halt("Controller UnkCommand");
		}
		//pbPIRAM[63 ^ 0x3] = 0x01;
		DPF(DEBUG_MEMORY_PIF, "Controller: next byte is at %d / %d", i+ucRead, pbPIRAM[(i + ucRead) ^ 0x3]);

		i += ucWrite + ucRead;
		break;

	}

	return i;
}



// i points to start of command
DWORD Cont_StatusEeprom(u8 *pbPIRAM, DWORD i, DWORD iError, DWORD ucWrite, DWORD ucRead)
{

	DPF(DEBUG_MEMORY_PIF, "Controller: GetStatusEEPROM");

	if (ucWrite != 0 || ucRead > 4)
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
		DBGConsole_Msg(0, "GetEepromStatus Overflow");
		return ~0;
	}

	if (g_bEepromPresent)
	{
		Cont_PIRAMWrite16Bits(pbPIRAM, i, CONT_EEPROM);
		Cont_PIRAMWrite8Bits(pbPIRAM, i+2, 0x00);

		i += 3;
		ucRead -= 3;
	}
	else
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
	}

	if (ucWrite > 0 || ucRead > 0)
	{
		DBGConsole_Msg(0, "GetEepromStatus Read / Write bytes remaining");
	}

	i += ucWrite + ucRead;
	return i;

}

// Returns FALSE if we should stop 
DWORD Cont_ReadEeprom(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD ucWrite, DWORD ucRead)
{
	u8 block;

	DPF(DEBUG_MEMORY_PIF, "Controller: ReadEEPROM");

	if (!s_bEepromUsed)
	{
		Cont_LoadEeprom();
		s_bEepromUsed = TRUE;
	}

	if (ucWrite != 1 || ucRead > 8)
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
		DBGConsole_Msg(0, "ReadEeprom Overflow");
		return ~0;
	}

	// Read the block 
	block = pbPIRAM[(i + 0) ^ 0x3];
	i++;
	ucWrite--;

	// TODO limit block to g_dwEepromSize / 8
	
	if (g_bEepromPresent)
	{
		u8 j;

		j = 0;
		while (ucRead)
		{
			pbPIRAM[i ^ 0x3] = g_pEepromData[(block*8 + j) ^ 0x3];

			i++;
			j++;
			ucRead--;
		}
	}
	else
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
	}

	/*if (ucWrite > 0 || ucRead > 0)
	{
		DBGConsole_Msg(0, "ReadEeprom Read / Write bytes remaining");
	}*/

	i += ucWrite + ucRead;
	return i;
}



// Returns FALSE if we should stop 
DWORD Cont_WriteEeprom(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD ucWrite, DWORD ucRead)
{
	DWORD j;
	u8 block;

	DPF(DEBUG_MEMORY_PIF, "Controller: WriteEEPROM");

	s_bEepromUsed = TRUE;

	// 9 bytes of input remaining - 8 bytes data + block
	if (ucWrite != 9 /*|| ucRead != 1*/)
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
		DBGConsole_Msg(0, "WriteEeprom Overflow");
		return ~0;
	}

	// Read the block 
	block = pbPIRAM[(i + 0) ^ 0x3];
	i++;
	ucWrite--;

	// TODO limit block to g_dwEepromSize / 8

	if (g_bEepromPresent)
	{
		j = 0;
		while (ucWrite)
		{
			g_pEepromData[(block*8 + j) ^ 0x3] = pbPIRAM[i ^ 0x3];

			i++;
			j++;
			ucWrite--;

		}
	}
	else
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_NO_RESPONSE_ERROR);
	}

	/*if (ucWrite > 0 || ucRead > 0)
	{
		DBGConsole_Msg(0, "WriteEeprom Read / Write bytes remaining");
	}*/

	i += ucWrite + ucRead;
	return i;
}


// Returns new position to continue reading
// i is the address of the first write info (after command itself)
DWORD Cont_ReadMemPack(u8 * pbPIRAM, DWORD i, DWORD iError, DWORD iChannel, DWORD ucWrite, DWORD ucRead)
{
	DWORD j;
	DWORD dwAddressCrc;
	DWORD dwAddress;
	DWORD dwCRC;
	u8 ucDataCRC;
	u8 * pBuf;

	if (!s_bMempackUsed)
	{
		Cont_LoadMempack();
		s_bMempackUsed = TRUE;
	}
	
	// There must be exactly 2 bytes to write, and 33 bytes to read
	if (ucWrite != 2 || ucRead != 33)
	{
		// TRANSFER ERROR!!!!
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
		return ~0;
	}

	DPF(DEBUG_MEMORY_PIF, "ReadMemPack: Channel %d, i is %d", iChannel, i);

	// Get address..
	dwAddressCrc = (pbPIRAM[(i + 0) ^ 0x3]<< 8) |
				   (pbPIRAM[(i + 1) ^ 0x3]);

	dwAddress = (dwAddressCrc >> 5);
	dwCRC = (dwAddressCrc & 0x1f);
	i += 2;	
	ucWrite -= 2;

	if (dwAddress > 0x400)
	{
		//DBGConsole_Msg(0, "Attempting to read from non-existing block 0x%08x", dwAddress);
		// SOME OTHER ERROR!
		DBGConsole_Msg(0, "ReadMemPack: Address out of range: 0x%08x", dwAddress);
		return ~0;
	}

	pBuf = &g_MemPack[iChannel][dwAddress * 32];


	DPF(DEBUG_MEMORY_PIF, "Controller: Reading from block 0x%04x (crc: 0x%02x)", dwAddress, dwCRC);
	
	for (j = 0; j < 32; j++)
	{
		if (i < 64)
		{
			// Here we would really read from the "mempack"
			pbPIRAM[i ^ 0x3] = pBuf[j];
		}
		i++;
		ucRead--;
	}

	// We would really generate a CRC on the mempack data
	ucDataCRC = Cont_DataCrc(pBuf);
	
	DPF(DEBUG_MEMORY_PIF, "Controller: data crc is 0x%02x", ucDataCRC);

	// Write the crc value:
	pbPIRAM[i ^ 0x3] = ucDataCRC;
	i++;
	ucRead--;
	
	DPF(DEBUG_MEMORY_PIF, "Returning, setting i to %d", i + 1);

	// With wetrix, there is still a padding byte?
	if (ucWrite > 0 || ucRead > 0)
	{
		DBGConsole_Msg(0, "ReadMemPack / Write bytes remaining");
	}


	return i;
}


// Returns new position to continue reading
// i is the address of the first write info (after command itself)
DWORD Cont_WriteMemPack(u8 * pbPIRAM, DWORD i, DWORD iError,  DWORD iChannel, DWORD ucWrite, DWORD ucRead)
{
	DWORD j;
	DWORD dwAddressCrc;
	DWORD dwAddress;
	DWORD dwCRC;
	u8 ucDataCRC;
	u8 * pBuf;

	if (!s_bMempackUsed)
	{
		Cont_LoadMempack();
		s_bMempackUsed = TRUE;
	}
	
	// There must be exactly 32+2 bytes to read

	if (ucWrite != 34 || ucRead != 1)
	{
		Cont_PIRAMWrite4BitsHi(pbPIRAM, iError, CONT_OVERRUN_ERROR);
		return ~0;
	}

	DPF(DEBUG_MEMORY_PIF, "WriteMemPack: Channel %d, i is %d", iChannel, i);

	// Get address..
	dwAddressCrc = (pbPIRAM[(i + 0) ^ 0x3]<< 8) |
		           (pbPIRAM[(i + 1) ^ 0x3]);

	dwAddress = (dwAddressCrc >>5);
	dwCRC = (dwAddressCrc & 0x1f);
	i += 2;	
	ucWrite -= 2;
	
	if (dwAddress > 0x400)
	{
		// Starfox does this
		//DBGConsole_Msg(0, "Attempting to write to non-existing block 0x%08x", dwAddress);
		return ~0;
	}

	pBuf = &g_MemPack[iChannel][dwAddress * 32];

	DPF(DEBUG_MEMORY_PIF, "Controller: Writing block 0x%04x (crc: 0x%02x)", dwAddress, dwCRC);
	

	for (j = 0; j < 32; j++)
	{
		if (i < 64)
		{
			// Here we would really write to the "mempack"
			pBuf[j] = pbPIRAM[i ^ 0x3];
		}
		i++;
		ucWrite--;
	}
	
	// We would really generate a CRC on the mempack data
	ucDataCRC = Cont_DataCrc(pBuf);
	
	DPF(DEBUG_MEMORY_PIF, "Controller: data crc is 0x%02x", ucDataCRC);

	// Write the crc value:
	pbPIRAM[i ^ 0x3] = ucDataCRC;
	i++;
	ucRead--;
	
	// With wetrix, there is still a padding byte?
	if (ucWrite > 0 || ucRead > 0)
	{
		DBGConsole_Msg(0, "WriteMemPack / Write bytes remaining");
	}
	return i;
}

u8 Cont_DataCrc(u8 * pBuf)
{
	u8 c;
	u8 x,s;
	u8 i;
	s8 z;

	c = 0;
	for (i = 0; i < 33; i++)
	{
		s = pBuf[i];

		for (z = 7; z >= 0; z--)
		{		
			if (c & 0x80)
				x = 0x85;
			else
				x = 0;

			c <<= 1;

			if (i < 32)
			{
				if (s & (1<<z))
					c |= 1;
			}

			c = c ^ x;
		}
	}

	return c;
}

void Cont_IDCheckSum(WORD * pBuf, WORD * pPos, WORD * pNot)
{
	WORD wPos = 0;
	WORD wNot = 0;

	for (DWORD i = 0; i < 14; i++)
	{
		wPos += pBuf[i];
		wNot += (~pBuf[i]);
	}

	*pPos = wPos;
	*pNot = wNot;
}

void Cont_InitMemPack()
{

	DWORD dwAddress;
	DWORD iChannel;

	for (iChannel = 0; iChannel < 4; iChannel++)
	{
		for (dwAddress = 0; dwAddress < 0x0400; dwAddress++)
		{
			u8 * pBuf = &g_MemPack[iChannel][dwAddress * 32];

			// Clear values
			memset(pBuf, 0, 32);

			// Generate checksum if necessary
			if (dwAddress == 3 || dwAddress == 4 || dwAddress == 6)
			{
				WORD wPos, wNot;
				WORD * pwBuf = (WORD *)pBuf;

				Cont_IDCheckSum(pwBuf, &wPos, &wNot);

				pwBuf[14] = (wPos >> 8) | (wPos << 8);
				pwBuf[15] = (wNot >> 8) | (wNot << 8);

				DPF(DEBUG_MEMORY_PIF, "Hacking ID Values: 0x%04x, 0x%04x", wPos, wNot);	
			}
		}
	}

}



