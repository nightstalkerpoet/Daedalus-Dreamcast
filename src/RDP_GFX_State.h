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



// State information for the RSP/RDP 
// This class simply remembers state information as it is passed in.
// When Apply() is called, this class passes the state information on
// to the renderer which applies any changes to the D3D or OGL device.

// The reason for doing this is to eliminate any unnecessary state changes
// by the 3d hardware


class RDPGfxState
{



	// Apply any changes that have occurred
	void ApplyChanges(RDPGfxState & oldstate)
	{
		if (dwMux0 != oldstate.dwMux0 ||
			dwMux1 != oldstate.dwMux1)
		{
			
		}

		oldstate = *this;
	}

private:
	DWORD		dwMux0, dwMux1;
	DWORD		dwGeometryMode;
	DWORD		dwOtherModeL;
	DWORD		dwOtherModeH;


};
