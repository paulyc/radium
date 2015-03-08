/* Copyright 2000 Kjetil S. Matheussen

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA. */








#include "nsmtracker.h"
#include "disk.h"
#include "disk_song_proc.h"

#include "disk_root_proc.h"



void SaveRoot(struct Root *theroot){
DC_start("ROOT");

//	DC_SSN("def_instrument",theroot->def_instrument->l.num);
	DC_SSN("curr_block",theroot->curr_block);
	DC_SSI("tempo",theroot->tempo);
	DC_SSI("lpb",theroot->lpb);
	DC_SSF("quantitize_numerator",theroot->quantitize_numerator);
        DC_SSF("quantitize_denominator",theroot->quantitize_denominator);
	DC_SSI("grid_numerator",theroot->grid_numerator);
	DC_SSI("grid_denominator",theroot->grid_denominator);
	DC_SSI("keyoct",theroot->keyoct);
	DC_SSI("min_standardvel",theroot->min_standardvel);
	DC_SSI("standardvel",theroot->standardvel);

	SaveSong(theroot->song);

DC_end();
}



/*********** Start Load song *************************/

struct Root *LoadRoot(void){
	static char *objs[1]={
		"SONG"
	};
	static char *vars[12]={
		"def_instrument",
		"curr_block",
		"tempo",
		"lpb",
		"quantitize",
                "quantitize_numerator",
                "quantitize_denominator",
                "grid_numerator",
                "grid_denominator",
		"keyoct",
		"min_standardvel",
		"standardvel"
	};
	struct Root *ret=DC_alloc(sizeof(struct Root));
	ret->scrollplayonoff=true;
        ret->min_standardvel=MAX_VELOCITY*40/100;
        ret->editonoff=true;
        ret->grid_numerator=1;
        ret->grid_denominator=1;

	GENERAL_LOAD(1,12);



obj0:
	ret->song=LoadSong();
	goto start;
var0:
	goto start;			// Don't bother with instruments yet.
var1:
	ret->curr_block=DC_LoadN();
	goto start;
var2:
	ret->tempo=DC_LoadI();
	goto start;
var3:
	ret->lpb=DC_LoadI();
	goto start;

var4:
        DC_LoadF();
	ret->quantitize_numerator = 1;
        ret->quantitize_denominator = 1;
	goto start;

var5:
	ret->quantitize_numerator = DC_LoadI();
	goto start;

var6:
        ret->quantitize_denominator = DC_LoadI();
	goto start;

var7:
        ret->grid_numerator=DC_LoadI();
        goto start;

var8:
        ret->grid_denominator=DC_LoadI();
        goto start;

var9:
	ret->keyoct=DC_LoadI();
	goto start;

var10:
	ret->min_standardvel=DC_LoadI();
	goto start;

var11:
	ret->standardvel=DC_LoadI();
	goto start;

var12:
var13:
var14:
var15:
var16:
var17:
var18:
var19:
        
obj1:
obj2:
obj3:
obj4:
obj5:
obj6:
debug("loadroot, very wrong\n");

error:
debug("loadroot, goto error\n");

end:

	return ret;
}


extern struct Root *root;

void DLoadRoot(struct Root *theroot){
	DLoadSong(theroot,theroot->song);
}


/*********************** End Load Song **************************/


