#pragma region Copyright (c) 2014-2016 OpenRCT2 Developers
/*****************************************************************************
 * OpenRCT2, an open source clone of Roller Coaster Tycoon 2.
 *
 * OpenRCT2 is the work of many authors, a full list can be found in contributors.md
 * For more information, visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * A full copy of the GNU General Public License can be found in licence.txt
 *****************************************************************************/
#pragma endregion

#include "../config.h"
#include "../game.h"
#include "../interface/graph.h"
#include "../interface/widget.h"
#include "../interface/window.h"
#include "../localisation/date.h"
#include "../localisation/localisation.h"
#include "../management/finance.h"
#include "../management/marketing.h"
#include "../management/research.h"
#include "../ride/ride.h"
#include "../ride/ride_data.h"
#include "../scenario/scenario.h"
#include "../sprites.h"
#include "dropdown.h"
#include "../interface/themes.h"

enum {
	WINDOW_FINANCES_PAGE_SUMMARY,
	WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH,
	WINDOW_FINANCES_PAGE_VALUE_GRAPH,
	WINDOW_FINANCES_PAGE_PROFIT_GRAPH,
	WINDOW_FINANCES_PAGE_MARKETING,
	WINDOW_FINANCES_PAGE_RESEARCH,
	WINDOW_FINANCES_PAGE_COUNT
};

enum {
	WIDX_BACKGROUND,
	WIDX_TITLE,
	WIDX_CLOSE,
	WIDX_PAGE_BACKGROUND,
	WIDX_TAB_1,
	WIDX_TAB_2,
	WIDX_TAB_3,
	WIDX_TAB_4,
	WIDX_TAB_5,
	WIDX_TAB_6,

	WIDX_LOAN = 10,
	WIDX_LOAN_INCREASE,
	WIDX_LOAN_DECREASE,

	WIDX_ACITVE_CAMPAGINS_GROUP = 10,
	WIDX_CAMPAGINS_AVAILABLE_GROUP,
	WIDX_CAMPAIGN_1,
	WIDX_CAMPAIGN_2,
	WIDX_CAMPAIGN_3,
	WIDX_CAMPAIGN_4,
	WIDX_CAMPAIGN_5,
	WIDX_CAMPAIGN_6,

	WIDX_RESEARCH_FUNDING = 11,
	WIDX_RESEARCH_FUNDING_DROPDOWN_BUTTON,
	WIDX_TRANSPORT_RIDES = 14,
	WIDX_GENTLE_RIDES,
	WIDX_ROLLER_COASTERS,
	WIDX_THRILL_RIDES,
	WIDX_WATER_RIDES,
	WIDX_SHOPS_AND_STALLS,
	WIDX_SCENERY_AND_THEMING,
};

#pragma region Widgets

static rct_widget window_finances_summary_widgets[] = {
	{ WWT_FRAME,			0,	0,		529,	0,		256,	0xFFFFFFFF,							STR_NONE },
	{ WWT_CAPTION,			0,	1,		528,	1,		14,		STR_FINANCIAL_SUMMARY,				STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	517,	527,	2,		13,		STR_CLOSE_X,						STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		529,	43,		256,	0xFFFFFFFF,							STR_NONE },
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,				STR_FINANCES_SHOW_SUMMARY_TAB_TIP },
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,				STR_FINANCES_SHOW_CASH_TAB_TIP },
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,				STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP },
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,				STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP },
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,				STR_FINANCES_SHOW_MARKETING_TAB_TIP },
	{ WWT_TAB,				1,	158,	188,	17,		43,		0x20000000 | SPR_TAB,				STR_FINANCES_RESEARCH_TIP },
	{ WWT_SPINNER,			1,	64,		153,	229,	240,	STR_FINANCES_SUMMARY_LOAN_VALUE,	STR_NONE },
	{ WWT_DROPDOWN_BUTTON,	1,	142,	152,	230,	234,	STR_NUMERIC_UP,						STR_NONE },
	{ WWT_DROPDOWN_BUTTON,	1,	142,	152,	235,	239,	STR_NUMERIC_DOWN,					STR_NONE },
	{ WIDGETS_END },
};

static rct_widget window_finances_cash_widgets[] = {
	{ WWT_FRAME,			0,	0,		529,	0,		256,	0xFFFFFFFF,				STR_NONE },
	{ WWT_CAPTION,			0,	1,		528,	1,		14,		STR_FINANCIAL_GRAPH,	STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	517,	527,	2,		13,		STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		529,	43,		256,	0xFFFFFFFF,				STR_NONE },
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_SUMMARY_TAB_TIP },
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_CASH_TAB_TIP },
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP },
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP },
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_MARKETING_TAB_TIP },
	{ WWT_TAB,				1,	158,	188,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_RESEARCH_TIP },
	{ WIDGETS_END },
};

static rct_widget window_finances_park_value_widgets[] = {
	{ WWT_FRAME,			0,	0,		529,	0,		256,	0xFFFFFFFF,				STR_NONE },
	{ WWT_CAPTION,			0,	1,		528,	1,		14,		STR_PARK_VALUE_GRAPH,	STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	517,	527,	2,		13,		STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		529,	43,		256,	0xFFFFFFFF,				STR_NONE },
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_SUMMARY_TAB_TIP },
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_CASH_TAB_TIP },
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP },
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP },
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_MARKETING_TAB_TIP },
	{ WWT_TAB,				1,	158,	188,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_RESEARCH_TIP },
	{ WIDGETS_END },
};

static rct_widget window_finances_profit_widgets[] = {
	{ WWT_FRAME,			0,	0,		529,	0,		256,	0xFFFFFFFF,				STR_NONE },
	{ WWT_CAPTION,			0,	1,		528,	1,		14,		STR_PROFIT_GRAPH,		STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	517,	527,	2,		13,		STR_CLOSE_X,			STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		529,	43,		256,	0xFFFFFFFF,				STR_NONE },
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_SUMMARY_TAB_TIP },
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_CASH_TAB_TIP },
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP },
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP },
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_SHOW_MARKETING_TAB_TIP },
	{ WWT_TAB,				1,	158,	188,	17,		43,		0x20000000 | SPR_TAB,	STR_FINANCES_RESEARCH_TIP },
	{ WIDGETS_END },
};

static rct_widget window_finances_marketing_widgets[] = {
	{ WWT_FRAME,			0,	0,		529,	0,		256,	0xFFFFFFFF,								STR_NONE },
	{ WWT_CAPTION,			0,	1,		528,	1,		14,		STR_MARKETING,							STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	517,	527,	2,		13,		STR_CLOSE_X,							STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		529,	43,		256,	0xFFFFFFFF,								STR_NONE },
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_SUMMARY_TAB_TIP },
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_CASH_TAB_TIP },
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP },
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP },
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_MARKETING_TAB_TIP },
	{ WWT_TAB,				1,	158,	188,	17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_RESEARCH_TIP },
	{ WWT_GROUPBOX,			2,	3,		526,	47,		91,		STR_MARKETING_CAMPAIGNS_IN_OPERATION,	STR_NONE },
	{ WWT_GROUPBOX,			2,	3,		526,	47,		252,	STR_MARKETING_CAMPAIGNS_AVAILABLE,		STR_NONE },
	{ WWT_IMGBTN,			1,	8,		521,	0,		11,		0xFFFFFFFF,								STR_START_THIS_MARKETING_CAMPAIGN },
	{ WWT_IMGBTN,			1,	8,		521,	0,		11,		0xFFFFFFFF,								STR_START_THIS_MARKETING_CAMPAIGN },
	{ WWT_IMGBTN,			1,	8,		521,	0,		11,		0xFFFFFFFF,								STR_START_THIS_MARKETING_CAMPAIGN },
	{ WWT_IMGBTN,			1,	8,		521,	0,		11,		0xFFFFFFFF,								STR_START_THIS_MARKETING_CAMPAIGN },
	{ WWT_IMGBTN,			1,	8,		521,	0,		11,		0xFFFFFFFF,								STR_START_THIS_MARKETING_CAMPAIGN },
	{ WWT_IMGBTN,			1,	8,		521,	0,		11,		0xFFFFFFFF,								STR_START_THIS_MARKETING_CAMPAIGN },
	{ WIDGETS_END },
};

static rct_widget window_finances_research_widgets[] = {
	{ WWT_FRAME,			0,	0,		319,	0,		206,	0xFFFFFFFF,								STR_NONE },
	{ WWT_CAPTION,			0,	1,		318,	1,		14,		STR_RESEARCH_FUNDING,					STR_WINDOW_TITLE_TIP },
	{ WWT_CLOSEBOX,			0,	307,	317,	2,		13,		STR_CLOSE_X,							STR_CLOSE_WINDOW_TIP },
	{ WWT_RESIZE,			1,	0,		319,	43,		206,	0xFFFFFFFF,								STR_NONE },
	{ WWT_TAB,				1,	3,		33,		17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_SUMMARY_TAB_TIP },
	{ WWT_TAB,				1,	34,		64,		17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_CASH_TAB_TIP },
	{ WWT_TAB,				1,	65,		95,		17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_PARK_VALUE_TAB_TIP },
	{ WWT_TAB,				1,	96,		126,	17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_WEEKLY_PROFIT_TAB_TIP },
	{ WWT_TAB,				1,	127,	157,	17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_SHOW_MARKETING_TAB_TIP },
	{ WWT_TAB,				1,	158,	188,	17,		43,		0x20000000 | SPR_TAB,					STR_FINANCES_RESEARCH_TIP },
	{ WWT_GROUPBOX,			2,	3,		316,	47,		91,		STR_RESEARCH_FUNDING_,					STR_NONE },
	{ WWT_DROPDOWN,			2,	8,		167,	59,		70,		0xFFFFFFFF,								STR_SELECT_LEVEL_OF_RESEARCH_AND_DEVELOPMENT },
	{ WWT_DROPDOWN_BUTTON,	2,	156,	166,	60,		69,		STR_DROPDOWN_GLYPH,			    		STR_SELECT_LEVEL_OF_RESEARCH_AND_DEVELOPMENT },
	{ WWT_GROUPBOX,			2,	3,		316,	96,		202,	STR_RESEARCH_PRIORITIES,				STR_NONE },
	{ WWT_CHECKBOX,			2,	8,		311,	108,	119,	STR_RESEARCH_NEW_TRANSPORT_RIDES,		STR_RESEARCH_NEW_TRANSPORT_RIDES_TIP },
	{ WWT_CHECKBOX,			2,	8,		311,	121,	132,	STR_RESEARCH_NEW_GENTLE_RIDES,			STR_RESEARCH_NEW_GENTLE_RIDES_TIP },
	{ WWT_CHECKBOX,			2,	8,		311,	134,	145,	STR_RESEARCH_NEW_ROLLER_COASTERS,		STR_RESEARCH_NEW_ROLLER_COASTERS_TIP },
	{ WWT_CHECKBOX,			2,	8,		311,	147,	158,	STR_RESEARCH_NEW_THRILL_RIDES,			STR_RESEARCH_NEW_THRILL_RIDES_TIP },
	{ WWT_CHECKBOX,			2,	8,		311,	160,	171,	STR_RESEARCH_NEW_WATER_RIDES,			STR_RESEARCH_NEW_WATER_RIDES_TIP },
	{ WWT_CHECKBOX,			2,	8,		311,	173,	184,	STR_RESEARCH_NEW_SHOPS_AND_STALLS,		STR_RESEARCH_NEW_SHOPS_AND_STALLS_TIP },
	{ WWT_CHECKBOX,			2,	8,		311,	186,	197,	STR_RESEARCH_NEW_SCENERY_AND_THEMING,	STR_RESEARCH_NEW_SCENERY_AND_THEMING_TIP },
	{ WIDGETS_END },
};

static rct_widget *window_finances_page_widgets[] = {
	window_finances_summary_widgets,
	window_finances_cash_widgets,
	window_finances_park_value_widgets,
	window_finances_profit_widgets,
	window_finances_marketing_widgets,
	window_finances_research_widgets
};

#ifdef __3DS__
// n3ds port: the summary page (window_finances_summary_n3ds_paint). The table on the left with
// the last two months, in the small font; what the original has below the table (loan, cash,
// park and company value) in a column on the right.
#define N3DS_SUMMARY_MONTHS       2
#define N3DS_SUMMARY_TABLE_LEFT   4
#define N3DS_SUMMARY_COLUMN_LEFT  98	// of the first month
#define N3DS_SUMMARY_COLUMN_WIDTH 62
#define N3DS_SUMMARY_SIDE_LEFT    228
#define N3DS_SUMMARY_SIDE_WIDTH   88
#define N3DS_SUMMARY_LOAN_TOP     70	// of the loan's two buttons

// The graph pages: the box starts this far below the page's top, the labels of the Y axis and
// the plot this far into the box, and the plot has this many points (the original: 15; 18, 14;
// 98, 17; 64). A point is 6 pixels wide and the plot 170 high (graph.c), so the window has room
// for half the history, and the plot is moved up as far as its heading lets it.
#define FINANCES_GRAPH_TOP        13
#define FINANCES_GRAPH_AXIS_X     36
#define FINANCES_GRAPH_AXIS_Y     9
#define FINANCES_GRAPH_PLOT_X     116
#define FINANCES_GRAPH_PLOT_Y     12
#define FINANCES_GRAPH_POINTS     32

static void n3ds_set_widget(rct_widget *widget, int left, int right, int top, int bottom)
{
	widget->left = left;
	widget->right = right;
	widget->top = top;
	widget->bottom = bottom;
}

/**
 * n3ds port: fit the window (530x257; the research page 320x207) to the 320x240 bottom screen,
 * every page that size. The table of the summary page and the texts of the marketing page are
 * in the small font, where the medium one does not fit; controls have the size for a finger
 * (platform.h). What the paint functions draw at places of their own: see their __3DS__ parts.
 */
static void window_finances_n3ds_layout()
{
	rct_widget *widgets;
	int i;

	for (i = 0; i < WINDOW_FINANCES_PAGE_COUNT; i++) {
		widgets = window_finances_page_widgets[i];
		n3ds_set_widget(&widgets[WIDX_BACKGROUND], 0, N3DS_BOTTOM_WIDTH - 1, 0, N3DS_BOTTOM_HEIGHT - 1);
		n3ds_set_widget(&widgets[WIDX_TITLE], 1, N3DS_BOTTOM_WIDTH - 2, 1, 14);
		n3ds_set_widget(&widgets[WIDX_CLOSE], N3DS_BOTTOM_WIDTH - 13, N3DS_BOTTOM_WIDTH - 3, 2, 13);
		n3ds_set_widget(&widgets[WIDX_PAGE_BACKGROUND], 0, N3DS_BOTTOM_WIDTH - 1, 43, N3DS_BOTTOM_HEIGHT - 1);
	}

	// Summary: the loan's buttons in the column on the right. The column is too narrow for the
	// amount beside them: the box holds the buttons only and paint draws the amount above it.
	widgets = window_finances_summary_widgets;
	window_n3ds_place_spinner(
		&widgets[WIDX_LOAN],
		N3DS_SUMMARY_SIDE_LEFT, N3DS_SUMMARY_SIDE_LEFT + 2 * N3DS_CONTROL_BUTTON_WIDTH + 1, N3DS_SUMMARY_LOAN_TOP
	);
	widgets[WIDX_LOAN].text = STR_NONE;

	// Marketing: the buttons are placed down the page by invalidate
	widgets = window_finances_marketing_widgets;
	widgets[WIDX_ACITVE_CAMPAGINS_GROUP].right = N3DS_BOTTOM_WIDTH - 4;
	widgets[WIDX_CAMPAGINS_AVAILABLE_GROUP].right = N3DS_BOTTOM_WIDTH - 4;
	widgets[WIDX_CAMPAGINS_AVAILABLE_GROUP].bottom = N3DS_BOTTOM_HEIGHT - 4;
	for (i = WIDX_CAMPAIGN_1; i <= WIDX_CAMPAIGN_6; i++)
		widgets[i].right = N3DS_BOTTOM_WIDTH - 9;

	// Research: the funding dropdown ends above the cost that the page paints below it
	// (window_research_funding_page_paint, shared with the research window); the priorities
	// in rows for a finger, down to the bottom of the window
	widgets = window_finances_research_widgets;
	window_n3ds_place_dropdown(&widgets[WIDX_RESEARCH_FUNDING], 8, 167, 57);
	widgets[WIDX_TRANSPORT_RIDES - 1].bottom = N3DS_BOTTOM_HEIGHT - 4;
	for (i = 0; i < 7; i++)
		n3ds_set_widget(&widgets[WIDX_TRANSPORT_RIDES + i], 8, 311, 108 + i * 18, 108 + i * 18 + 15);
}
#else
#define FINANCES_GRAPH_TOP        15
#define FINANCES_GRAPH_AXIS_X     18
#define FINANCES_GRAPH_AXIS_Y     14
#define FINANCES_GRAPH_PLOT_X     98
#define FINANCES_GRAPH_PLOT_Y     17
#define FINANCES_GRAPH_POINTS     64
#endif

#pragma endregion

#pragma region Events

static void window_finances_summary_mouseup(rct_window *w, int widgetIndex);
static void window_finances_summary_mousedown(int widgetIndex, rct_window*w, rct_widget* widget);
static void window_finances_summary_update(rct_window *w);
static void window_finances_summary_invalidate(rct_window *w);
static void window_finances_summary_paint(rct_window *w, rct_drawpixelinfo *dpi);

static void window_finances_financial_graph_mouseup(rct_window *w, int widgetIndex);
static void window_finances_financial_graph_update(rct_window *w);
static void window_finances_financial_graph_invalidate(rct_window *w);
static void window_finances_financial_graph_paint(rct_window *w, rct_drawpixelinfo *dpi);

static void window_finances_park_value_graph_mouseup(rct_window *w, int widgetIndex);
static void window_finances_park_value_graph_update(rct_window *w);
static void window_finances_park_value_graph_invalidate(rct_window *w);
static void window_finances_park_value_graph_paint(rct_window *w, rct_drawpixelinfo *dpi);

static void window_finances_profit_graph_mouseup(rct_window *w, int widgetIndex);
static void window_finances_profit_graph_update(rct_window *w);
static void window_finances_profit_graph_invalidate(rct_window *w);
static void window_finances_profit_graph_paint(rct_window *w, rct_drawpixelinfo *dpi);

static void window_finances_marketing_mouseup(rct_window *w, int widgetIndex);
static void window_finances_marketing_update(rct_window *w);
static void window_finances_marketing_invalidate(rct_window *w);
static void window_finances_marketing_paint(rct_window *w, rct_drawpixelinfo *dpi);

static void window_finances_research_mouseup(rct_window *w, int widgetIndex);
static void window_finances_research_mousedown(int widgetIndex, rct_window*w, rct_widget* widget);
static void window_finances_research_dropdown(rct_window *w, int widgetIndex, int dropdownIndex);
static void window_finances_research_update(rct_window *w);
static void window_finances_research_invalidate(rct_window *w);
static void window_finances_research_paint(rct_window *w, rct_drawpixelinfo *dpi);

// 0x00988EB8
static rct_window_event_list window_finances_summary_events = {
	NULL,
	window_finances_summary_mouseup,
	NULL,
	window_finances_summary_mousedown,
	NULL,
	NULL,
	window_finances_summary_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_summary_invalidate,
	window_finances_summary_paint,
	NULL
};

// 0x00988F28
static rct_window_event_list window_finances_financial_graph_events = {
	NULL,
	window_finances_financial_graph_mouseup,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_financial_graph_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_financial_graph_invalidate,
	window_finances_financial_graph_paint,
	NULL
};

// 0x00988F98
static rct_window_event_list window_finances_value_graph_events = {
	NULL,
	window_finances_park_value_graph_mouseup,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_park_value_graph_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_park_value_graph_invalidate,
	window_finances_park_value_graph_paint,
	NULL
};

// 0x00989008
static rct_window_event_list window_finances_profit_graph_events = {
	NULL,
	window_finances_profit_graph_mouseup,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_profit_graph_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_profit_graph_invalidate,
	window_finances_profit_graph_paint,
	NULL
};

// 0x00989078
static rct_window_event_list window_finances_marketing_events = {
	NULL,
	window_finances_marketing_mouseup,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_marketing_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_marketing_invalidate,
	window_finances_marketing_paint,
	NULL
};

// 0x009890E8
static rct_window_event_list window_finances_research_events = {
	NULL,
	window_finances_research_mouseup,
	NULL,
	window_finances_research_mousedown,
	window_finances_research_dropdown,
	NULL,
	window_finances_research_update,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
	window_finances_research_invalidate,
	window_finances_research_paint,
	NULL
};

static rct_window_event_list *window_finances_page_events[] = {
	&window_finances_summary_events,
	&window_finances_financial_graph_events,
	&window_finances_value_graph_events,
	&window_finances_profit_graph_events,
	&window_finances_marketing_events,
	&window_finances_research_events
};

static void window_finances_set_colours();

#pragma endregion

#pragma region Enabled widgets

static uint32 window_finances_page_enabled_widgets[] = {
	(1 << WIDX_CLOSE) |
	(1 << WIDX_TAB_1) |
	(1 << WIDX_TAB_2) |
	(1 << WIDX_TAB_3) |
	(1 << WIDX_TAB_4) |
	(1 << WIDX_TAB_5) |
	(1 << WIDX_TAB_6) |
	(1 << WIDX_LOAN_INCREASE) |
	(1 << WIDX_LOAN_DECREASE),

	(1 << WIDX_CLOSE) |
	(1 << WIDX_TAB_1) |
	(1 << WIDX_TAB_2) |
	(1 << WIDX_TAB_3) |
	(1 << WIDX_TAB_4) |
	(1 << WIDX_TAB_5) |
	(1 << WIDX_TAB_6),

	(1 << WIDX_CLOSE) |
	(1 << WIDX_TAB_1) |
	(1 << WIDX_TAB_2) |
	(1 << WIDX_TAB_3) |
	(1 << WIDX_TAB_4) |
	(1 << WIDX_TAB_5) |
	(1 << WIDX_TAB_6),

	(1 << WIDX_CLOSE) |
	(1 << WIDX_TAB_1) |
	(1 << WIDX_TAB_2) |
	(1 << WIDX_TAB_3) |
	(1 << WIDX_TAB_4) |
	(1 << WIDX_TAB_5) |
	(1 << WIDX_TAB_6),

	(1 << WIDX_CLOSE) |
	(1 << WIDX_TAB_1) |
	(1 << WIDX_TAB_2) |
	(1 << WIDX_TAB_3) |
	(1 << WIDX_TAB_4) |
	(1 << WIDX_TAB_5) |
	(1 << WIDX_TAB_6) |
	(1 << WIDX_CAMPAIGN_1) |
	(1 << WIDX_CAMPAIGN_2) |
	(1 << WIDX_CAMPAIGN_3) |
	(1 << WIDX_CAMPAIGN_4) |
	(1 << WIDX_CAMPAIGN_5) |
	(1 << WIDX_CAMPAIGN_6),

	(1 << WIDX_CLOSE) |
	(1 << WIDX_TAB_1) |
	(1 << WIDX_TAB_2) |
	(1 << WIDX_TAB_3) |
	(1 << WIDX_TAB_4) |
	(1 << WIDX_TAB_5) |
	(1 << WIDX_TAB_6) |

	(1 << WIDX_RESEARCH_FUNDING) |
	(1 << WIDX_RESEARCH_FUNDING_DROPDOWN_BUTTON) |
	(1 << WIDX_TRANSPORT_RIDES) |
	(1 << WIDX_GENTLE_RIDES) |
	(1 << WIDX_ROLLER_COASTERS) |
	(1 << WIDX_THRILL_RIDES) |
	(1 << WIDX_WATER_RIDES) |
	(1 << WIDX_SHOPS_AND_STALLS) |
	(1 << WIDX_SCENERY_AND_THEMING)
};

static uint32 window_finances_page_hold_down_widgets[] = {
	(1 << WIDX_LOAN_INCREASE) |
	(1 << WIDX_LOAN_DECREASE),

	0,
	0,
	0,
	0,
	0
};

#pragma endregion

const int window_finances_tab_animation_loops[] = { 16, 32, 32, 32, 38, 16 };

static const rct_string_id window_finances_summary_row_labels[RCT_EXPENDITURE_TYPE_COUNT] = {
	STR_FINANCES_SUMMARY_RIDE_CONSTRUCTION,
	STR_FINANCES_SUMMARY_RIDE_RUNNING_COSTS,
	STR_FINANCES_SUMMARY_LAND_PURCHASE,
	STR_FINANCES_SUMMARY_LANDSCAPING,
	STR_FINANCES_SUMMARY_PARK_ENTRANCE_TICKETS,
	STR_FINANCES_SUMMARY_RIDE_TICKETS,
	STR_FINANCES_SUMMARY_SHOP_SALES,
	STR_FINANCES_SUMMARY_SHOP_STOCK,
	STR_FINANCES_SUMMARY_FOOD_DRINK_SALES,
	STR_FINANCES_SUMMARY_FOOD_DRINK_STOCK,
	STR_FINANCES_SUMMARY_STAFF_WAGES,
	STR_FINANCES_SUMMARY_MARKETING,
	STR_FINANCES_SUMMARY_RESEARCH,
	STR_FINANCES_SUMMARY_LOAN_INTEREST,
};

static void window_finances_set_page(rct_window *w, int page);
static void window_finances_set_pressed_tab(rct_window *w);
static void window_finances_draw_tab_images(rct_drawpixelinfo *dpi, rct_window *w);

/**
 *
 *  rct2: 0x0069DDF1
 */
void window_finances_open()
{
	rct_window *w;

	w = window_bring_to_front_by_class(WC_FINANCES);
	if (w == NULL) {
#ifdef __3DS__
		window_finances_n3ds_layout();
		w = window_create_auto_pos(N3DS_BOTTOM_WIDTH, N3DS_BOTTOM_HEIGHT, window_finances_page_events[0], WC_FINANCES, WF_10);
#else
		w = window_create_auto_pos(530, 257, window_finances_page_events[0], WC_FINANCES, WF_10);
#endif
		w->number = 0;
		w->frame_no = 0;

		research_update_uncompleted_types();
	}

	w->page = WINDOW_FINANCES_PAGE_SUMMARY;
	window_invalidate(w);
#ifdef __3DS__
	w->width = N3DS_BOTTOM_WIDTH;
	w->height = N3DS_BOTTOM_HEIGHT;
#else
	w->width = 530;
	w->height = 257;
#endif
	window_invalidate(w);

	w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_SUMMARY];
	w->enabled_widgets = window_finances_page_enabled_widgets[WINDOW_FINANCES_PAGE_SUMMARY];
	w->hold_down_widgets = window_finances_page_hold_down_widgets[WINDOW_FINANCES_PAGE_SUMMARY];
	w->event_handlers = window_finances_page_events[WINDOW_FINANCES_PAGE_SUMMARY];
	w->pressed_widgets = 0;
	w->disabled_widgets = 0;
	window_init_scroll_widgets(w);
}

/**
 *
 *  rct2: 0x0069DDE1
 */
void window_finances_research_open()
{
	rct_window *w;

	window_finances_open();
	w = window_find_by_class(WC_FINANCES);
	if (w != NULL)
		window_finances_set_page(w, WINDOW_FINANCES_PAGE_RESEARCH);
}

#pragma region Summary page

/**
 *
 *  rct2: 0x0069CA99
 */
static void window_finances_summary_mouseup(rct_window *w, int widgetIndex)
{
	if (widgetIndex == WIDX_CLOSE)
		window_close(w);
	else if (widgetIndex >= WIDX_TAB_1 && widgetIndex <= WIDX_TAB_6)
		window_finances_set_page(w, widgetIndex - WIDX_TAB_1);
}

/**
 *
 *  rct2: 0x0069CAB0
 */
static void window_finances_summary_mousedown(int widgetIndex, rct_window*w, rct_widget* widget)
{
	money32 newLoan;

	switch (widgetIndex) {
	case WIDX_LOAN_INCREASE:
		newLoan = gBankLoan + MONEY(1000, 00);
		gGameCommandErrorTitle = STR_CANT_BORROW_ANY_MORE_MONEY;
		finance_set_loan(newLoan);
		break;
	case WIDX_LOAN_DECREASE:
		if (gBankLoan > 0) {
			newLoan = gBankLoan - MONEY(1000, 00);
			gGameCommandErrorTitle = STR_CANT_PAY_BACK_LOAN;
			finance_set_loan(newLoan);
		}
		break;
	}
}

/**
 *
 *  rct2: 0x0069CBA6
 */
static void window_finances_summary_update(rct_window *w)
{
	// Tab animation
	if (++w->frame_no >= window_finances_tab_animation_loops[w->page])
		w->frame_no = 0;
	widget_invalidate(w, WIDX_TAB_1);
}

/**
 *
 *  rct2: 0x0069C732
 */
static void window_finances_summary_invalidate(rct_window *w)
{
	colour_scheme_update(w);

	if (w->widgets != window_finances_page_widgets[WINDOW_FINANCES_PAGE_SUMMARY]) {
		w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_SUMMARY];
		window_init_scroll_widgets(w);
	}

	window_finances_set_pressed_tab(w);
	set_format_arg(6, money32, gBankLoan);
}

#ifdef __3DS__
/**
 * n3ds port: the summary page in 320x240 (see N3DS_SUMMARY_*).
 * The table is where the original has it, with the last two months instead of five and its
 * text in the small font: drawn through STR_GRAPH_LABEL, "{SMALLFONT}{BLACK}{STRINGID}", which
 * takes the original's string and that string's arguments. The drawing functions set the medium
 * font themselves, so the small one has to come from the string.
 * Below the table there is no room: the loan (its amount above its two buttons), the cash and
 * the values go down a column on the right, label and amount on a line each where both do not
 * fit on one.
 */
static void window_finances_summary_n3ds_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, j, x, y;
	rct_string_id format;

	x = w->x + N3DS_SUMMARY_TABLE_LEFT;
	y = w->y + 47;
	int tableRight = w->x + N3DS_SUMMARY_COLUMN_LEFT + N3DS_SUMMARY_MONTHS * N3DS_SUMMARY_COLUMN_WIDTH;

	// Expenditure / Income heading
	format = STR_FINANCES_SUMMARY_EXPENDITURE_INCOME;
	draw_string_left_underline(dpi, STR_GRAPH_LABEL, &format, COLOUR_BLACK, x + 2, y - 1);
	y += 14;

	// Expenditure / Income row labels
	for (i = 0; i < 14; i++) {
		// Darken every even row
		if (i % 2 == 0)
			gfx_fill_rect(dpi, x, y, tableRight, y + 9, ColourMapA[w->colours[1]].lighter | 0x1000000);

		format = window_finances_summary_row_labels[i];
		gfx_draw_string_left(dpi, STR_GRAPH_LABEL, &format, COLOUR_BLACK, x + 2, y - 1);
		y += 10;
	}

	// Expenditure / Income values for each month
	x = w->x + N3DS_SUMMARY_COLUMN_LEFT;
	sint16 currentMonthYear = gDateMonthsElapsed;
	for (i = N3DS_SUMMARY_MONTHS - 1; i >= 0; i--) {
		int columnRight = x + N3DS_SUMMARY_COLUMN_WIDTH - 2;
		y = w->y + 47;

		sint16 monthyear = currentMonthYear - i;
		if (monthyear < 0)
			continue;

		// Month heading
		set_format_arg(0, rct_string_id, STR_FINANCES_SUMMARY_MONTH_HEADING);
		set_format_arg(2, uint16, monthyear);
		draw_string_right_underline(
			dpi,
			monthyear == currentMonthYear ? STR_SMALL_WINDOW_COLOUR_2_STRINGID : STR_GRAPH_LABEL,
			gCommonFormatArgs,
			COLOUR_BLACK,
			columnRight,
			y - 1
		);
		y += 14;

		// Month expenditures
		money32 profit = 0;
		money32 *expenditures = &gExpenditureTable[i * RCT_EXPENDITURE_TYPE_COUNT];
		for (j = 0; j < 14; j++) {
			money32 expenditure = expenditures[j];
			if (expenditure != 0) {
				profit += expenditure;
				set_format_arg(0, rct_string_id, expenditure >= 0 ? STR_FINANCES_SUMMARY_INCOME_VALUE : STR_FINANCES_SUMMARY_EXPENDITURE_VALUE);
				set_format_arg(2, money32, expenditure);
				gfx_draw_string_right(dpi, STR_GRAPH_LABEL, gCommonFormatArgs, COLOUR_BLACK, columnRight, y - 1);
			}
			y += 10;
		}
		y += 4;

		// Month profit
		set_format_arg(0, rct_string_id, profit >= 0 ? STR_FINANCES_SUMMARY_INCOME_VALUE : STR_FINANCES_SUMMARY_LOSS_VALUE);
		set_format_arg(2, money32, profit);
		gfx_draw_string_right(dpi, STR_GRAPH_LABEL, gCommonFormatArgs, COLOUR_BLACK, columnRight, y - 1);
		gfx_fill_rect(dpi, x + 6, y - 2, columnRight, y - 2, 10);

		x += N3DS_SUMMARY_COLUMN_WIDTH;
	}

	// The column on the right: loan and interest rate
	x = w->x + N3DS_SUMMARY_SIDE_LEFT;
	y = w->y + 47;
	gfx_draw_string_left(dpi, STR_FINANCES_SUMMARY_LOAN, NULL, COLOUR_BLACK, x, y);
	y += 11;
	set_format_arg(6, money32, gBankLoan);
	gfx_draw_string_left(dpi, STR_FINANCES_SUMMARY_LOAN_VALUE, gCommonFormatArgs, COLOUR_BLACK, x, y);
	y = w->y + N3DS_SUMMARY_LOAN_TOP + N3DS_CONTROL_HEIGHT + 3;
	set_format_arg(0, uint16, gBankLoanInterestRate);
	gfx_draw_string_left(dpi, STR_FINANCES_SUMMARY_AT_X_PER_YEAR, gCommonFormatArgs, COLOUR_BLACK, x - 3, y);
	y += 16;

	// Current cash
	money32 currentCash = DECRYPT_MONEY(gCashEncrypted);
	rct_string_id stringId = currentCash >= 0 ? STR_CASH_LABEL : STR_CASH_NEGATIVE_LABEL;
	y += gfx_draw_string_left_wrapped(dpi, &currentCash, x, y, N3DS_SUMMARY_SIDE_WIDTH, stringId, COLOUR_BLACK) + 3;

	// Objective related financial information
	if (gScenarioObjectiveType == OBJECTIVE_MONTHLY_FOOD_INCOME) {
		money32 lastMonthProfit = finance_get_last_month_shop_profit();
		set_format_arg(0, money32, lastMonthProfit);
		gfx_draw_string_left_wrapped(dpi, gCommonFormatArgs, x, y, N3DS_SUMMARY_SIDE_WIDTH, STR_LAST_MONTH_PROFIT_FROM_FOOD_DRINK_MERCHANDISE_SALES_LABEL, COLOUR_BLACK);
	} else {
		// Park value and company value
		y += gfx_draw_string_left_wrapped(dpi, &gParkValue, x, y, N3DS_SUMMARY_SIDE_WIDTH, STR_PARK_VALUE_LABEL, COLOUR_BLACK) + 3;
		gfx_draw_string_left_wrapped(dpi, &gCompanyValue, x, y, N3DS_SUMMARY_SIDE_WIDTH, STR_COMPANY_VALUE_LABEL, COLOUR_BLACK);
	}
}
#endif

/**
 *
 *  rct2: 0x0069C771
 */
static void window_finances_summary_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, j, x, y;

	window_draw_widgets(w, dpi);
	window_finances_draw_tab_images(dpi, w);

#ifdef __3DS__
	// n3ds port: laid out for the bottom screen; the rest of this function is the original's
	window_finances_summary_n3ds_paint(w, dpi);
	return;
#endif

	x = w->x + 8;
	y = w->y + 47;

	// Expenditure / Income heading
	draw_string_left_underline(dpi, STR_FINANCES_SUMMARY_EXPENDITURE_INCOME, NULL, COLOUR_BLACK, x, y - 1);
	y += 14;

	// Expenditure / Income row labels
	for (i = 0; i < 14; i++) {
		// Darken every even row
		if (i % 2 == 0)
			gfx_fill_rect(dpi, x, y, x + 513 - 2, y + 9, ColourMapA[w->colours[1]].lighter | 0x1000000);

		gfx_draw_string_left(dpi, window_finances_summary_row_labels[i], NULL, COLOUR_BLACK, x, y - 1);
		y += 10;
	}

	// Expenditure / Income values for each month
	x = w->x + 118;
	sint16 currentMonthYear = gDateMonthsElapsed;
	for (i = 4; i >= 0; i--) {
		y = w->y + 47;

		sint16 monthyear = currentMonthYear - i;
		if (monthyear < 0)
			continue;

		// Month heading
		set_format_arg(0, rct_string_id, STR_FINANCES_SUMMARY_MONTH_HEADING);
		set_format_arg(2, uint16, monthyear);
		draw_string_right_underline(
			dpi,
			monthyear == currentMonthYear ? STR_WINDOW_COLOUR_2_STRINGID : STR_BLACK_STRING,
			gCommonFormatArgs,
			COLOUR_BLACK,
			x + 80,
			y - 1
		);
		y += 14;

		// Month expenditures
		money32 profit = 0;
		money32 *expenditures = &gExpenditureTable[i * RCT_EXPENDITURE_TYPE_COUNT];
		for (j = 0; j < 14; j++) {
			money32 expenditure = expenditures[j];
			if (expenditure != 0) {
				profit += expenditure;
				gfx_draw_string_right(
					dpi,
					expenditure >= 0 ? STR_FINANCES_SUMMARY_INCOME_VALUE : STR_FINANCES_SUMMARY_EXPENDITURE_VALUE,
					&expenditure,
					COLOUR_BLACK,
					x + 80,
					y - 1
				);
			}
			y += 10;
		}
		y += 4;

		// Month profit
		gfx_draw_string_right(
			dpi,
			profit >= 0 ? STR_FINANCES_SUMMARY_INCOME_VALUE : STR_FINANCES_SUMMARY_LOSS_VALUE,
			&profit,
			COLOUR_BLACK,
			x + 80,
			y - 1
		);
		gfx_fill_rect(dpi, x + 10, y - 2, x + 10 + 70, y - 2, 10);

		x += 80;
	}


	// Horizontal rule below expenditure / income table
	gfx_fill_rect_inset(dpi, w->x + 8, w->y + 223, w->x + 8 + 513, w->y + 223 + 1, w->colours[1], INSET_RECT_FLAG_BORDER_INSET);

	// Loan and interest rate
	gfx_draw_string_left(dpi, STR_FINANCES_SUMMARY_LOAN, NULL, COLOUR_BLACK, w->x + 4, w->y + 229);
	set_format_arg(0, uint16, gBankLoanInterestRate);
	gfx_draw_string_left(dpi, STR_FINANCES_SUMMARY_AT_X_PER_YEAR, gCommonFormatArgs, COLOUR_BLACK, w->x + 156, w->y + 229);

	// Current cash
	money32 currentCash = DECRYPT_MONEY(gCashEncrypted);
	rct_string_id stringId = currentCash >= 0 ? STR_CASH_LABEL : STR_CASH_NEGATIVE_LABEL;
	gfx_draw_string_left(dpi, stringId, &currentCash, COLOUR_BLACK, w->x + 4, w->y + 244);

	// Objective related financial information
	if (gScenarioObjectiveType == OBJECTIVE_MONTHLY_FOOD_INCOME) {
		money32 lastMonthProfit = finance_get_last_month_shop_profit();
		set_format_arg(0, money32, lastMonthProfit);
		gfx_draw_string_left(dpi, STR_LAST_MONTH_PROFIT_FROM_FOOD_DRINK_MERCHANDISE_SALES_LABEL, gCommonFormatArgs, COLOUR_BLACK, w->x + 280, w->y + 229);
	} else {
		// Park value and company value
		gfx_draw_string_left(dpi, STR_PARK_VALUE_LABEL, &gParkValue, COLOUR_BLACK, w->x + 280, w->y + 229);
		gfx_draw_string_left(dpi, STR_COMPANY_VALUE_LABEL, &gCompanyValue, COLOUR_BLACK, w->x + 280, w->y + 244);
	}
}

#pragma endregion

#pragma region Financial graph page

/**
 *
 *  rct2: 0x0069CF70
 */
static void window_finances_financial_graph_mouseup(rct_window *w, int widgetIndex)
{
	if (widgetIndex == WIDX_CLOSE)
		window_close(w);
	else if (widgetIndex >= WIDX_TAB_1 && widgetIndex <= WIDX_TAB_6)
		window_finances_set_page(w, widgetIndex - WIDX_TAB_1);
}

/**
 *
 *  rct2: 0x0069CF8B
 */
static void window_finances_financial_graph_update(rct_window *w)
{
	// Tab animation
	if (++w->frame_no >= window_finances_tab_animation_loops[w->page])
		w->frame_no = 0;
	widget_invalidate(w, WIDX_TAB_2);
}

/**
 *
 *  rct2: 0x0069CBDB
 */
static void window_finances_financial_graph_invalidate(rct_window *w)
{
	colour_scheme_update(w);

	if (w->widgets != window_finances_page_widgets[WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH]) {
		w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH];
		window_init_scroll_widgets(w);
	}

	window_finances_set_pressed_tab(w);
}

/**
 *
 *  rct2: 0x0069CC10
 */
static void window_finances_financial_graph_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, x, y, graphLeft, graphTop, graphRight, graphBottom;

	window_draw_widgets(w, dpi);
	window_finances_draw_tab_images(dpi, w);

	rct_widget *pageWidget = &window_finances_cash_widgets[WIDX_PAGE_BACKGROUND];
	graphLeft = w->x + pageWidget->left + 4;
	graphTop = w->y + pageWidget->top + FINANCES_GRAPH_TOP;
	graphRight = w->x + pageWidget->right - 4;
	graphBottom = w->y + pageWidget->bottom - 4;

	// Cash (less loan)
	money32 cashLessLoan =
		DECRYPT_MONEY(gCashEncrypted) -
		gBankLoan;

	gfx_draw_string_left(
		dpi,
		cashLessLoan >= 0 ?
			STR_FINANCES_FINANCIAL_GRAPH_CASH_LESS_LOAN_POSITIVE : STR_FINANCES_FINANCIAL_GRAPH_CASH_LESS_LOAN_NEGATIVE,
		&cashLessLoan,
		COLOUR_BLACK,
		graphLeft,
		graphTop - 11
	);

	// Graph
	gfx_fill_rect_inset(dpi, graphLeft, graphTop, graphRight, graphBottom, w->colours[1], INSET_RECT_F_30);

	// Calculate the Y axis scale (log2 of highest [+/-]balance)
	int yAxisScale = 0;
	for (i = 0; i < 64; i++) {
		money32 balance = gCashHistory[i];
		if (balance == MONEY32_UNDEFINED)
			continue;

		// Modifier balance then keep halving until less than 127 pixels
		balance = abs(balance) >> yAxisScale;
		while (balance > 127) {
			balance /= 2;
			yAxisScale++;
		}
	}

	// Y axis labels
	x = graphLeft + FINANCES_GRAPH_AXIS_X;
	y = graphTop + FINANCES_GRAPH_AXIS_Y;
	money32 axisBase;
	for (axisBase = MONEY(12,00); axisBase >= MONEY(-12,00); axisBase -= MONEY(6,00)) {
		money32 axisValue = axisBase << yAxisScale;
		gfx_draw_string_right(dpi, STR_FINANCES_FINANCIAL_GRAPH_CASH_VALUE, &axisValue, COLOUR_BLACK, x + 70, y);
		y += 39;
	}

	// X axis labels and values
	x = graphLeft + FINANCES_GRAPH_PLOT_X;
	y = graphTop + FINANCES_GRAPH_PLOT_Y;
	graph_draw_money32(dpi, gCashHistory, FINANCES_GRAPH_POINTS, x, y, yAxisScale, 128);
}

#pragma endregion

#pragma region Value graph page

/**
 *
 *  rct2: 0x0069D338
 */
static void window_finances_park_value_graph_mouseup(rct_window *w, int widgetIndex)
{
	if (widgetIndex == WIDX_CLOSE)
		window_close(w);
	else if (widgetIndex >= WIDX_TAB_1 && widgetIndex <= WIDX_TAB_6)
		window_finances_set_page(w, widgetIndex - WIDX_TAB_1);
}

/**
 *
 *  rct2: 0x0069D353
 */
static void window_finances_park_value_graph_update(rct_window *w)
{
	// Tab animation
	if (++w->frame_no >= window_finances_tab_animation_loops[w->page])
		w->frame_no = 0;
	widget_invalidate(w, WIDX_TAB_2);
}

/**
 *
 *  rct2: 0x0069CFC0
 */
static void window_finances_park_value_graph_invalidate(rct_window *w)
{
	colour_scheme_update(w);

	if (w->widgets != window_finances_page_widgets[WINDOW_FINANCES_PAGE_VALUE_GRAPH]) {
		w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_VALUE_GRAPH];
		window_init_scroll_widgets(w);
	}

	window_finances_set_pressed_tab(w);
}

/**
 *
 *  rct2: 0x0069CFF5
 */
static void window_finances_park_value_graph_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, x, y, graphLeft, graphTop, graphRight, graphBottom;

	window_draw_widgets(w, dpi);
	window_finances_draw_tab_images(dpi, w);

	rct_widget *pageWidget = &window_finances_cash_widgets[WIDX_PAGE_BACKGROUND];
	graphLeft = w->x + pageWidget->left + 4;
	graphTop = w->y + pageWidget->top + FINANCES_GRAPH_TOP;
	graphRight = w->x + pageWidget->right - 4;
	graphBottom = w->y + pageWidget->bottom - 4;

	// Park value
	money32 parkValue = gParkValue;
	gfx_draw_string_left(
		dpi,
		STR_FINANCES_PARK_VALUE,
		&parkValue,
		COLOUR_BLACK,
		graphLeft,
		graphTop - 11
	);

	// Graph
	gfx_fill_rect_inset(dpi, graphLeft, graphTop, graphRight, graphBottom, w->colours[1], INSET_RECT_F_30);

	// Calculate the Y axis scale (log2 of highest [+/-]balance)
	int yAxisScale = 0;
	for (i = 0; i < 64; i++) {
		money32 balance = gParkValueHistory[i];
		if (balance == MONEY32_UNDEFINED)
			continue;

		// Modifier balance then keep halfing until less than 255 pixels
		balance = abs(balance) >> yAxisScale;
		while (balance > 255) {
			balance /= 2;
			yAxisScale++;
		}
	}

	// Y axis labels
	x = graphLeft + FINANCES_GRAPH_AXIS_X;
	y = graphTop + FINANCES_GRAPH_AXIS_Y;
	money32 axisBase;
	for (axisBase = MONEY(24,00); axisBase >= MONEY(0,00); axisBase -= MONEY(6,00)) {
		money32 axisValue = axisBase << yAxisScale;
		gfx_draw_string_right(dpi, STR_FINANCES_FINANCIAL_GRAPH_CASH_VALUE, &axisValue, COLOUR_BLACK, x + 70, y);
		y += 39;
	}

	// X axis labels and values
	x = graphLeft + FINANCES_GRAPH_PLOT_X;
	y = graphTop + FINANCES_GRAPH_PLOT_Y;
	graph_draw_money32(dpi, gParkValueHistory, FINANCES_GRAPH_POINTS, x, y, yAxisScale, 0);
}

#pragma endregion

#pragma region Profit graph page

/**
 *
 *  rct2: 0x0069D715
 */
static void window_finances_profit_graph_mouseup(rct_window *w, int widgetIndex)
{
	if (widgetIndex == WIDX_CLOSE)
		window_close(w);
	else if (widgetIndex >= WIDX_TAB_1 && widgetIndex <= WIDX_TAB_6)
		window_finances_set_page(w, widgetIndex - WIDX_TAB_1);
}

/**
 *
 *  rct2: 0x0069D730
 */
static void window_finances_profit_graph_update(rct_window *w)
{
	// Tab animation
	if (++w->frame_no >= window_finances_tab_animation_loops[w->page])
		w->frame_no = 0;
	widget_invalidate(w, WIDX_TAB_2);
}

/**
 *
 *  rct2: 0x0069D388
 */
static void window_finances_profit_graph_invalidate(rct_window *w)
{
	colour_scheme_update(w);

	if (w->widgets != window_finances_page_widgets[WINDOW_FINANCES_PAGE_PROFIT_GRAPH]) {
		w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_PROFIT_GRAPH];
		window_init_scroll_widgets(w);
	}

	window_finances_set_pressed_tab(w);
}

/**
 *
 *  rct2: 0x0069D3BD
 */
static void window_finances_profit_graph_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, x, y, graphLeft, graphTop, graphRight, graphBottom;

	window_draw_widgets(w, dpi);
	window_finances_draw_tab_images(dpi, w);

	rct_widget *pageWidget = &window_finances_cash_widgets[WIDX_PAGE_BACKGROUND];
	graphLeft = w->x + pageWidget->left + 4;
	graphTop = w->y + pageWidget->top + FINANCES_GRAPH_TOP;
	graphRight = w->x + pageWidget->right - 4;
	graphBottom = w->y + pageWidget->bottom - 4;

	// Weekly profit
	money32 weeklyPofit = gCurrentProfit;
	gfx_draw_string_left(
		dpi,
		weeklyPofit >= 0 ? STR_FINANCES_WEEKLY_PROFIT_POSITIVE : STR_FINANCES_WEEKLY_PROFIT_LOSS,
		&weeklyPofit,
		COLOUR_BLACK,
		graphLeft,
		graphTop - 11
	);

	// Graph
	gfx_fill_rect_inset(dpi, graphLeft, graphTop, graphRight, graphBottom, w->colours[1], INSET_RECT_F_30);

	// Calculate the Y axis scale (log2 of highest [+/-]balance)
	int yAxisScale = 0;
	for (i = 0; i < 64; i++) {
		money32 balance = gWeeklyProfitHistory[i];
		if (balance == MONEY32_UNDEFINED)
			continue;

		// Modifier balance then keep halfing until less than 127 pixels
		balance = abs(balance) >> yAxisScale;
		while (balance > 127) {
			balance /= 2;
			yAxisScale++;
		}
	}

	// Y axis labels
	x = graphLeft + FINANCES_GRAPH_AXIS_X;
	y = graphTop + FINANCES_GRAPH_AXIS_Y;
	money32 axisBase;
	for (axisBase = MONEY(12,00); axisBase >= MONEY(-12,00); axisBase -= MONEY(6,00)) {
		money32 axisValue = axisBase << yAxisScale;
		gfx_draw_string_right(dpi, STR_FINANCES_FINANCIAL_GRAPH_CASH_VALUE, &axisValue, COLOUR_BLACK, x + 70, y);
		y += 39;
	}

	// X axis labels and values
	x = graphLeft + FINANCES_GRAPH_PLOT_X;
	y = graphTop + FINANCES_GRAPH_PLOT_Y;
	graph_draw_money32(dpi, gWeeklyProfitHistory, FINANCES_GRAPH_POINTS, x, y, yAxisScale, 128);
}

#pragma endregion

#pragma region Marketing page

/**
 *
 *  rct2: 0x0069D9F9
 */
static void window_finances_marketing_mouseup(rct_window *w, int widgetIndex)
{
	if (widgetIndex == WIDX_CLOSE)
		window_close(w);
	else if (widgetIndex >= WIDX_TAB_1 && widgetIndex <= WIDX_TAB_6)
		window_finances_set_page(w, widgetIndex - WIDX_TAB_1);
	else if (widgetIndex >= WIDX_CAMPAIGN_1 && widgetIndex <= WIDX_CAMPAIGN_6)
		window_new_campaign_open(widgetIndex - WIDX_CAMPAIGN_1);

}

/**
 *
 *  rct2: 0x0069DA2F
 */
static void window_finances_marketing_update(rct_window *w)
{
	// Tab animation
	if (++w->frame_no >= window_finances_tab_animation_loops[w->page])
		w->frame_no = 0;
	widget_invalidate(w, WIDX_TAB_5);
}

/**
 *
 *  rct2: 0x0069D765
 */
static void window_finances_marketing_invalidate(rct_window *w)
{
	int i;

	colour_scheme_update(w);

	if (w->widgets != window_finances_page_widgets[WINDOW_FINANCES_PAGE_MARKETING]) {
		w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_MARKETING];
		window_init_scroll_widgets(w);
	}

	window_finances_set_pressed_tab(w);

	// Count number of active campaigns
	int numActiveCampaigns = 0;
	for (i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++)
		if (gMarketingCampaignDaysLeft[i] != 0)
			numActiveCampaigns++;

	int y = max(1, numActiveCampaigns) * 10 + 92;

	// Update group box positions
	window_finances_marketing_widgets[WIDX_ACITVE_CAMPAGINS_GROUP].bottom = y - 20;
	window_finances_marketing_widgets[WIDX_CAMPAGINS_AVAILABLE_GROUP].top = y - 13;

	// Update new campagin button visibility
	for (i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++) {
		rct_widget *campaginButton = &window_finances_marketing_widgets[WIDX_CAMPAIGN_1 + i];

		campaginButton->type = WWT_EMPTY;

		if (gMarketingCampaignDaysLeft[i] != 0)
			continue;

		if (!marketing_is_campaign_type_applicable(i))
			continue;

		campaginButton->type = WWT_DROPDOWN_BUTTON;
		campaginButton->top = y;
#ifdef __3DS__
		// n3ds port: buttons of a height for a finger
		campaginButton->bottom = y + N3DS_CONTROL_HEIGHT - 1;
		y += N3DS_CONTROL_HEIGHT + 2;
#else
		campaginButton->bottom = y + 11;
		y += 12;
#endif
	}
}

/**
 *
 *  rct2: 0x0069D834
 */
static void window_finances_marketing_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	int i, x, y, weeksRemaining;
	rct_ride *ride;

	window_draw_widgets(w, dpi);
	window_finances_draw_tab_images(dpi, w);

	x = w->x + 8;
	y = w->y + 62;

	int noCampaignsActive = 1;
	for (i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++) {
		if (gMarketingCampaignDaysLeft[i] == 0)
			continue;

		noCampaignsActive = 0;
		set_format_arg(0, rct_string_id, gParkName);
		set_format_arg(2, uint32, gParkNameArgs);

		// Set special parameters
		switch (i) {
		case ADVERTISING_CAMPAIGN_RIDE_FREE:
		case ADVERTISING_CAMPAIGN_RIDE:
			ride = get_ride(gMarketingCampaignRideIndex[i]);
			set_format_arg(0, rct_string_id, ride->name);
			set_format_arg(2, uint32, ride->name_arguments);
			break;
		case ADVERTISING_CAMPAIGN_FOOD_OR_DRINK_FREE:
			set_format_arg(0, rct_string_id, ShopItemStringIds[gMarketingCampaignRideIndex[i]].plural);
			break;
		}

#ifdef __3DS__
		// n3ds port: in the small font (see window_finances_summary_n3ds_paint), the duration
		// at the right edge: in the medium font the two do not fit beside one another in 320
		uint8 smallArgs[8];
		rct_string_id smallFormat = MarketingCampaignNames[i][1];
		memcpy(smallArgs, &smallFormat, 2);
		memcpy(smallArgs + 2, gCommonFormatArgs, 6);
		gfx_draw_string_left_clipped(dpi, STR_GRAPH_LABEL, smallArgs, COLOUR_BLACK, x + 4, y, 204);

		weeksRemaining = (gMarketingCampaignDaysLeft[i] % 128);
		smallFormat = weeksRemaining == 1 ? STR_1_WEEK_REMAINING : STR_X_WEEKS_REMAINING;
		memcpy(smallArgs, &smallFormat, 2);
		memcpy(smallArgs + 2, &weeksRemaining, 2);
		gfx_draw_string_right(dpi, STR_GRAPH_LABEL, smallArgs, COLOUR_BLACK, w->x + N3DS_BOTTOM_WIDTH - 9, y);
#else
		// Advertisement
		gfx_draw_string_left_clipped(dpi, MarketingCampaignNames[i][1], gCommonFormatArgs, COLOUR_BLACK, x + 4, y, 296);

		// Duration
		weeksRemaining = (gMarketingCampaignDaysLeft[i] % 128);
		gfx_draw_string_left(dpi, weeksRemaining == 1 ? STR_1_WEEK_REMAINING : STR_X_WEEKS_REMAINING, &weeksRemaining, COLOUR_BLACK, x + 304, y);
#endif

		y += 10;
	}

	if (noCampaignsActive) {
		gfx_draw_string_left(dpi, STR_MARKETING_CAMPAGINS_NONE, NULL, COLOUR_BLACK, x + 4, y);
		y += 10;
	}
	y += 31;

	// Draw campaign button text
	for (i = 0; i < ADVERTISING_CAMPAIGN_COUNT; i++) {
		rct_widget *campaginButton = &window_finances_marketing_widgets[WIDX_CAMPAIGN_1 + i];

		if (campaginButton->type == WWT_EMPTY)
			continue;

		money32 pricePerWeek = AdvertisingCampaignPricePerWeek[i];

#ifdef __3DS__
		// n3ds port: in the small font, in the middle of the higher button, the price at the
		// button's right end
		uint8 smallArgs[8];
		rct_string_id smallFormat = MarketingCampaignNames[i][0];
		y = w->y + campaginButton->top + N3DS_CONTROL_TEXT_OFFSET;
		gfx_draw_string_left(dpi, STR_GRAPH_LABEL, &smallFormat, COLOUR_BLACK, x + 4, y);
		smallFormat = STR_MARKETING_PER_WEEK;
		memcpy(smallArgs, &smallFormat, 2);
		memcpy(smallArgs + 2, &pricePerWeek, 4);
		gfx_draw_string_right(dpi, STR_GRAPH_LABEL, smallArgs, COLOUR_BLACK, w->x + campaginButton->right - 4, y);
#else
		// Draw button text
		gfx_draw_string_left(dpi, MarketingCampaignNames[i][0], NULL, COLOUR_BLACK, x + 4, y - 1);
		gfx_draw_string_left(dpi, STR_MARKETING_PER_WEEK, &pricePerWeek, COLOUR_BLACK, x + 310, y - 1);

		y += 12;
#endif
	}
}

#pragma endregion

#pragma region Research page

/**
 *
 *  rct2: 0x0069DB3F
 */
static void window_finances_research_mouseup(rct_window *w, int widgetIndex)
{
	int activeResearchTypes;

	switch (widgetIndex) {
	case WIDX_CLOSE:
		window_close(w);
		break;
	case WIDX_TAB_1:
	case WIDX_TAB_2:
	case WIDX_TAB_3:
	case WIDX_TAB_4:
	case WIDX_TAB_5:
	case WIDX_TAB_6:
		window_finances_set_page(w, widgetIndex - WIDX_TAB_1);
		break;
	case WIDX_TRANSPORT_RIDES:
	case WIDX_GENTLE_RIDES:
	case WIDX_ROLLER_COASTERS:
	case WIDX_THRILL_RIDES:
	case WIDX_WATER_RIDES:
	case WIDX_SHOPS_AND_STALLS:
	case WIDX_SCENERY_AND_THEMING:
		activeResearchTypes = gResearchPriorities;
		activeResearchTypes ^= 1 << (widgetIndex - WIDX_TRANSPORT_RIDES);
		research_set_priority(activeResearchTypes);
		break;
	}
}

/**
 *
 *  rct2: 0x0069DB66
 */
static void window_finances_research_mousedown(int widgetIndex, rct_window *w, rct_widget* widget)
{
	rct_widget *dropdownWidget;
	int i;

	if (widgetIndex != WIDX_RESEARCH_FUNDING_DROPDOWN_BUTTON)
		return;

	dropdownWidget = widget - 1;

	for (i = 0; i < 4; i++) {
		gDropdownItemsFormat[i] = STR_DROPDOWN_MENU_LABEL;
		gDropdownItemsArgs[i] = ResearchFundingLevelNames[i];
	}
	window_dropdown_show_text_custom_width(
		w->x + dropdownWidget->left,
		w->y + dropdownWidget->top,
		dropdownWidget->bottom - dropdownWidget->top + 1,
		w->colours[1],
		DROPDOWN_FLAG_STAY_OPEN,
		4,
		dropdownWidget->right - dropdownWidget->left - 3
	);

	int currentResearchLevel = gResearchFundingLevel;
	dropdown_set_checked(currentResearchLevel, true);
}

/**
 *
 *  rct2: 0x0069DB6D
 */
static void window_finances_research_dropdown(rct_window *w, int widgetIndex, int dropdownIndex)
{
	if (widgetIndex != WIDX_RESEARCH_FUNDING_DROPDOWN_BUTTON || dropdownIndex == -1)
		return;

	research_set_funding(dropdownIndex);
}

/**
 *
 *  rct2: 0x0069DC23
 */
static void window_finances_research_update(rct_window *w)
{
	// Tab animation
	if (++w->frame_no >= window_finances_tab_animation_loops[w->page])
		w->frame_no = 0;
	widget_invalidate(w, WIDX_TAB_6);
}

/**
 *
 *  rct2: 0x0069DA64
 */
static void window_finances_research_invalidate(rct_window *w)
{
	colour_scheme_update(w);

	if (w->widgets != window_finances_page_widgets[WINDOW_FINANCES_PAGE_RESEARCH]) {
		w->widgets = window_finances_page_widgets[WINDOW_FINANCES_PAGE_RESEARCH];
		window_init_scroll_widgets(w);
	}

	window_finances_set_pressed_tab(w);
	if (gResearchProgressStage == RESEARCH_STAGE_FINISHED_ALL) {
		window_finances_research_widgets[WIDX_RESEARCH_FUNDING].type = WWT_EMPTY;
		window_finances_research_widgets[WIDX_RESEARCH_FUNDING_DROPDOWN_BUTTON].type = WWT_EMPTY;
	}
	int currentResearchLevel = gResearchFundingLevel;

	// Current funding
	window_finances_research_widgets[WIDX_RESEARCH_FUNDING].text = ResearchFundingLevelNames[currentResearchLevel];

	// Checkboxes
	uint8 activeResearchTypes = gResearchPriorities;
	int uncompletedResearchTypes = gResearchUncompletedCategories;
	for (int i = 0; i < 7; i++) {
		int mask = 1 << i;
		int widgetMask = 1 << (i + WIDX_TRANSPORT_RIDES);

		// Set checkbox disabled if research type is complete
		if (uncompletedResearchTypes & mask) {
			w->disabled_widgets &= ~widgetMask;

			// Set checkbox ticked if research type is active
			if (activeResearchTypes & mask)
				w->pressed_widgets |= widgetMask;
			else
				w->pressed_widgets &= ~widgetMask;
		} else {
			w->disabled_widgets |= widgetMask;
			w->pressed_widgets &= ~widgetMask;
		}
	}
}

/**
 *
 *  rct2: 0x0069DAF0
 */
static void window_finances_research_paint(rct_window *w, rct_drawpixelinfo *dpi)
{
	window_draw_widgets(w, dpi);
	window_finances_draw_tab_images(dpi, w);

	window_research_funding_page_paint(w, dpi, WIDX_RESEARCH_FUNDING);
}

#pragma endregion

#pragma region Common

/**
 *
 *  rct2: 0x0069CAC5
 */
static void window_finances_set_page(rct_window *w, int page)
{
	w->page = page;
	w->frame_no = 0;
	if (w->viewport != NULL) {
		w->viewport->width = 0;
		w->viewport = NULL;
	}

	w->enabled_widgets = window_finances_page_enabled_widgets[page];
	w->hold_down_widgets = window_finances_page_hold_down_widgets[page];
	w->event_handlers = window_finances_page_events[page];
	w->widgets = window_finances_page_widgets[page];
	w->disabled_widgets = 0;
	w->pressed_widgets = 0;

	window_invalidate(w);
#ifdef __3DS__
	// n3ds port: every page fills the bottom screen (window_finances_n3ds_layout)
	w->width = N3DS_BOTTOM_WIDTH;
	w->height = N3DS_BOTTOM_HEIGHT;
#else
	if (w->page == WINDOW_FINANCES_PAGE_RESEARCH) {
		w->width = 320;
		w->height = 207;
	} else {
		w->width = 530;
		w->height = 257;
	}
#endif
	window_event_resize_call(w);
	window_event_invalidate_call(w);

	window_init_scroll_widgets(w);
	window_invalidate(w);
}

static void window_finances_set_pressed_tab(rct_window *w)
{
	int i;
	for (i = 0; i < WINDOW_FINANCES_PAGE_COUNT; i++)
		w->pressed_widgets &= ~(1 << (WIDX_TAB_1 + i));
	w->pressed_widgets |= 1LL << (WIDX_TAB_1 + w->page);
}

static void window_finances_draw_tab_image(rct_drawpixelinfo *dpi, rct_window *w, int page, int spriteIndex)
{
	int widgetIndex = WIDX_TAB_1 + page;

	if (!(w->disabled_widgets & (1LL << widgetIndex))) {
		if (w->page == page) {
			int frame = w->frame_no / 2;
			if (page == WINDOW_FINANCES_PAGE_SUMMARY)
				frame %= 8;
			spriteIndex += frame;
		}

		gfx_draw_sprite(dpi, spriteIndex, w->x + w->widgets[widgetIndex].left, w->y + w->widgets[widgetIndex].top, 0);
	}
}

static void window_finances_draw_tab_images(rct_drawpixelinfo *dpi, rct_window *w)
{
	window_finances_draw_tab_image(dpi, w, WINDOW_FINANCES_PAGE_SUMMARY, SPR_TAB_FINANCES_SUMMARY_0);
	window_finances_draw_tab_image(dpi, w, WINDOW_FINANCES_PAGE_FINANCIAL_GRAPH, SPR_TAB_FINANCES_FINANCIAL_GRAPH_0);
	window_finances_draw_tab_image(dpi, w, WINDOW_FINANCES_PAGE_VALUE_GRAPH, SPR_TAB_FINANCES_VALUE_GRAPH_0);
	window_finances_draw_tab_image(dpi, w, WINDOW_FINANCES_PAGE_PROFIT_GRAPH, SPR_TAB_FINANCES_PROFIT_GRAPH_0);
	window_finances_draw_tab_image(dpi, w, WINDOW_FINANCES_PAGE_MARKETING, SPR_TAB_FINANCES_MARKETING_0);
	window_finances_draw_tab_image(dpi, w, WINDOW_FINANCES_PAGE_RESEARCH, SPR_TAB_FINANCES_RESEARCH_0);
}

#pragma endregion
