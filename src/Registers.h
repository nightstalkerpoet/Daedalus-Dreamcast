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

#ifndef __REGISTERS_H__
#define __REGISTERS_H__

// Register names

/*
  Main CPU registers:
  -------------------
    00h = r0/reg0     08h = t0/reg8     10h = s0/reg16    18h = t8/reg24
    01h = at/reg1     09h = t1/reg9     11h = s1/reg17    19h = t9/reg25
    02h = v0/reg2     0Ah = t2/reg10    12h = s2/reg18    1Ah = k0/reg26
    03h = v1/reg3     0Bh = t3/reg11    13h = s3/reg19    1Bh = k1/reg27
    04h = a0/reg4     0Ch = t4/reg12    14h = s4/reg20    1Ch = gp/reg28
    05h = a1/reg5     0Dh = t5/reg13    15h = s5/reg21    1Dh = sp/reg29
    06h = a2/reg6     0Eh = t6/reg14    16h = s6/reg22    1Eh = s8/reg30
    07h = a3/reg7     0Fh = t7/reg15    17h = s7/reg23    1Fh = ra/reg31
*/
/* Shared by RSP */
static char *RegNames[32] = {
	"r0", "at", "v0", "v1", "a0", "a1", "a2", "a3",
	"t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
	"s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
	"t8", "t9", "k0", "k1", "gp", "sp", "s8", "ra"
};





/*
  COP0 registers:
  ---------------
    00h = Index       08h = BadVAddr    10h = Config      18h = *RESERVED* 
    01h = Random      09h = Count       11h = LLAddr      19h = *RESERVED*
    02h = EntryLo0    0Ah = EntryHi     12h = WatchLo     1Ah = PErr
    03h = EntryLo1    0Bh = Compare     13h = WatchHi     1Bh = CacheErr
    04h = Context     0Ch = Status      14h = XContext    1Ch = TagLo
    05h = PageMask    0Dh = Cause       15h = *RESERVED*  1Dh = TagHi
    06h = Wired       0Eh = EPC         16h = *RESERVED*  1Eh = ErrorEPC
    07h = *RESERVED*  0Fh = PRevID      17h = *RESERVED*  1Fh = *RESERVED*
*/

static char *Cop0RegNames[32] = {
	"Index", "Random", "EntryLo0", "EntryLo1", "Context", "PageMask", "Wired", "*RESERVED1*",
	"BadVAddr", "Count", "EntryHi", "Compare", "Status", "Cause", "EPC", "PRid",
	"Config", "LLAddr", "WatchLo", "WatchHi", "XContext", "*RESERVED2*", "*RESERVED3*", "*RESERVED4*",
	"*RESERVED5*", "*RESERVED6*", "PErr", "CacheErr", "TagLo", "TagHi", "ErrorEPC", "*RESERVED7*"
};
static char *ShortCop0RegNames[32] = {
	"Idx", "Rnd", "ELo0", "ELo1", "Ctx", "PMsk", "Wrd", "*",
	"BadV", "Cnt", "EHi", "Cmp", "Stat", "Caus", "EPC", "PRid",
	"Cfg", "LLA", "WLo", "WHi", "XCtx", "*", "*", "*",
	"*", "*", "PErr", "CErr", "TLo", "THi", "EEPC", "*"
};

#endif
