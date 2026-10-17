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

#include <new>
#include "../core/Exception.hpp"
#include "../core/IStream.hpp"
#include "../core/String.hpp"
#include "../object/ObjectRepository.h"
#include "../object/Object.h"
#include "S6Exporter.h"

extern "C"
{
    #include "../config.h"
    #include "../game.h"
    #include "../interface/viewport.h"
    #include "../interface/window.h"
    #include "../localisation/date.h"
    #include "../localisation/localisation.h"
    #include "../management/finance.h"
    #include "../management/marketing.h"
    #include "../management/news_item.h"
    #include "../management/research.h"
    #include "../object.h"
    #include "../OpenRCT2.h"
    #include "../peep/staff.h"
    #include "../platform/platform.h"
    #include "../rct2.h"
    #include "../ride/ride.h"
    #include "../ride/ride_ratings.h"
    #include "../scenario/scenario.h"
    #include "../util/sawyercoding.h"
    #include "../util/util.h"
    #include "../world/climate.h"
    #include "../world/map_animation.h"
    #include "../world/park.h"
    #include "../world/sprite.h"
}

S6Exporter::S6Exporter()
{
    RemoveTracklessRides = false;
    memset(&_s6, 0, sizeof(_s6));
}

void * S6Exporter::operator new(size_t size)
{
    void * memory = platform_n3ds_temp_alloc(size);
    if (memory == nullptr)
    {
        throw std::bad_alloc();
    }
    return memory;
}

void S6Exporter::operator delete(void * memory)
{
    platform_n3ds_temp_free(memory);
}

// The file is written to a buffer in memory first, and read back to add up its checksum
// n3ds port: from the linear heap where possible (platform_n3ds_temp_alloc in n3ds.c)
#define FILE_BUFFER_ALLOC(size) platform_n3ds_temp_alloc(size)
#define FILE_BUFFER_FREE(memory) platform_n3ds_temp_free(memory)

void S6Exporter::SaveGame(const utf8 * path)
{
    SDL_RWops * rw = SDL_RWFromFile(path, "rb");
    if (rw == nullptr)
    {
        throw IOException("Unable to write to destination file.");
    }

    SaveGame(rw);

    SDL_RWclose(rw);
}

void S6Exporter::SaveGame(SDL_RWops *rw)
{
    Save(rw, false);
}

void S6Exporter::SaveScenario(const utf8 * path)
{
    SDL_RWops * rw = SDL_RWFromFile(path, "rb");
    if (rw == nullptr)
    {
        throw IOException("Unable to write to destination file.");
    }

    SaveGame(rw);

    SDL_RWclose(rw);
}

void S6Exporter::SaveScenario(SDL_RWops *rw)
{
    Save(rw, true);
}

void S6Exporter::Save(SDL_RWops * rw, bool isScenario)
{
    _s6.header.type = isScenario ? S6_TYPE_SCENARIO : S6_TYPE_SAVEDGAME;
    _s6.header.classic_flag = 0;
    _s6.header.num_packed_objects = uint16(ExportObjectsList.size());
    _s6.header.version = S6_RCT2_VERSION;
    _s6.header.magic_number = S6_MAGIC_NUMBER;

    _s6.game_version_number = 201028;

    uint8 * buffer = (uint8 *)FILE_BUFFER_ALLOC(0x600000);
    if (buffer == NULL)
    {
        log_error("Unable to allocate enough space for a write buffer.");
        throw Exception("Unable to allocate memory.");
    }

    sawyercoding_chunk_header chunkHeader;
    size_t encodedLength;

    // 0: Write header chunk
    chunkHeader.encoding = CHUNK_ENCODING_ROTATE;
    chunkHeader.length = sizeof(rct_s6_header);
    encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.header, chunkHeader);
    SDL_RWwrite(rw, buffer, encodedLength, 1);

    // 1: Write scenario info chunk
    if (_s6.header.type == S6_TYPE_SCENARIO)
    {
        chunkHeader.encoding = CHUNK_ENCODING_ROTATE;
        chunkHeader.length = sizeof(rct_s6_info);
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.info, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);
    }

    log_warning("exporting %u objects", _s6.header.num_packed_objects);
    // 2: Write packed objects
    if (_s6.header.num_packed_objects > 0)
    {
        if (!scenario_write_packed_objects(rw, ExportObjectsList))
        {
            FILE_BUFFER_FREE(buffer);
            throw Exception("Unable to pack objects.");
        }
    }

    // 3: Write available objects chunk
    chunkHeader.encoding = CHUNK_ENCODING_ROTATE;
    chunkHeader.length = OBJECT_ENTRY_COUNT * sizeof(rct_object_entry);
    encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)_s6.objects, chunkHeader);
    SDL_RWwrite(rw, buffer, encodedLength, 1);

    // 4: Misc fields (data, rand...) chunk
    chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
    chunkHeader.length = 16;
    encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.elapsed_months, chunkHeader);
    SDL_RWwrite(rw, buffer, encodedLength, 1);

    // 5: Map elements + sprites and other fields chunk
    chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
    chunkHeader.length = 0x180000;
    encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)_s6.map_elements, chunkHeader);
    SDL_RWwrite(rw, buffer, encodedLength, 1);

    if (_s6.header.type == S6_TYPE_SCENARIO)
    {
        // 6:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 0x27104C;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.next_free_map_element_pointer_index, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 7:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 4;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.guests_in_park, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 8:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 8;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.last_guests_in_park, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 9:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 2;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.park_rating, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 10:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 1082;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.active_research_types, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 11:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 16;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.current_expenditure, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 12:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 4;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.park_value, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);

        // 13:
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 0x761E8;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.completed_company_value, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);
    }
    else
    {
        // 6: Everything else...
        chunkHeader.encoding = CHUNK_ENCODING_RLECOMPRESSED;
        chunkHeader.length = 0x2E8570;
        encodedLength = sawyercoding_write_chunk_buffer(buffer, (uint8*)&_s6.next_free_map_element_pointer_index, chunkHeader);
        SDL_RWwrite(rw, buffer, encodedLength, 1);
    }

    FILE_BUFFER_FREE(buffer);

    // Determine number of bytes written
    size_t fileSize = (size_t)SDL_RWtell(rw);
    SDL_RWseek(rw, 0, RW_SEEK_SET);

    // Read all written bytes back into a single buffer
    buffer = (uint8 *)FILE_BUFFER_ALLOC(fileSize);
    SDL_RWread(rw, buffer, fileSize, 1);
    uint32 checksum = sawyercoding_calculate_checksum(buffer, fileSize);
    FILE_BUFFER_FREE(buffer);

    // Append the checksum
    SDL_RWseek(rw, fileSize, RW_SEEK_SET);
    SDL_RWwrite(rw, &checksum, sizeof(uint32), 1);
}

// n3ds port: a ride that is being simulated (RIDE_STATUS_SIMULATING, a status OpenRCT2 added in 2019;
// ride.h) is saved as closed and without its ghost trains, so that the original game can open the
// file (CLAUDE.md "코드 규칙": 파일 형식 불변). Done on the copy: the simulation goes on in the game.
// Each step mirrors what closing the ride does in the game (sprite_remove, ride_clear_for_construction,
// game_command_set_ride_status for RIDE_STATUS_CLOSED), so that the file is the one closing the ride
// and saving would write.
static void N3dsRemoveSpriteFromCopy(rct_s6_data * s6, uint16 spriteIndex)
{
    rct_unk_sprite * sprite = &s6->sprites[spriteIndex].unknown;
    // as move_sprite_to_list(sprite, SPRITE_LIST_NULL * 2)
    int oldList = sprite->linked_list_type_offset >> 1;
    if (sprite->previous == SPRITE_INDEX_NULL)
    {
        s6->sprite_lists_head[oldList] = sprite->next;
    }
    else
    {
        s6->sprites[sprite->previous].unknown.next = sprite->next;
    }
    if (sprite->next != SPRITE_INDEX_NULL)
    {
        s6->sprites[sprite->next].unknown.previous = sprite->previous;
    }
    sprite->previous = SPRITE_INDEX_NULL;
    sprite->linked_list_type_offset = SPRITE_LIST_NULL * 2;
    sprite->next = s6->sprite_lists_head[SPRITE_LIST_NULL];
    s6->sprite_lists_head[SPRITE_LIST_NULL] = spriteIndex;
    if (sprite->next != SPRITE_INDEX_NULL)
    {
        s6->sprites[sprite->next].unknown.previous = spriteIndex;
    }
    s6->sprite_lists_count[oldList]--;
    s6->sprite_lists_count[SPRITE_LIST_NULL]++;
    // as the rest of sprite_remove: out of its quadrant's chain (the chains' heads are not in the file,
    // they are rebuilt on load, so the sprite before it is found by searching), and the identifier
    for (int i = 0; i < MAX_SPRITES; i++)
    {
        rct_unk_sprite * other = &s6->sprites[i].unknown;
        if (other->sprite_identifier != SPRITE_IDENTIFIER_NULL && other->next_in_quadrant == spriteIndex)
        {
            other->next_in_quadrant = sprite->next_in_quadrant;
            break;
        }
    }
    sprite->sprite_identifier = SPRITE_IDENTIFIER_NULL;
    // and as sprite_clear_all_unused, which scenario_save ran on the game's own null sprites just before
    // this copy was taken
    uint16 next = sprite->next;
    uint16 previous = sprite->previous;
    memset(sprite, 0, sizeof(rct_sprite));
    sprite->sprite_identifier = SPRITE_IDENTIFIER_NULL;
    sprite->next = next;
    sprite->previous = previous;
    sprite->linked_list_type_offset = SPRITE_LIST_NULL * 2;
    sprite->sprite_index = spriteIndex;
}

static void N3dsRemoveTrainFromCopy(rct_s6_data * s6, uint16 spriteIndex)
{
    while (spriteIndex != SPRITE_INDEX_NULL)
    {
        uint16 next = s6->sprites[spriteIndex].vehicle.next_vehicle_on_train;
        N3dsRemoveSpriteFromCopy(s6, spriteIndex);
        spriteIndex = next;
    }
}

static void N3dsSaveSimulatingRidesAsClosed(rct_s6_data * s6)
{
    for (int rideIndex = 0; rideIndex < MAX_RIDES; rideIndex++)
    {
        rct_ride * ride = &s6->rides[rideIndex];
        if (ride->type == RIDE_TYPE_NULL || ride->status != RIDE_STATUS_SIMULATING)
        {
            continue;
        }
        // as game_command_set_ride_status, RIDE_STATUS_CLOSED, for a ride that is not broken down:
        ride->lifecycle_flags &= ~RIDE_LIFECYCLE_CRASHED;
        // ride_clear_for_construction: ride_measurement_clear, the breakdown flags,
        if (ride->measurement_index != 255)
        {
            s6->ride_measurements[ride->measurement_index].ride_index = 255;
            ride->measurement_index = 255;
        }
        ride->lifecycle_flags &= ~(RIDE_LIFECYCLE_BREAKDOWN_PENDING | RIDE_LIFECYCLE_BROKEN_DOWN);
        ride->window_invalidate_flags |= RIDE_INVALIDATE_RIDE_MAIN | RIDE_INVALIDATE_RIDE_LIST;
        // ride_remove_cable_lift (the ride's cable_lift index stays, as there),
        if (ride->lifecycle_flags & RIDE_LIFECYCLE_CABLE_LIFT)
        {
            ride->lifecycle_flags &= ~RIDE_LIFECYCLE_CABLE_LIFT;
            N3dsRemoveTrainFromCopy(s6, ride->cable_lift);
        }
        // ride_remove_vehicles,
        if (ride->lifecycle_flags & RIDE_LIFECYCLE_ON_TRACK)
        {
            ride->lifecycle_flags &= ~(RIDE_LIFECYCLE_ON_TRACK | RIDE_LIFECYCLE_TEST_IN_PROGRESS | RIDE_LIFECYCLE_11);
            for (int i = 0; i < 32; i++)
            {
                N3dsRemoveTrainFromCopy(s6, ride->vehicles[i]);
                ride->vehicles[i] = SPRITE_INDEX_NULL;
            }
            for (int i = 0; i < 4; i++)
            {
                ride->train_at_station[i] = 255;
            }
        }
        // ride_remove_peeps (no guest is queuing for, entering or on a simulated ride: nothing to move),
        ride->num_riders = 0;
        ride->slide_in_use = 0;
        // and the status itself
        ride->status = RIDE_STATUS_CLOSED;
        ride->lifecycle_flags &= ~RIDE_LIFECYCLE_PASS_STATION_NO_STOPPING;
        ride->race_winner = SPRITE_INDEX_NULL;
    }
}

void S6Exporter::Export()
{
    _s6.info = gS6Info;

    for (int i = 0; i < OBJECT_ENTRY_COUNT; i++)
    {
        const rct_object_entry * entry = get_loaded_object_entry(i);
        void * entryData = get_loaded_object_chunk(i);
        if (entryData == (void *)-1)
        {
            Memory::Set(&_s6.objects[i], 0xFF, sizeof(rct_object_entry));
        }
        else
        {
            _s6.objects[i] = *((rct_object_entry*)entry);
        }
    }

    _s6.elapsed_months = gDateMonthsElapsed;
    _s6.current_day = gDateMonthTicks;
    _s6.scenario_ticks = gScenarioTicks;
    _s6.scenario_srand_0 = gScenarioSrand0;
    _s6.scenario_srand_1 = gScenarioSrand1;

    memcpy(_s6.map_elements, gMapElements, sizeof(_s6.map_elements));

    _s6.next_free_map_element_pointer_index = gNextFreeMapElementPointerIndex;
    for (int i = 0; i < MAX_SPRITES; i++)
    {
        memcpy(&_s6.sprites[i], get_sprite(i), sizeof(rct_sprite));
    }

    for (int i = 0; i < NUM_SPRITE_LISTS; i++)
    {
        _s6.sprite_lists_head[i] = gSpriteListHead[i];
        _s6.sprite_lists_count[i] = gSpriteListCount[i];
    }
    _s6.park_name = gParkName;
    // pad_013573D6
    _s6.park_name_args = gParkNameArgs;
    _s6.initial_cash = gInitialCash;
    _s6.current_loan = gBankLoan;
    _s6.park_flags = gParkFlags;
    _s6.park_entrance_fee = gParkEntranceFee;
    // rct1_park_entrance_x
    // rct1_park_entrance_y
    // pad_013573EE
    // rct1_park_entrance_z
    memcpy(_s6.peep_spawns, gPeepSpawns, sizeof(_s6.peep_spawns));
    _s6.guest_count_change_modifier = gGuestChangeModifier;
    _s6.current_research_level = gResearchFundingLevel;
    // pad_01357400
    memcpy(_s6.researched_ride_types, gResearchedRideTypes, sizeof(_s6.researched_ride_types));
    memcpy(_s6.researched_ride_entries, gResearchedRideEntries, sizeof(_s6.researched_ride_entries));
    memcpy(_s6.researched_track_types_a, gResearchedTrackTypesA, sizeof(_s6.researched_track_types_a));
    memcpy(_s6.researched_track_types_b, gResearchedTrackTypesB, sizeof(_s6.researched_track_types_b));

    _s6.guests_in_park = gNumGuestsInPark;
    _s6.guests_heading_for_park = gNumGuestsHeadingForPark;

    memcpy(_s6.expenditure_table, gExpenditureTable, sizeof(_s6.expenditure_table));

    _s6.last_guests_in_park = gNumGuestsInParkLastWeek;
    // pad_01357BCA
    _s6.handyman_colour = gStaffHandymanColour;
    _s6.mechanic_colour = gStaffMechanicColour;
    _s6.security_colour = gStaffSecurityColour;

    memcpy(_s6.researched_scenery_items, gResearchedSceneryItems, sizeof(_s6.researched_scenery_items));

    _s6.park_rating = gParkRating;

    memcpy(_s6.park_rating_history, gParkRatingHistory, sizeof(_s6.park_rating_history));
    memcpy(_s6.guests_in_park_history, gGuestsInParkHistory, sizeof(_s6.guests_in_park_history));

    _s6.active_research_types = gResearchPriorities;
    _s6.research_progress_stage = gResearchProgressStage;
    _s6.last_researched_item_subject = gResearchLastItemSubject;
    // pad_01357CF8
    _s6.next_research_item = gResearchNextItem;
    _s6.research_progress = gResearchProgress;
    _s6.next_research_category = gResearchNextCategory;
    _s6.next_research_expected_day = gResearchExpectedDay;
    _s6.next_research_expected_month = gResearchExpectedMonth;
    _s6.guest_initial_happiness = gGuestInitialHappiness;
    _s6.park_size = gParkSize;
    _s6.guest_generation_probability = _guestGenerationProbability;
    _s6.total_ride_value = gTotalRideValue;
    _s6.maximum_loan = gMaxBankLoan;
    _s6.guest_initial_cash = gGuestInitialCash;
    _s6.guest_initial_hunger = gGuestInitialHunger;
    _s6.guest_initial_thirst = gGuestInitialThirst;
    _s6.objective_type = gScenarioObjectiveType;
    _s6.objective_year = gScenarioObjectiveYear;
    // pad_013580FA
    _s6.objective_currency = gScenarioObjectiveCurrency;
    _s6.objective_guests = gScenarioObjectiveNumGuests;
    memcpy(_s6.campaign_weeks_left, gMarketingCampaignDaysLeft, sizeof(_s6.campaign_weeks_left));
    memcpy(_s6.campaign_ride_index, gMarketingCampaignRideIndex, sizeof(_s6.campaign_ride_index));

    memcpy(_s6.balance_history, gCashHistory, sizeof(_s6.balance_history));

    _s6.current_expenditure = gCurrentExpenditure;
    _s6.current_profit = gCurrentProfit;
    _s6.weekly_profit_average_dividend = gWeeklyProfitAverageDividend;
    _s6.weekly_profit_average_divisor = gWeeklyProfitAverageDivisor;
    // pad_0135833A

    memcpy(_s6.weekly_profit_history, gWeeklyProfitHistory, sizeof(_s6.weekly_profit_history));

    _s6.park_value = gParkValue;

    memcpy(_s6.park_value_history, gParkValueHistory, sizeof(_s6.park_value_history));

    _s6.completed_company_value = gScenarioCompletedCompanyValue;
    _s6.total_admissions = gTotalAdmissions;
    _s6.income_from_admissions = gTotalIncomeFromAdmissions;
    _s6.company_value = gCompanyValue;
    memcpy(_s6.peep_warning_throttle, gPeepWarningThrottle, sizeof(_s6.peep_warning_throttle));
    memcpy(_s6.awards, gCurrentAwards, sizeof(_s6.awards));
    _s6.land_price = gLandPrice;
    _s6.construction_rights_price = gConstructionRightsPrice;
    // unk_01358774
    // pad_01358776
    // _s6.cd_key
    // _s6.game_version_number
    _s6.completed_company_value_record = gScenarioCompanyValueRecord;
    _s6.loan_hash = GetLoanHash(gInitialCash, gBankLoan, gMaxBankLoan);
    _s6.ride_count = gRideCount;
    // pad_013587CA
    _s6.historical_profit = gHistoricalProfit;
    // pad_013587D4
    memcpy(_s6.scenario_completed_name, gScenarioCompletedBy, sizeof(_s6.scenario_completed_name));
    _s6.cash = gCashEncrypted;
    // pad_013587FC
    _s6.park_rating_casualty_penalty = gParkRatingCasualtyPenalty;
    _s6.map_size_units = gMapSizeUnits;
    _s6.map_size_minus_2 = gMapSizeMinus2;
    _s6.map_size = gMapSize;
    _s6.map_max_xy = gMapSizeMaxXY;
    _s6.same_price_throughout = gSamePriceThroughoutParkA;
    _s6.suggested_max_guests = _suggestedGuestMaximum;
    _s6.park_rating_warning_days = gScenarioParkRatingWarningDays;
    _s6.last_entrance_style = gLastEntranceStyle;
    // rct1_water_colour
    // pad_01358842
    memcpy(_s6.research_items, gResearchItems, sizeof(_s6.research_items));
    _s6.map_base_z = gMapBaseZ;
    memcpy(_s6.scenario_name, gScenarioName, sizeof(_s6.scenario_name));
    memcpy(_s6.scenario_description, gScenarioDetails, sizeof(_s6.scenario_description));
    _s6.current_interest_rate = gBankLoanInterestRate;
    // pad_0135934B
    _s6.same_price_throughout_extended = gSamePriceThroughoutParkB;
    memcpy(_s6.park_entrance_x, gParkEntranceX, sizeof(_s6.park_entrance_x));
    memcpy(_s6.park_entrance_y, gParkEntranceY, sizeof(_s6.park_entrance_y));
    memcpy(_s6.park_entrance_z, gParkEntranceZ, sizeof(_s6.park_entrance_z));
    memcpy(_s6.park_entrance_direction, gParkEntranceDirection, sizeof(_s6.park_entrance_direction));
    safe_strcpy(_s6.scenario_filename, _scenarioFileName, sizeof(_s6.scenario_filename));
    memcpy(_s6.saved_expansion_pack_names, gScenarioExpansionPacks, sizeof(_s6.saved_expansion_pack_names));
    memcpy(_s6.banners, gBanners, sizeof(_s6.banners));
    memcpy(_s6.custom_strings, gUserStrings, sizeof(_s6.custom_strings));
    _s6.game_ticks_1 = gCurrentTicks;
    memcpy(_s6.rides, gRideList, sizeof(_s6.rides));
    _s6.saved_age = gSavedAge;
    _s6.saved_view_x = gSavedViewX;
    _s6.saved_view_y = gSavedViewY;
    _s6.saved_view_zoom = gSavedViewZoom;
    _s6.saved_view_rotation = gSavedViewRotation;
    memcpy(_s6.map_animations, gAnimatedObjects, sizeof(_s6.map_animations));
    _s6.num_map_animations = gNumMapAnimations;
    // pad_0138B582

    _s6.ride_ratings_calc_data = gRideRatingsCalcData;
    memcpy(_s6.ride_measurements, gRideMeasurements, sizeof(_s6.ride_measurements));
    _s6.next_guest_index = gNextGuestNumber;
    _s6.grass_and_scenery_tilepos = gGrassSceneryTileLoopPosition;
    memcpy(_s6.patrol_areas, gStaffPatrolAreas, sizeof(_s6.patrol_areas));
    memcpy(_s6.staff_modes, gStaffModes, sizeof(_s6.staff_modes));
    // unk_13CA73E
    // pad_13CA73F
    _s6.byte_13CA740 = gUnk13CA740;
    _s6.climate = gClimate;
    // pad_13CA741;
    // byte_13CA742
    // pad_013CA747
    _s6.climate_update_timer = gClimateUpdateTimer;
    _s6.current_weather = gClimateCurrentWeather;
    _s6.next_weather = gClimateNextWeather;
    _s6.temperature = gClimateCurrentTemperature;
    _s6.next_temperature = gClimateNextTemperature;
    _s6.current_weather_effect = gClimateCurrentWeatherEffect;
    _s6.next_weather_effect = gClimateNextWeatherEffect;
    _s6.current_weather_gloom = gClimateCurrentWeatherGloom;
    _s6.next_weather_gloom = gClimateNextWeatherGloom;
    _s6.current_rain_level = gClimateCurrentRainLevel;
    _s6.next_rain_level = gClimateNextRainLevel;
    memcpy(_s6.news_items, gNewsItems, sizeof(_s6.news_items));
    // pad_13CE730
    // rct1_scenario_flags
    _s6.wide_path_tile_loop_x = gWidePathTileLoopX;
    _s6.wide_path_tile_loop_y = gWidePathTileLoopY;
    // pad_13CE778

    String::Set(_s6.scenario_filename, sizeof(_s6.scenario_filename), _scenarioFileName);

    if (RemoveTracklessRides)
    {
        scenario_remove_trackless_rides(&_s6);
    }

    scenario_fix_ghosts(&_s6);
    N3dsSaveSimulatingRidesAsClosed(&_s6);
    game_convert_strings_to_rct2(&_s6);
}

uint32 S6Exporter::GetLoanHash(money32 initialCash, money32 bankLoan, uint32 maxBankLoan)
{
    sint32 value = 0x70093A;
    value -= initialCash;
    value = ror32(value, 5);
    value -= bankLoan;
    value = ror32(value, 7);
    value += maxBankLoan;
    value = ror32(value, 3);
    return value;
}

static bool object_is_custom(const ObjectRepositoryItem * object)
{
    Guard::ArgumentNotNull(object);

    // Validate the object is not one from base game or expansion pack
    return (object->LoadedObject != nullptr &&
            object->LoadedObject->GetLegacyData() != nullptr
            && !(object->ObjectEntry.flags & 0xF0));
}

int scenario_write_packed_objects(SDL_RWops* rw, std::vector<const ObjectRepositoryItem *> &objects)
{
    log_verbose("exporting packed objects");
    for (const auto &object : objects)
    {
        Guard::ArgumentNotNull(object);
        log_verbose("exporting object %.8s", object->ObjectEntry.name);
        if (object_is_custom(object))
        {
            if (!object_saved_packed(rw, &object->ObjectEntry))
            {
                return 0;
            }
        }
        else
        {
            log_warning("Refusing to pack vanilla/expansion object \"%s\"", object->ObjectEntry.name);
        }
    }
    return 1;
}

/**
 *
 *  rct2: 0x006AA244
 */
std::vector<const ObjectRepositoryItem *> scenario_get_packable_objects()
{
    std::vector<const ObjectRepositoryItem *> objects;
    IObjectRepository * repo = GetObjectRepository();
    for (size_t i = 0; i < repo->GetNumObjects(); i++)
    {
        const ObjectRepositoryItem *item = &repo->GetObjects()[i];
        // Validate the object is not one from base game or expansion pack
        if (object_is_custom(item))
        {
            objects.push_back(item);
        }
    }
    return objects;
}

extern "C"
{
    enum {
        S6_SAVE_FLAG_EXPORT    = 1 << 0,
        S6_SAVE_FLAG_SCENARIO  = 1 << 1,
        S6_SAVE_FLAG_AUTOMATIC = 1u << 31,
    };

    /**
     *
     *  rct2: 0x006754F5
     * @param flags bit 0: pack objects, 1: save as scenario
     */
    int scenario_save(SDL_RWops* rw, int flags)
    {
        // n3ds port: a save takes seconds, with nothing drawn: a box that says so
        platform_n3ds_saving_begin();
        if (flags & S6_SAVE_FLAG_SCENARIO)
        {
            log_verbose("saving scenario");
        }
        else
        {
            log_verbose("saving game");
        }

        if (!(flags & S6_SAVE_FLAG_AUTOMATIC))
        {
            window_close_construction_windows();
        }

        map_reorganise_elements();
        sprite_clear_all_unused();

        viewport_set_saved_view();

        bool result = false;
        auto s6exporter = new S6Exporter();
        try
        {
            if (flags & S6_SAVE_FLAG_EXPORT) {
                s6exporter->ExportObjectsList = scenario_get_packable_objects();
            }
            s6exporter->RemoveTracklessRides = true;
            s6exporter->Export();
            if (flags & S6_SAVE_FLAG_SCENARIO)
            {
                s6exporter->SaveScenario(rw);
            }
            else
            {
                s6exporter->SaveGame(rw);
            }
            result = true;
        }
        catch (Exception)
        {
        }
        delete s6exporter;

        gfx_invalidate_screen();

        if (result && !(flags & S6_SAVE_FLAG_AUTOMATIC))
        {
            gScreenAge = 0;
        }
        return result;
    }
}
