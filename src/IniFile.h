/*
Copyright (C) 2001 CyRUS64 (http://www.boob.co.uk)

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

#ifndef __INIFILE_H__
#define __INIFILE_H__

#include "windef.h"

/*class IniFile {
public: //functions

	IniFile(char *szFileName);
	~IniFile();
		
	BOOL ReadIniFile();
	// (STRMNNRMN - Write ini back out to disk?)
	void WriteIniFile(LPCTSTR szFileName);

	// Find entry in list, create if missing
	int FindEntry(DWORD dwCRC1, DWORD dwCRC2, BYTE nCountryID, LPCTSTR szName); 


public: //variables
	bool	bChanged;			// (STRMNNRMN - Changed since read from disk?)

	struct section
	{
		bool    bOutput;			// (STRMNNRMN - Has it been written to disk yet?)
		char crccheck[50];
		char name[50];
		int ucode;
		char comment[50];
		char info[50];
		BOOL bDisableDynarec;
		BOOL bDisablePatches;
		BOOL bDisableTextureCRC;
		BOOL bDisableEeprom;
		BOOL bDisableSpeedSync;
		BOOL bIncTexRectEdge;
		BOOL bExpansionPak;
		DWORD dwEepromSize;
		DWORD dwRescanCount;		// Rescan for symbols after this # of dma xfers

	};
	std::vector<section> sections;
	//section sections[1000];
	//int sectionstotal;

private: //functions

	istream & getline( istream & is, char *str );
	void OutputSectionDetails(DWORD i, FILE * fh);

private: //variables

	char m_szFileName[300];
	char m_szSectionName[300];

};*/

#endif //__INIFILE_H__
