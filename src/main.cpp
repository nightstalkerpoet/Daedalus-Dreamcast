#include <kos.h>
#include <conio/conio.h>
#include "main.h"
#include "rdp.h"
#include "rom.h"
#include "sr.h"
#include "memory.h"
#include "dbgconsole.h"

void outputMessage( MSG_TYPES type, char *format, ... )
{
    char buffer[1024];
    
    va_list va;
    va_start( va, format );

	vsprintf( buffer, format, va );
	strcat( buffer, "\r\n");
    printf( buffer );
    
    va_end( va );
}

int min( int a, int b )
{
    if( a < b )
        return a;
    else
        return b;
}
extern bool g_bCPURunning;
BOOL StartCPUThread(LPSTR szReason, LONG nLen);
KOS_INIT_FLAGS( INIT_DEFAULT | INIT_MALLOCSTATS );
int main( int argc, char *argv[] )
{
    // initialize debug output (serial cable)
	dbgio_init();
	irq_enable();
    // set video mode
	vid_init(DM_GENERIC_FIRST, PM_RGB565);
    pvr_init_defaults();
	
	vmu();

	// initialize emulator
	if (!Memory_Init()) return 0;
    
    if (FAILED(SR_Init(30000)))
		return 1;
		
    if( FAILED(RDPInit()) )
        return -1;

	// Don't care about failures
	DBGConsole_Enable(g_bShowDebug);

	// list CD contents
	uint32 hnd = fs_open("/cd", O_RDONLY | O_DIR);
	if (!hnd) {
		conio_printf("Error accesing disk\n");
		return -1;
	}

	conio_init(CONIO_TTY_PVR, CONIO_INPUT_LINE);
	
	// Run until a ROM file is selected
	bool bSelected = false;
	char strFilename[256];

	// Clean up the menu
	fs_close(hnd);
	conio_shutdown();

	sprintf(strFilename, "/cd/%s", "m.z64");
	ROM_LoadFile(strFilename);
    
    // emulation finished -> tidying up
    pvr_mem_reset();
    SR_Fini();
    ROM_Unload();
	DBGConsole_Enable(FALSE);
	Memory_Fini();
    
    return 0;
}

void printFPS()
{
	// save the starting time of the function
	static uint64 u64Start = timer_ms_gettime64();

	// save the FPS
	static int iFPS = 0;

	// calculate past time
	float fDiff = (float)( timer_ms_gettime64() - u64Start ) / 1000.0f ;

	// Check if one second has passed
	if( fDiff > 1.0f )
	{
		// If yes, then spend the frames per second
		printf( "FPS: %.2f\n", (float)iFPS / fDiff );

		// reset everything
		iFPS = 0;
		u64Start = timer_ms_gettime64();
	}
	else
		iFPS++;

}
