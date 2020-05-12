/* vim: ts=4 st=4 : */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "mach_defines.h"
#include "sdk.h"
#include "gfx_load.h"
#include "cache.h"

// Set up for background
extern char _binary_GOLbak_png_start;
extern char _binary_GOLbak_png_end;
extern char _binary_tileset_png_start;
extern char _binary_tileset_png_end;

//Pointer to the framebuffer memory.
uint8_t *fbmem;

//The dimensions of the framebuffer
#define FB_WIDTH 512
#define FB_HEIGHT 320

#define for_x for (int x = 0; x < w; x++)
#define for_y for (int y = 0; y < h; y++)
#define for_xy for_x for_y

#define BUTTON_READ_DELAY		15

//Convenience macros
#define COMP_COLOR(A, R, G, B) ((((A) & 0xFF) << 24) | \
								(((B) & 0xFF) << 16) | \
								(((G) & 0xFF) <<  8) | \
								(((R) & 0xFF) <<  0))
#define FB_PIX(X, Y) fbmem[(X) + ((Y) * FB_WIDTH)]

uint32_t counter60hz(void) {
	return GFX_REG(GFX_VBLCTR_REG);
}

void create_fire_palette(void) {

	// transparent to blue (leaving the first 16 for the tileset)
	// this could be as well just black to blue, but why not. :)
	for (int i = 0; i < 16; i++) {
		GFXPAL[i+17] = COMP_COLOR(i << 2, 0, 0, i << 2);
	}

	// setting the remaining palette in one go
	for (uint32_t i = 0; i < 32; i++) {
		// blue to red
		GFXPAL[i +  32] = COMP_COLOR(0xFF, i << 3, 0, 64 - (i << 1));
		// red to yellow
		GFXPAL[i +  64] = COMP_COLOR(0xFF, 0xFF, i << 3, 0);
		// yellow to white
		GFXPAL[i +  96] = COMP_COLOR(0xFF, 0xFF, 0xFF,   0 + (i << 2));
		GFXPAL[i + 128] = COMP_COLOR(0xFF, 0xFF, 0xFF,  64 + (i << 2));
		GFXPAL[i + 160] = COMP_COLOR(0xFF, 0xFF, 0xFF, 128 + (i << 2));
		GFXPAL[i + 192] = COMP_COLOR(0xFF, 0xFF, 0xFF, 192 + i);
		GFXPAL[i + 224] = COMP_COLOR(0xFF, 0xFF, 0xFF, 224 + i);
	}
}

//Here is where the party begins
void main(int argc, char **argv) {
	//Shut off LEDs
	MISC_REG(MISC_LED_REG)=0x00;

	// Blank out fb while we're loading stuff by disabling all layers. This
	// just shows the background color.
	GFX_REG(GFX_BGNDCOL_REG)=0x330011; //a soft gray
	GFX_REG(GFX_LAYEREN_REG)=0; //disable all gfx layers

	fbmem=calloc(FB_WIDTH,FB_HEIGHT);
	printf("Hello World: framebuffer at %p\n", fbmem);
	
	//Tell the GFX hardware to use this, and its pitch. We also tell the GFX hardware to use palette entries starting
	//from 128 for the frame buffer; the tiles left by the IPL will use palette entries 0-16 already.
	GFX_REG(GFX_FBPITCH_REG)=(128<<GFX_FBPITCH_PAL_OFF)|(FB_WIDTH<<GFX_FBPITCH_PITCH_OFF);
	//Set up the framebuffer address
	GFX_REG(GFX_FBADDR_REG)=((uint32_t)fbmem);

	// load the background picture into memory
	//Now, use a library function to load the image into the framebuffer memory. This function will also set up the palette entries,
	//we tell it to start writing from entry 128.
	int png_size=(&_binary_GOLbak_png_end-&_binary_GOLbak_png_start);
	int i=gfx_load_fb_mem(fbmem, &GFXPAL[128], 8, FB_WIDTH, &_binary_GOLbak_png_start, png_size);
	if (i) printf("gfx_load_fb_mem: error %d\n", i);

	//Flush the memory region to psram so the GFX hw can stream it from there.
	cache_flush(fbmem, fbmem+FB_WIDTH*FB_HEIGHT);

	//Load up the default tileset and font.
	//ToDo: loading pngs takes a long time... move over to pcx instead.
	printf("Loading tiles...\n");
	int gfx_tiles_err = gfx_load_tiles_mem(GFXTILES, &GFXPAL[0], &_binary_tileset_png_start, (&_binary_tileset_png_end-&_binary_tileset_png_start));
	printf("Tiles initialized err=%d\n", gfx_tiles_err);


	FILE *f;
	f=fopen("/dev/console", "w");
	setvbuf(f, NULL, _IONBF, 0); //make console line unbuffered
	// Note that without the setvbuf command, no characters would be printed
	// until 1024 characters are buffered. You normally don't want this.

	fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.
	
	// Reenable the tile layer to show the above text
	GFX_REG(GFX_LAYEREN_REG)=GFX_LAYEREN_FB_8BIT|GFX_LAYEREN_FB|GFX_LAYEREN_TILEA;

	// wait for the button to be released
	while (MISC_REG(MISC_BTN_REG)) ;

	uint32_t buttondebounce = counter60hz();
	uint8_t globber = 2;
	// draw the title screen with instructions
	fprintf(f, "\0330X");
    fprintf(f, "\0330Y");
    fprintf(f, "\n\n           Conway's\n");
    fprintf(f, "       The Game Of Life\n\n");
	fprintf(f, "\n    While on this screen:\n");
    fprintf(f, " Press UP Button one or more\n");
    fprintf(f, " times to set a random seed.\n");
    fprintf(f, "\n\n      Press START to run\n");
    fprintf(f, "\n\n        While running:\n");
    fprintf(f, "    Press LEFT to restart\n");
    fprintf(f, "    Press Button B to exit");

    // wait for the button to be released
	while (MISC_REG(MISC_BTN_REG)) ;
	while (MISC_REG(MISC_BTN_REG) == 0);  // wait for press

	int MyA = 0;
    // Watch for keypress UP or A
    // A will exit the loop and start the evolving
    while (MyA == 0) {
    	while (MISC_REG(MISC_BTN_REG)) ;
    	while (MISC_REG(MISC_BTN_REG) == 0);
    	if (counter60hz() > buttondebounce) {
    		if ((MISC_REG(MISC_BTN_REG) & BUTTON_UP)) {
				globber++;
				buttondebounce = counter60hz()+BUTTON_READ_DELAY; //prevent multiple button reads
			}
			if ((MISC_REG(MISC_BTN_REG) & BUTTON_START)) {
				MyA = 1;
				buttondebounce = counter60hz()+BUTTON_READ_DELAY; //prevent multiple button reads
			}
		}
    }
    	// set the random seed using keypresses

	srand(rand()%globber);
	// The screen is 30x20
	int playArea[30][19];
	int newPlayArea[30][19];
	int xx = 0,  yy = 0, holder = 0, grand = 0;
	for (yy = 0; yy < 19; yy++) {
		for (xx = 0; xx < 30; xx++) {
			grand = rand()%4;
			if (grand == 1) {
				playArea[xx][yy] = 1;
			} else {
				playArea[xx][yy] = 0; }
			newPlayArea[xx][yy] = 0;
		}
	}
	// Display the initial screen
	fprintf(f, "\0330X");
    fprintf(f, "\0330Y");
	for (yy = 0; yy < 19; yy++) {
		for (xx = 0; xx < 30; xx++) {
			if (playArea[xx][yy] == 1) {
				fprintf(f, "#");
			} else {
				fprintf(f, " ");
			}
		}
	}
	fprintf(f, "\0330X");
	fprintf(f, "\03319Y");
	fprintf(f, " B to exit - LEFT to restart");

	// Loop unti the B button is pressed
	while ((MISC_REG(MISC_BTN_REG) & BUTTON_B)==0) {
	// Do the code to evolve
	// top left square
	holder = 0;
		if (playArea[1][0] == 1) {
			holder++; }
		if (playArea[0][1] == 1) {
			holder++; }
		if (playArea[1][1] == 1) {
			holder++; }
	// now check holder status
		if (playArea[0][0] == 1) {
			if (holder == 2) {
				newPlayArea[0][0] = 1;
			} else if (holder == 3) {
				newPlayArea[0][0] = 1;
			} else {
				newPlayArea[0][0] = 0; }
			}
	// now check holder status
		if (playArea[0][0] == 0) {
			if (holder == 3) {
				newPlayArea[0][0] = 1; 
			} else { newPlayArea[0][0] = 0; }
			}
	// top right square
	holder = 0;
	if (playArea[28][0] == 1) {
		holder++; }
	if (playArea[29][1] == 1) {
		holder++; }
	if (playArea[28][1] == 1) {
		holder++; }
	// now check holder status
	if (playArea[29][0] == 1) {
		if (holder == 2) {
			newPlayArea[29][0] = 1;
		} else if (holder == 3) {
			newPlayArea[29][0] = 1;
		} else {
			newPlayArea[29][0] = 0; } }
	// now check holder status
	if (playArea[29][0] == 0) {
		if (holder == 3) {
			newPlayArea[29][0] = 1; 
		} else { newPlayArea[29][0] = 0; }
		}
	//lower left square
	holder = 0;
	if (playArea[1][18] == 1) {
		holder++; }
	if (playArea[0][17] == 1) {
		holder++; }
	if (playArea[1][17] == 1) {
		holder++; }
	// now check holder status
	if (playArea[0][18] == 1) {
		if (holder == 2) {
			newPlayArea[0][18] = 1;
		} else if (holder == 3) {
			newPlayArea[0][18] = 1;
		} else {
			newPlayArea[0][18] = 0; } }
	// now check holder status
	if (playArea[0][18] == 0) {
		if (holder == 3) {
			newPlayArea[0][18] = 1; 
		} else { newPlayArea[0][18] = 0; }
		}
	//lower right square
	holder = 0;
	if (playArea[28][18] == 1) {
		holder++; }
	if (playArea[29][17] == 1) {
		holder++; }
	if (playArea[28][17] == 1) {
		holder++; }
	// now check holder status
	if (playArea[29][18] == 1) {
		if (holder == 2) {
			newPlayArea[29][18] = 1;
		} else if (holder == 3) {
			newPlayArea[29][18] = 1;
		} else {
			newPlayArea[29][18] = 0; } }
	// now check holder status
	if (playArea[29][19] == 0) {
		if (holder == 3) {
			newPlayArea[29][18] = 1; 
		} else { newPlayArea[29][18] = 0; }
		}
	// top row without end corners
	holder = 0;
	for ( xx = 1; xx < 29; xx++) {
		holder  = 0;
		if (playArea[xx-1][0] == 1)
			{ holder++; }
		if (playArea[xx+1][0] == 1)
			{ holder++; }
		if (playArea[xx-1][1] == 1)
			{ holder++; }
		if (playArea[xx][1] == 1)
			{ holder++; }
		if (playArea[xx+1][1] == 1)
			{ holder++; }
		// now check the holder status
		if (playArea[xx][0] == 1) {
			if (holder == 2) {
				newPlayArea[xx][0] = 1; 
			} else if (holder == 3) {
				newPlayArea[xx][0] = 1; 
			} else {
				newPlayArea[xx][0] = 0; } }		
	// new style, shorter code
	if (playArea[xx][0] == 0) {
		if (holder == 3) {
			newPlayArea[xx][0] = 1;
		} else { newPlayArea[xx][0] = 0; }
		}
	}
	// bottom row without end corners
	holder  = 0;
	for ( xx = 1; xx < 29; xx++) {
		holder  = 0;
		if (playArea[xx-1][18] == 1)
			{ holder++; }
		if (playArea[xx+1][18] == 1)
			{ holder++; }
		if (playArea[xx-1][18] == 1)
			{ holder++; }
		if (playArea[xx][17] == 1)
			{ holder++; }
		if (playArea[xx+1][17] == 1)
			{ holder++; }
		// now check the holder status
		if (playArea[xx][18] == 1) {
			if (holder == 2) {
				newPlayArea[xx][18] = 1; 
			} else if (holder == 3) {
				newPlayArea[xx][18] = 1; 
			} else {
				newPlayArea[xx][18] = 0; } }		
	// new style, shorter code
	if (playArea[xx][18] == 0) {
		if (holder == 3) {
			newPlayArea[xx][18] = 1;
		} else { newPlayArea[xx][18] = 0; }
		}
	}
	// left side without corners
	holder = 0;
	for (yy = 1; yy < 18; yy++) {
		holder  = 0;
		if (playArea[0][yy-1] == 1)
			{ holder++; }
		if (playArea[0][yy+1] == 1)
			{ holder++; }
		if (playArea[1][yy-1] == 1)
			{ holder++; }
		if (playArea[1][yy] == 1)
			{ holder++; }
		if (playArea[1][yy+1] == 1)
			{ holder++; }
		// now check holder status
		if (playArea[0][yy] == 1) {
			if (holder == 2) {
				newPlayArea[0][yy] = 1; 
			} else if (holder == 3) {
				newPlayArea[0][yy] = 1; 
			} else {
				newPlayArea[0][yy] = 0; } }
		// new style, shorter code
		if (playArea[0][yy] == 0) {
			if (holder == 3) {
				newPlayArea[0][yy] = 1;
			} else { newPlayArea[0][yy] = 0; }
		}
	}
	// right side without corners
	holder = 0;
	for (yy = 1; yy < 18; yy++) {
		holder  = 0;
		if (playArea[29][yy-1] == 1)
			{ holder++; }
		if (playArea[29][yy+1] == 1)
			{ holder++; }
		if (playArea[28][yy-1] == 1)
			{ holder++; }
		if (playArea[28][yy] == 1)
			{ holder++; }
		if (playArea[28][yy+1] == 1)
			{ holder++; }
		// now check holder status
		if (playArea[29][yy] == 1) {
			if (holder == 2) {
				newPlayArea[29][yy] = 1; 
			} else if (holder == 3) {
				newPlayArea[29][yy] = 1; 
			} else {
				newPlayArea[29][yy] = 0; } }
		// new style, shorter code
		if (playArea[29][yy] == 0) {
			if (holder == 3) {
				newPlayArea[29][yy] = 1;
			} else { newPlayArea[29][yy] = 0; }
		}
	}
	//NOW do the Evolve for the entire rest of the screen
	holder = 0;
	for (yy = 1; yy < 18; yy++) {
		for (xx = 1; xx < 29; xx++) {
			holder  = 0;
			if (playArea[xx-1][yy-1] == 1)
				{ holder++; }
			if (playArea[xx][yy-1] == 1)
				{ holder++; }
			if (playArea[xx+1][yy-1] == 1)
				{ holder++; }
			if (playArea[xx-1][yy] == 1)
				{ holder++; }
			if (playArea[xx+1][yy] == 1)
				{ holder++; }
			if (playArea[xx-1][yy+1] == 1)
				{ holder++; }
			if (playArea[xx][yy+1] == 1)
				{ holder++; }
			if (playArea[xx+1][yy+1] == 1)
				{ holder++; }
			// check holder status
			if (playArea[xx][yy] == 1) {
				if (holder == 2) {
					newPlayArea[xx][yy] = 1; 
				} else if (holder == 3) {
					newPlayArea[xx][yy] = 1; 
				} else {
					newPlayArea[xx][yy] = 0; }
				}
		// new style, shorter code
		if (playArea[xx][yy] == 0) {
			if (holder == 3) {
				newPlayArea[xx][yy] = 1;
			} else { newPlayArea[xx][yy] = 0; }
			}
		}
		}
	// Last thing to do is to copy the newPlayArea to the playArea
	for (yy = 0; yy < 19; yy++) {
		for (xx = 0; xx < 30; xx++) {
			playArea[xx][yy] = newPlayArea[xx][yy];
		}
	}
	
	// And then print the result to the screen
	fprintf(f, "\0330X");
    fprintf(f, "\0330Y");
	for (yy = 0; yy < 19; yy++) {
		for (xx = 0; xx < 30; xx++) {
			if (playArea[xx][yy] == 1) {
				fprintf(f, "#");
			} else {
				fprintf(f, " ");
			}
		}
	}
	// This is a crappydelay function that is needed to keep the program
	// from running too quickly
	for (int ii = 0; ii < 256; ii++) {
		for (volatile int t=0; t<(1<<11); t++);
	}
	// This is the area where we will check for a different button press
	// If the button press is LEFT arrow then reset all of the parameters
	// to the defaults and send the result to the screen, then continue.
	if (MISC_REG(MISC_BTN_REG) & BUTTON_LEFT) {
		fprintf(f, "\033C"); //clear the console. Note '\033' is the escape character.

		// wait for the button to be released
		while (MISC_REG(MISC_BTN_REG)) ;

		buttondebounce = counter60hz();
		globber = 2;
		// draw the title screen with instructions
		fprintf(f, "\0330X");
	    fprintf(f, "\0330Y");
	    fprintf(f, "\n\n           Conway's\n");
		fprintf(f, "       The Game Of Life\n\n");
		fprintf(f, "\n    While on this screen:\n");
		fprintf(f, " Press UP Button one or more\n");
		fprintf(f, " times to set a random seed.\n");
		fprintf(f, "\n\n      Press START to run\n");
		fprintf(f, "\n\n        While running:\n");
		fprintf(f, "    Press LEFT to restart\n");
		fprintf(f, "    Press Button B to exit");

	    // wait for the button to be released
		while (MISC_REG(MISC_BTN_REG)) ;
		while (MISC_REG(MISC_BTN_REG) == 0);  // wait for press

		int MyA = 0;
	    // Watch for keypress UP or A
	    // A will exit the loop and start the evolving
	    while (MyA == 0) {
	    	while (MISC_REG(MISC_BTN_REG)) ;
	    	while (MISC_REG(MISC_BTN_REG) == 0);
	    	if (counter60hz() > buttondebounce) {
	    		if ((MISC_REG(MISC_BTN_REG) & BUTTON_UP)) {
					globber++;
					buttondebounce = counter60hz()+BUTTON_READ_DELAY; //prevent multiple button reads
				}
				if ((MISC_REG(MISC_BTN_REG) & BUTTON_START)) {
					MyA = 1;
					buttondebounce = counter60hz()+BUTTON_READ_DELAY; //prevent multiple button reads
				}
			}
	    }
	    // set the random seed using keypresses

		srand(rand()%globber);
		// The screen is 30x20
		// int playArea[30][19];
		// int newPlayArea[30][19];
		int xx = 0,  yy = 0, holder = 0, grand = 0;
		for (yy = 0; yy < 19; yy++) {
			for (xx = 0; xx < 30; xx++) {
				grand = rand()%4;
				if (grand == 1) {
					playArea[xx][yy] = 1;
				} else {
					playArea[xx][yy] = 0; }
				newPlayArea[xx][yy] = 0;
			}
		}
		// Display the initial screen
		fprintf(f, "\0330X");
	    fprintf(f, "\0330Y");
		for (yy = 0; yy < 19; yy++) {
			for (xx = 0; xx < 30; xx++) {
				if (playArea[xx][yy] == 1) {
					fprintf(f, "#");
				} else {
					fprintf(f, " ");
				}
			}
		}
		fprintf(f, "\0330X");
		fprintf(f, "\03319Y");
		fprintf(f, " B to exit - LEFT to restart");

		}
	
	}
	
}