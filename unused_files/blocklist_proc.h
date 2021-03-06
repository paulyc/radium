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

extern LANGSPEC void update_seqtrack_timing(struct SeqTrack *seqtrack);
extern LANGSPEC void update_all_seqtrack_timing(void);

extern LANGSPEC void BL_init(void);
extern LANGSPEC int *BL_copy(void);
extern LANGSPEC void BL_paste(int *playlist);
extern LANGSPEC void BL_paste2(struct Song *song, int *playlist);
extern LANGSPEC void BL_insert(int pos,struct Blocks *block);
extern LANGSPEC void BL_insertCurrPos(int pos,struct Blocks *block);
extern LANGSPEC void BL_delete(int pos);
extern LANGSPEC void BL_deleteCurrPos(int pos);
extern LANGSPEC struct Blocks *BL_GetBlockFromPos(int pos);
extern LANGSPEC void BL_removeBlockFromPlaylist(struct Blocks *block);

extern LANGSPEC void BL_setLength(int length);
extern LANGSPEC void BL_setBlock(int pos, struct Blocks *block);

extern LANGSPEC void BL_moveDown(int pos);
extern LANGSPEC void BL_moveUp(int pos);

