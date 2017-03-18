/*
 * Copyright (c) 1999 Greg Haerr <greg@censoft.com>
 *
 * 8bpp (256 color) standard palette definition
 */
#include "device.h"

/*
 * Special palette for supporting 48 Windows colors and a 216 color
 * uniform color distribution.
 * Note: the first 20 colors are used internally as system colors.
 */
RGBENTRY stdpal8[256] = {
	/* 16 EGA colors, arranged for direct predefined palette indexing*/
	RGBDEF( 0  , 0  , 0   ),	/* black*/
	RGBDEF( 0  , 0  , 128 ),	/* blue*/
	RGBDEF( 0  , 128, 0   ),	/* green*/
	RGBDEF( 0  , 128, 128 ),	/* cyan*/ /* COLOR_BACKGROUND*/
	RGBDEF( 128, 0  , 0   ),	/* red*/  /* COLOR_ACTIVECAPTION A*/
	RGBDEF( 128, 0  , 128 ),	/* magenta*/ /* COLOR_ACTIVECAPTION B*/
	RGBDEF( 128, 64 , 0   ),	/* adjusted brown*/
	RGBDEF( 192, 192, 192 ),	/* ltgray*/
	RGBDEF( 128, 128, 128 ),	/* gray*/
	RGBDEF( 0  , 0  , 255 ),	/* ltblue*/
	RGBDEF( 0  , 255, 0   ),	/* ltgreen*/
	RGBDEF( 0  , 255, 255 ),	/* ltcyan*/
	RGBDEF( 255, 0  , 0   ),	/* ltred*/
	RGBDEF( 255, 0  , 255 ),	/* ltmagenta*/
	RGBDEF( 255, 255, 0   ),	/* yellow*/
	RGBDEF( 255, 255, 255 ),	/* white*/

	/* 32 basic windows colors (first 8 are most important)*/
	RGBDEF( 32 , 32 , 32  ),	/* DKGRAY_BRUSH*/
	RGBDEF( 128, 128, 0   ),	/* non-adjusted brown*/
	RGBDEF( 223, 223, 223 ),	/* COLOR_3DLIGHT B*/
	RGBDEF( 160, 160, 160 ), 	/* COLOR_3DLIGHT C*/

	RGBDEF( 234, 230, 221 ),	/* COLOR_BTNHIGHLIGHT A*/
	RGBDEF( 213, 204, 187 ),	/* COLOR_BTNFACE A*/
	RGBDEF( 162, 141, 104 ),	/* COLOR_BTNSHADOW A*/
	RGBDEF( 0  , 64 , 128 ),	/* COLOR_INACTIVECAPTION C*/
	//RGBDEF( 0  , 0  , 64  ),
	//RGBDEF( 0  , 64 , 0   ),
	//RGBDEF( 0  , 64 , 64  ),
	//RGBDEF( 0  , 128, 64  ),
	RGBDEF( 0  , 128, 255 ),
	RGBDEF( 0  , 255, 128 ),
	RGBDEF( 64 , 0  , 0   ),
	RGBDEF( 64 , 0  , 64  ),
	RGBDEF( 64 , 0  , 128 ),
	RGBDEF( 64 , 128, 128 ),
	RGBDEF( 128, 0  , 64  ),
	RGBDEF( 128, 0  , 255 ),
	RGBDEF( 128, 64 , 64  ),
	RGBDEF( 128, 128, 64  ),
	RGBDEF( 128, 128, 192 ),
	RGBDEF( 128, 128, 255 ),
	RGBDEF( 128, 255, 0   ),
	RGBDEF( 128, 255, 255 ),
	RGBDEF( 164, 200, 240 ),
	RGBDEF( 192, 220, 192 ),
	RGBDEF( 255, 0  , 128 ),
	RGBDEF( 255, 128, 0   ),
	RGBDEF( 255, 128, 192 ),
	RGBDEF( 255, 128, 255 ),
	RGBDEF( 255, 128, 128 ),
	RGBDEF( 255, 255, 128 ),
	RGBDEF( 255, 251, 240 ),
	RGBDEF( 255, 255, 232 ),

	/* 216 colors spread uniformly across rgb spectrum*/
	/* 8 colors removed that are duplicated above*/
	//RGBDEF( 0x00, 0x00, 0x00 ),		//
	RGBDEF( 0x00, 0x00, 0x33 ),
	RGBDEF( 0x00, 0x00, 0x66 ),
	RGBDEF( 0x00, 0x00, 0x99 ),
	RGBDEF( 0x00, 0x00, 0xcc ),
	//RGBDEF( 0x00, 0x00, 0xff ),		//
	RGBDEF( 0x33, 0x00, 0x00 ),
	RGBDEF( 0x33, 0x00, 0x33 ),
	RGBDEF( 0x33, 0x00, 0x66 ),
	RGBDEF( 0x33, 0x00, 0x99 ),
	RGBDEF( 0x33, 0x00, 0xcc ),
	RGBDEF( 0x33, 0x00, 0xff ),
	RGBDEF( 0x66, 0x00, 0x00 ),
	RGBDEF( 0x66, 0x00, 0x33 ),
	RGBDEF( 0x66, 0x00, 0x66 ),
	RGBDEF( 0x66, 0x00, 0x99 ),
	RGBDEF( 0x66, 0x00, 0xcc ),
	RGBDEF( 0x66, 0x00, 0xff ),
	RGBDEF( 0x99, 0x00, 0x00 ),
	RGBDEF( 0x99, 0x00, 0x33 ),
	RGBDEF( 0x99, 0x00, 0x66 ),
	RGBDEF( 0x99, 0x00, 0x99 ),
	RGBDEF( 0x99, 0x00, 0xcc ),
	RGBDEF( 0x99, 0x00, 0xff ),
	RGBDEF( 0xcc, 0x00, 0x00 ),
	RGBDEF( 0xcc, 0x00, 0x33 ),
	RGBDEF( 0xcc, 0x00, 0x66 ),
	RGBDEF( 0xcc, 0x00, 0x99 ),
	RGBDEF( 0xcc, 0x00, 0xcc ),
	RGBDEF( 0xcc, 0x00, 0xff ),
	//RGBDEF( 0xff, 0x00, 0x00 ),		//
	RGBDEF( 0xff, 0x00, 0x33 ),
	RGBDEF( 0xff, 0x00, 0x66 ),
	RGBDEF( 0xff, 0x00, 0x99 ),
	RGBDEF( 0xff, 0x00, 0xcc ),
	//RGBDEF( 0xff, 0x00, 0xff ),		//
	RGBDEF( 0x00, 0x33, 0x00 ),
	RGBDEF( 0x00, 0x33, 0x33 ),
	RGBDEF( 0x00, 0x33, 0x66 ),
	RGBDEF( 0x00, 0x33, 0x99 ),
	RGBDEF( 0x00, 0x33, 0xcc ),
	RGBDEF( 0x00, 0x33, 0xff ),
	RGBDEF( 0x33, 0x33, 0x00 ),
	RGBDEF( 0x33, 0x33, 0x33 ),
	RGBDEF( 0x33, 0x33, 0x66 ),
	RGBDEF( 0x33, 0x33, 0x99 ),
	RGBDEF( 0x33, 0x33, 0xcc ),
	RGBDEF( 0x33, 0x33, 0xff ),
	RGBDEF( 0x66, 0x33, 0x00 ),
	RGBDEF( 0x66, 0x33, 0x33 ),
	RGBDEF( 0x66, 0x33, 0x66 ),
	RGBDEF( 0x66, 0x33, 0x99 ),
	RGBDEF( 0x66, 0x33, 0xcc ),
	RGBDEF( 0x66, 0x33, 0xff ),
	RGBDEF( 0x99, 0x33, 0x00 ),
	RGBDEF( 0x99, 0x33, 0x33 ),
	RGBDEF( 0x99, 0x33, 0x66 ),
	RGBDEF( 0x99, 0x33, 0x99 ),
	RGBDEF( 0x99, 0x33, 0xcc ),
	RGBDEF( 0x99, 0x33, 0xff ),
	RGBDEF( 0xcc, 0x33, 0x00 ),
	RGBDEF( 0xcc, 0x33, 0x33 ),
	RGBDEF( 0xcc, 0x33, 0x66 ),
	RGBDEF( 0xcc, 0x33, 0x99 ),
	RGBDEF( 0xcc, 0x33, 0xcc ),
	RGBDEF( 0xcc, 0x33, 0xff ),
	RGBDEF( 0xff, 0x33, 0x00 ),
	RGBDEF( 0xff, 0x33, 0x33 ),
	RGBDEF( 0xff, 0x33, 0x66 ),
	RGBDEF( 0xff, 0x33, 0x99 ),
	RGBDEF( 0xff, 0x33, 0xcc ),
	RGBDEF( 0xff, 0x33, 0xff ),
	RGBDEF( 0x00, 0x66, 0x00 ),
	RGBDEF( 0x00, 0x66, 0x33 ),
	RGBDEF( 0x00, 0x66, 0x66 ),
	RGBDEF( 0x00, 0x66, 0x99 ),
	RGBDEF( 0x00, 0x66, 0xcc ),
	RGBDEF( 0x00, 0x66, 0xff ),
	RGBDEF( 0x33, 0x66, 0x00 ),
	RGBDEF( 0x33, 0x66, 0x33 ),
	RGBDEF( 0x33, 0x66, 0x66 ),
	RGBDEF( 0x33, 0x66, 0x99 ),
	RGBDEF( 0x33, 0x66, 0xcc ),
	RGBDEF( 0x33, 0x66, 0xff ),
	RGBDEF( 0x66, 0x66, 0x00 ),
	RGBDEF( 0x66, 0x66, 0x33 ),
	RGBDEF( 0x66, 0x66, 0x66 ),
	RGBDEF( 0x66, 0x66, 0x99 ),
	RGBDEF( 0x66, 0x66, 0xcc ),
	RGBDEF( 0x66, 0x66, 0xff ),
	RGBDEF( 0x99, 0x66, 0x00 ),
	RGBDEF( 0x99, 0x66, 0x33 ),
	RGBDEF( 0x99, 0x66, 0x66 ),
	RGBDEF( 0x99, 0x66, 0x99 ),
	RGBDEF( 0x99, 0x66, 0xcc ),
	RGBDEF( 0x99, 0x66, 0xff ),
	RGBDEF( 0xcc, 0x66, 0x00 ),
	RGBDEF( 0xcc, 0x66, 0x33 ),
	RGBDEF( 0xcc, 0x66, 0x66 ),
	RGBDEF( 0xcc, 0x66, 0x99 ),
	RGBDEF( 0xcc, 0x66, 0xcc ),
	RGBDEF( 0xcc, 0x66, 0xff ),
	RGBDEF( 0xff, 0x66, 0x00 ),
	RGBDEF( 0xff, 0x66, 0x33 ),
	RGBDEF( 0xff, 0x66, 0x66 ),
	RGBDEF( 0xff, 0x66, 0x99 ),
	RGBDEF( 0xff, 0x66, 0xcc ),
	RGBDEF( 0xff, 0x66, 0xff ),
	RGBDEF( 0x00, 0x99, 0x00 ),
	RGBDEF( 0x00, 0x99, 0x33 ),
	RGBDEF( 0x00, 0x99, 0x66 ),
	RGBDEF( 0x00, 0x99, 0x99 ),
	RGBDEF( 0x00, 0x99, 0xcc ),
	RGBDEF( 0x00, 0x99, 0xff ),
	RGBDEF( 0x33, 0x99, 0x00 ),
	RGBDEF( 0x33, 0x99, 0x33 ),
	RGBDEF( 0x33, 0x99, 0x66 ),
	RGBDEF( 0x33, 0x99, 0x99 ),
	RGBDEF( 0x33, 0x99, 0xcc ),
	RGBDEF( 0x33, 0x99, 0xff ),
	RGBDEF( 0x66, 0x99, 0x00 ),
	RGBDEF( 0x66, 0x99, 0x33 ),
	RGBDEF( 0x66, 0x99, 0x66 ),
	RGBDEF( 0x66, 0x99, 0x99 ),
	RGBDEF( 0x66, 0x99, 0xcc ),
	RGBDEF( 0x66, 0x99, 0xff ),
	RGBDEF( 0x99, 0x99, 0x00 ),
	RGBDEF( 0x99, 0x99, 0x33 ),
	RGBDEF( 0x99, 0x99, 0x66 ),
	RGBDEF( 0x99, 0x99, 0x99 ),
	RGBDEF( 0x99, 0x99, 0xcc ),
	RGBDEF( 0x99, 0x99, 0xff ),
	RGBDEF( 0xcc, 0x99, 0x00 ),
	RGBDEF( 0xcc, 0x99, 0x33 ),
	RGBDEF( 0xcc, 0x99, 0x66 ),
	RGBDEF( 0xcc, 0x99, 0x99 ),
	RGBDEF( 0xcc, 0x99, 0xcc ),
	RGBDEF( 0xcc, 0x99, 0xff ),
	RGBDEF( 0xff, 0x99, 0x00 ),
	RGBDEF( 0xff, 0x99, 0x33 ),
	RGBDEF( 0xff, 0x99, 0x66 ),
	RGBDEF( 0xff, 0x99, 0x99 ),
	RGBDEF( 0xff, 0x99, 0xcc ),
	RGBDEF( 0xff, 0x99, 0xff ),
	RGBDEF( 0x00, 0xcc, 0x00 ),
	RGBDEF( 0x00, 0xcc, 0x33 ),
	RGBDEF( 0x00, 0xcc, 0x66 ),
	RGBDEF( 0x00, 0xcc, 0x99 ),
	RGBDEF( 0x00, 0xcc, 0xcc ),
	RGBDEF( 0x00, 0xcc, 0xff ),
	RGBDEF( 0x33, 0xcc, 0x00 ),
	RGBDEF( 0x33, 0xcc, 0x33 ),
	RGBDEF( 0x33, 0xcc, 0x66 ),
	RGBDEF( 0x33, 0xcc, 0x99 ),
	RGBDEF( 0x33, 0xcc, 0xcc ),
	RGBDEF( 0x33, 0xcc, 0xff ),
	RGBDEF( 0x66, 0xcc, 0x00 ),
	RGBDEF( 0x66, 0xcc, 0x33 ),
	RGBDEF( 0x66, 0xcc, 0x66 ),
	RGBDEF( 0x66, 0xcc, 0x99 ),
	RGBDEF( 0x66, 0xcc, 0xcc ),
	RGBDEF( 0x66, 0xcc, 0xff ),
	RGBDEF( 0x99, 0xcc, 0x00 ),
	RGBDEF( 0x99, 0xcc, 0x33 ),
	RGBDEF( 0x99, 0xcc, 0x66 ),
	RGBDEF( 0x99, 0xcc, 0x99 ),
	RGBDEF( 0x99, 0xcc, 0xcc ),
	RGBDEF( 0x99, 0xcc, 0xff ),
	RGBDEF( 0xcc, 0xcc, 0x00 ),
	RGBDEF( 0xcc, 0xcc, 0x33 ),
	RGBDEF( 0xcc, 0xcc, 0x66 ),
	RGBDEF( 0xcc, 0xcc, 0x99 ),
	RGBDEF( 0xcc, 0xcc, 0xcc ),
	RGBDEF( 0xcc, 0xcc, 0xff ),
	RGBDEF( 0xff, 0xcc, 0x00 ),
	RGBDEF( 0xff, 0xcc, 0x33 ),
	RGBDEF( 0xff, 0xcc, 0x66 ),
	RGBDEF( 0xff, 0xcc, 0x99 ),
	RGBDEF( 0xff, 0xcc, 0xcc ),
	RGBDEF( 0xff, 0xcc, 0xff ),
	//RGBDEF( 0x00, 0xff, 0x00 ),		//
	RGBDEF( 0x00, 0xff, 0x33 ),
	RGBDEF( 0x00, 0xff, 0x66 ),
	RGBDEF( 0x00, 0xff, 0x99 ),
	RGBDEF( 0x00, 0xff, 0xcc ),
	//RGBDEF( 0x00, 0xff, 0xff ),		//
	RGBDEF( 0x33, 0xff, 0x00 ),
	RGBDEF( 0x33, 0xff, 0x33 ),
	RGBDEF( 0x33, 0xff, 0x66 ),
	RGBDEF( 0x33, 0xff, 0x99 ),
	RGBDEF( 0x33, 0xff, 0xcc ),
	RGBDEF( 0x33, 0xff, 0xff ),
	RGBDEF( 0x66, 0xff, 0x00 ),
	RGBDEF( 0x66, 0xff, 0x33 ),
	RGBDEF( 0x66, 0xff, 0x66 ),
	RGBDEF( 0x66, 0xff, 0x99 ),
	RGBDEF( 0x66, 0xff, 0xcc ),
	RGBDEF( 0x66, 0xff, 0xff ),
	RGBDEF( 0x99, 0xff, 0x00 ),
	RGBDEF( 0x99, 0xff, 0x33 ),
	RGBDEF( 0x99, 0xff, 0x66 ),
	RGBDEF( 0x99, 0xff, 0x99 ),
	RGBDEF( 0x99, 0xff, 0xcc ),
	RGBDEF( 0x99, 0xff, 0xff ),
	RGBDEF( 0xcc, 0xff, 0x00 ),
	RGBDEF( 0xcc, 0xff, 0x33 ),
	RGBDEF( 0xcc, 0xff, 0x66 ),
	RGBDEF( 0xcc, 0xff, 0x99 ),
	RGBDEF( 0xcc, 0xff, 0xcc ),
	RGBDEF( 0xcc, 0xff, 0xff ),
	//RGBDEF( 0xff, 0xff, 0x00 ),		//
	RGBDEF( 0xff, 0xff, 0x33 ),
	RGBDEF( 0xff, 0xff, 0x66 ),
	RGBDEF( 0xff, 0xff, 0x99 ),
	RGBDEF( 0xff, 0xff, 0xcc ),
	//RGBDEF( 0xff, 0xff, 0xff )		//
};

#if TEST
main()
{
	int	c;

	printf("%d\n", ((int)&stdpalette[1]) - (int)&stdpalette[0]);

	c = FindNearestColor(stdpalette, 224, 224, 224);
	printf("%d = %02x %02x %02x\n", c, stdpalette[c].r, stdpalette[c].g,
		stdpalette[c].b);
}
#endif
