// Made by scripts/n3ds_title_logo.py of the port's repository: do not edit by hand.
// The changes to the logo on the title screen (window_title_logo_paint): the picture the game
// has for it (SPR_MENU_LOGO) is cut off at its right edge. Positions are in the logo's window.
// To change them: n3ds_title_logo.py export, edit the picture, n3ds_title_logo.py apply.

// Drawn over the picture, each run { x, top, bottom, colour }: a column of pixels of one
// palette colour
#define N3DS_TITLE_LOGO_RUN_COUNT 17
static const uint8 N3dsTitleLogoRuns[17][4] = {
	{ 195, 32, 32, 122 },
	{ 195, 34, 34, 187 },
	{ 195, 36, 37, 53 },
	{ 196, 32, 32, 13 },
	{ 196, 33, 33, 41 },
	{ 196, 42, 42, 41 },
	{ 196, 50, 50, 120 },
	{ 197, 31, 35, 120 },
	{ 197, 36, 36, 130 },
	{ 197, 37, 37, 120 },
	{ 197, 38, 39, 130 },
	{ 197, 40, 41, 120 },
	{ 197, 42, 44, 130 },
	{ 197, 45, 45, 120 },
	{ 197, 46, 47, 130 },
	{ 197, 48, 48, 120 },
	{ 197, 49, 49, 130 },
};

// Pixels of the picture that are not shown, each { x, y }
#define N3DS_TITLE_LOGO_ERASED_COUNT 1
static const uint8 N3dsTitleLogoErased[1][2] = {
	{ 196, 51 },
};
