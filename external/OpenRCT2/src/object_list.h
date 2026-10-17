#pragma once

#include "ride/ride.h"
#include "world/banner.h"
#include "world/entrance.h"
#include "world/footpath.h"
#include "world/scenery.h"
#include "world/water.h"

	extern rct_ride_entry				*gRideEntries[128];
	extern rct_small_scenery_entry		*gSmallSceneryEntries[252];
	extern rct_large_scenery_entry		*gLargeSceneryEntries[128];
	extern rct_wall_scenery_entry		*gWallSceneryEntries[128];
	extern rct_banner					*gBannerSceneryEntries[32];
	extern rct_footpath_entry			*gFootpathEntries[16];
	extern rct_path_bit_scenery_entry	*gFootpathAdditionEntries[15];
	extern rct_scenery_set_entry		*gSceneryGroupEntries[19];
	extern rct_entrance_type			*gParkEntranceEntries[1];
	extern rct_water_type				*gWaterEntries[1];
	extern rct_stex_entry				*gStexEntries[1];

void get_type_entry_index(size_t index, uint8 * outObjectType, uint8 * outEntryIndex);
const rct_object_entry * get_loaded_object_entry(size_t index);
void * get_loaded_object_chunk(size_t index);
