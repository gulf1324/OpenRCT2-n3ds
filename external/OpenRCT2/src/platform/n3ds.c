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

/*
 * n3ds port: Nintendo 3DS counterpart of linux.c.
 * posix.c provides the shared POSIX implementation (enabled for __3DS__ as well);
 * this file supplies the parts that linux.c provides on Linux.
 *
 * SD card layout (FAT is case-insensitive, so RCT2's "Data" and OpenRCT2's "data"
 * must not share a parent folder):
 *   /3ds/openrct2/data   OpenRCT2 data (g2.dat, language/, title/)
 *   /3ds/openrct2/rct2   original RollerCoaster Tycoon 2 files (Data/, ObjData/, ...)
 *   /3ds/openrct2/user   config.ini, saves
 *   /3ds/openrct2/rct1   original RollerCoaster Tycoon 1 files, optional: Scenarios/
 */

#include "../common.h"

#ifdef __3DS__

#include <3ds.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../config.h"
#include "../interface/viewport.h"
#include "../interface/window.h"
#include "../localisation/currency.h"
#include "../localisation/language.h"
#include "../util/util.h"
#include "../version.h"
#include "platform.h"

#define N3DS_BASE_PATH "/3ds/openrct2"

// libctru's default main thread stack is 32 KB, too small for code written for PC.
u32 __stacksize__ = 1024 * 1024;

// libctru's __appInit (it runs before main), with the registration with the system (aptInit,
// and hidInit which follows it) taken out and left to a thread.
// The HOME menu shows the start-up logo until the game registers; it then ends the logo and,
// some time after that, hands the screens over.
// libctru registers at once: the hand-over then comes after 1.8 s, in which the game only
// waited, and the screens are black for all of the loading that follows (11.7 s to the intro.
// User's report: too long; their decision: a logo of 3 s, with the loading going on under it,
// and whatever loading is left after it under black screens as before).
// So the main thread starts loading at once and a thread registers N3DS_REGISTER_AFTER_MS
// after the start. Measured on the device: the game starts about 2 s after the icon is tapped;
// the logo goes about 0.3 s after the registration (the user's stopwatch: 4.5 s after the
// tap); the hand-over comes 0.7 s after the registration when the game is idle and the logo
// has been up for long. It does not come while the game reads the SD card: the HOME menu needs
// the card for about 0.44 s of that time, which is why rct2_init ends the loading before the
// screens are opened with work that reads nothing (the sound effects).
// What needs the hand-over waits for it in platform_n3ds_apt_begin (platform_create_window
// calls it): the screens, the sound device (the DSP is the HOME menu's until then, and SDL
// needs APT to put its audio thread on core 1), the buttons. Nothing before that may use
// them. SDL's main sets the New 3DS speed (osSetSpeedupEnable): that needs ptm:sysm only.
#define N3DS_REGISTER_AFTER_MS 2250

static u64 _startTick;               // __appInit
static u64 _registerBeginTick;       // aptInit called
static u64 _registerEndTick;         // aptInit back: the HOME menu has handed over
static volatile bool _registered = false;
static Thread _registerThread = NULL;

static void n3ds_register(void)
{
	_registerBeginTick = svcGetSystemTick();
	aptInit();
	_registerEndTick = svcGetSystemTick();
	hidInit();
	_registered = true;
}

static void n3ds_register_thread(void *arg)
{
	svcSleepThread((s64)N3DS_REGISTER_AFTER_MS * 1000 * 1000);
	n3ds_register();
}

void __appInit(void)
{
	_startTick = svcGetSystemTick();
	srvInit();

	// Above the main thread, so that it runs when its sleep and its waits end although the
	// main thread is busy. It only sleeps and waits.
	s32 priority = 0x30;
	svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
	_registerThread = threadCreate(n3ds_register_thread, NULL, 16 * 1024, priority - 1, -2, false);
	if (_registerThread == NULL)
		n3ds_register();	// at once, as libctru does

	fsInit();
	archiveMountSdmc();
}

// Returns once the game is registered with the system and has been handed the screens
static void n3ds_wait_registered(void)
{
	if (_registerThread != NULL) {
		threadJoin(_registerThread, U64_MAX);
		threadFree(_registerThread);
		_registerThread = NULL;
	}
}

static unsigned int n3ds_ms_since_start(u64 tick)
{
	return (unsigned int)((tick - _startTick) * 1000 / SYSCLOCK_ARM11);
}

void platform_n3ds_apt_begin(void)
{
	if (_registerThread == NULL) return;

	u64 begin = svcGetSystemTick();
	n3ds_wait_registered();
	log_warning("n3ds start: registered with the system %u ms after the start (the start-up logo ends about then) and handed the screens at %u ms; the game waited %u ms for that, at %u ms",
		n3ds_ms_since_start(_registerBeginTick), n3ds_ms_since_start(_registerEndTick),
		(unsigned int)((svcGetSystemTick() - begin) * 1000 / SYSCLOCK_ARM11), n3ds_ms_since_start(begin));
}

// libctru's __appExit. If the game ends before it has opened the screens (data not found, a
// wrong argument) it waits for the registration first: the HOME menu would keep its logo up
// for a game that is gone. No logging here: stdio is closed by now.
void __appExit(void)
{
	n3ds_wait_registered();

	archiveUnmountAll();
	fsExit();

	hidExit();
	aptExit();
	srvExit();
}

#pragma region File buffering

// libctru's SD card driver reports no block size, so newlib gives every FILE a 1 KB buffer and
// each refill becomes a separate 1 KB request to the SD card, each with a fixed delay. That made
// loading crawl (about 0.2 MB/s: g1.dat, objects, parks). The link option -Wl,--wrap=fopen
// (cmake/n3ds.cmake) routes every fopen, including SDL_RWFromFile's, through here to get a large
// buffer instead.
#define N3DS_FILE_BUFFER_SIZE (64 * 1024)

FILE *__real_fopen(const char *path, const char *mode);

FILE *__wrap_fopen(const char *path, const char *mode)
{
	uint64 perfBegin = platform_n3ds_perf_begin();
	FILE *file = __real_fopen(path, mode);
	if (file != NULL) {
		setvbuf(file, NULL, _IOFBF, N3DS_FILE_BUFFER_SIZE);
	}
	platform_n3ds_perf_end(N3DS_PERF_FOPEN, perfBegin);
	return file;
}

#pragma endregion

#pragma region Heap split

// Replaces libctru's weak __system_allocateHeaps (libctru source/system/allocateHeaps.c).
// libctru splits application memory evenly and may give up to 32 MB to the linear heap (GPU/DSP
// memory) while capping the regular heap at 24 MB. This game only needs linear memory for the
// SDL framebuffers (about 1.4 MB) and audio buffers, while malloc needs room for g1.dat (17 MB)
// and much more. So give the regular heap as much as its virtual area allows (96 MB,
// OS_HEAP_AREA_BEGIN..END) while leaving at least N3DS_MIN_LINEAR_HEAP for linear memory.
// A plain "linear heap = 8 MB" setting fails on a 124 MB system: the rest exceeds 96 MB.
#define N3DS_MIN_LINEAR_HEAP (8 * 1024 * 1024)

extern char *fake_heap_start;
extern char *fake_heap_end;
extern u32 __ctru_heap;
extern u32 __ctru_heap_size;
extern u32 __ctru_linear_heap;
extern u32 __ctru_linear_heap_size;

void __system_allocateHeaps(void)
{
	Handle reslimit = 0;
	if (R_FAILED(svcGetResourceLimit(&reslimit, CUR_PROCESS_HANDLE)))
		svcBreak(USERBREAK_PANIC);
	s64 maxCommit = 0, currentCommit = 0;
	ResourceLimitType reslimitType = RESLIMIT_COMMIT;
	svcGetResourceLimitLimitValues(&maxCommit, reslimit, &reslimitType, 1);
	svcGetResourceLimitCurrentValues(&currentCommit, reslimit, &reslimitType, 1);
	svcCloseHandle(reslimit);
	u32 remaining = (u32)(maxCommit - currentCommit) & ~0xFFF;

	u32 heapAreaSize = OS_HEAP_AREA_END - OS_HEAP_AREA_BEGIN;
	u32 heapSize = remaining > N3DS_MIN_LINEAR_HEAP ? remaining - N3DS_MIN_LINEAR_HEAP : remaining / 2;
	if (heapSize > heapAreaSize)
		heapSize = heapAreaSize;
	heapSize &= ~0xFFF;
	__ctru_heap_size = heapSize;
	__ctru_linear_heap_size = remaining - heapSize;

	if (R_FAILED(svcControlMemory(&__ctru_heap, OS_HEAP_AREA_BEGIN, 0x0, __ctru_heap_size, MEMOP_ALLOC, MEMPERM_READ | MEMPERM_WRITE)))
		svcBreak(USERBREAK_PANIC);
	if (R_FAILED(svcControlMemory(&__ctru_linear_heap, 0x0, 0x0, __ctru_linear_heap_size, MEMOP_ALLOC_LINEAR, MEMPERM_READ | MEMPERM_WRITE)))
		svcBreak(USERBREAK_PANIC);

	mappableInit(OS_MAP_AREA_BEGIN, OS_MAP_AREA_END);

	fake_heap_start = (char *)__ctru_heap;
	fake_heap_end = fake_heap_start + __ctru_heap_size;
}

#pragma endregion

#pragma region Log output to the PC

// When started through 3dslink, stdout/stderr are sent to the PC terminal.
#define N3DS_SOC_ALIGN      0x1000
#define N3DS_SOC_BUFFERSIZE 0x100000

static u32 *_socBuffer = NULL;
static int _linkSocket = -1;

static void n3ds_shutdown_log(void)
{
	if (_socBuffer == NULL) return;
	// Order matters: link3dsStdio() dup2()s the socket onto fd 1 and 2. If socExit() runs first,
	// newlib's device table entry becomes NULL and closing stdio at exit crashes (data abort in
	// _close_r). Close every fd that refers to the socket before socExit(). See CLAUDE.md.
	if (_linkSocket >= 0) {
		fflush(stdout);
		fflush(stderr);
		close(STDOUT_FILENO);
		close(STDERR_FILENO);
		close(_linkSocket);
		_linkSocket = -1;
	}
	socExit();
	free(_socBuffer);
	_socBuffer = NULL;
}

// A fixed amount of computation that does not touch memory: each step waits for the one before
// (a multiplication and an addition), which takes an ARM11 4 to 6 clock cycles.
static u32 __attribute__((noinline)) n3ds_cpu_test(u32 steps)
{
	u32 x = 1;
	for (u32 i = 0; i < steps; i++)
		x = x * 1664525 + 1013904223;
	return x;
}

void platform_n3ds_init(void)
{
	if (__3dslink_host.s_addr != 0) {
		_socBuffer = (u32 *)memalign(N3DS_SOC_ALIGN, N3DS_SOC_BUFFERSIZE);
		if (_socBuffer != NULL && socInit(_socBuffer, N3DS_SOC_BUFFERSIZE) == 0) {
			_linkSocket = link3dsStdio();
			setvbuf(stdout, NULL, _IONBF, 0);
		} else {
			free(_socBuffer);
			_socBuffer = NULL;
		}
	} else {
		// Not started through 3dslink: from the HOME menu (the installed CIA), or in the Azahar
		// emulator. stderr, where the game logs, goes to a file on the SD card, user/log.txt (the
		// run before is kept as user/log_prev.txt). The installed game has no other way to show
		// its log, and without it its speed could not be compared with that of a run through
		// 3dslink (user's report: the installed game felt slower). A line is written when it is
		// complete: a few lines every 10 seconds (the performance log).
		// Without the file (no user folder yet) stderr goes to svcOutputDebugString, which Azahar
		// prints in its log (Debug.Emulated) and which on a 3DS only reaches an attached debugger.
		remove(N3DS_BASE_PATH "/user/log_prev.txt");
		rename(N3DS_BASE_PATH "/user/log.txt", N3DS_BASE_PATH "/user/log_prev.txt");
		int logFile = open(N3DS_BASE_PATH "/user/log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0666);
		if (logFile >= 0) {
			dup2(logFile, STDERR_FILENO);
			close(logFile);
		} else {
			consoleDebugInit(debugDevice_SVC);
		}
		setvbuf(stderr, NULL, _IOLBF, 1024);
	}
	// atexit handlers run before newlib closes stdio at exit.
	atexit(n3ds_shutdown_log);

	// Which build wrote this log (the same name as in the version text on the title screen)
	log_warning("n3ds build: %s", gCommitSha1Short);
	// The times of the "n3ds start" lines count from this log's first line
	log_warning("n3ds start: this log begins %u ms after the start of the game (__appInit)",
		n3ds_ms_since_start(svcGetSystemTick()));
	log_warning("n3ds memory: application region %lu KB, heap %lu KB, linear heap %lu KB",
		(unsigned long)(osGetMemRegionSize(MEMREGION_APPLICATION) / 1024),
		(unsigned long)(__ctru_heap_size / 1024),
		(unsigned long)(__ctru_linear_heap_size / 1024));

	// Is the New 3DS speedup on (804 MHz and the L2 cache; SDL's main asks for it with
	// osSetSpeedupEnable)? The clock that times the test always runs at 268 MHz, so the time
	// tells the CPU's speed: about 5 cycles per step if the guess of the clock is right.
	const u32 steps = 2000000;
	u64 begin = svcGetSystemTick();
	u32 result = n3ds_cpu_test(steps);
	u32 us = (u32)((svcGetSystemTick() - begin) * 1000000 / SYSCLOCK_ARM11);
	log_warning("n3ds cpu: %u steps in %u us: %u.%u cycles per step if the clock is 804 MHz, %u.%u if 268 MHz (%08x)",
		(unsigned int)steps, (unsigned int)us,
		(unsigned int)(us * 804 / (steps / 10) / 10), (unsigned int)(us * 804 / (steps / 10) % 10),
		(unsigned int)(us * 268 / (steps / 10) / 10), (unsigned int)(us * 268 / (steps / 10) % 10),
		(unsigned int)result);
}

#pragma endregion

#pragma region Temporary memory

// Loading or saving a park needs large blocks of memory for a short time: a copy of the whole
// park (about 5.5 MB), the decoded data of a chunk, the file being written. By then the regular
// heap is mostly in use and may have no free block that large, and the original code does not
// check: it wrote through null pointers when going back to the title screen and when
// autosaving. The linear heap is nearly unused (heap split above), so these blocks are taken
// from there first.
void *platform_n3ds_temp_alloc(size_t size)
{
	void *memory = linearAlloc(size);
	if (memory == NULL)
		memory = malloc(size);
	return memory;
}

void platform_n3ds_temp_free(void *memory)
{
	u32 address = (u32)memory;
	if (address >= __ctru_linear_heap && address < __ctru_linear_heap + __ctru_linear_heap_size)
		linearFree(memory);
	else
		free(memory);
}

#pragma endregion

#pragma region Performance log

// Logs every 10 seconds how fast the game runs and where the time of a frame goes (the parts in
// platform.h), to decide what to optimise. The game wants 40 frames a second; it does not skip
// drawing when it falls behind, so a lower rate means the game itself runs slower.
// The parts are timed by the clock: whatever interrupts a part (the audio thread) counts as it.
static u64 _perfPartTicks[N3DS_PERF_COUNT];
static u32 _perfPartCalls[N3DS_PERF_COUNT];
static u64 _perfLogStart = 0;
static u64 _perfFrameStart = 0;
static u64 _perfWorstFrame = 0;
static u32 _perfFrames = 0;

uint64 platform_n3ds_perf_begin()
{
	return svcGetSystemTick();
}

void platform_n3ds_perf_end(int part, uint64 begin)
{
	_perfPartTicks[part] += svcGetSystemTick() - begin;
	_perfPartCalls[part]++;
}

static unsigned int n3ds_ticks_to_tenths_of_ms(u64 ticks)
{
	return (unsigned int)(ticks * 10000 / SYSCLOCK_ARM11);
}

// Called once per frame
void platform_n3ds_perf_frame()
{
	u64 now = svcGetSystemTick();
	if (_perfLogStart == 0) {
		_perfLogStart = _perfFrameStart = now;
		return;
	}
	if (now - _perfFrameStart > _perfWorstFrame)
		_perfWorstFrame = now - _perfFrameStart;
	_perfFrameStart = now;
	_perfFrames++;
	if (now - _perfLogStart < 10ULL * SYSCLOCK_ARM11)
		return;

	unsigned int fps = (unsigned int)((u64)_perfFrames * 10 * SYSCLOCK_ARM11 / (now - _perfLogStart));
	unsigned int update = n3ds_ticks_to_tenths_of_ms(_perfPartTicks[N3DS_PERF_UPDATE] / _perfFrames);
	unsigned int paint = n3ds_ticks_to_tenths_of_ms(_perfPartTicks[N3DS_PERF_PAINT] / _perfFrames);
	unsigned int present = n3ds_ticks_to_tenths_of_ms(_perfPartTicks[N3DS_PERF_PRESENT] / _perfFrames);
	unsigned int worst = n3ds_ticks_to_tenths_of_ms(_perfWorstFrame);
	int parkWidth, parkHeight, zoom = -1;
	platform_n3ds_get_park_size(&parkWidth, &parkHeight);
	rct_window *mainWindow = window_get_main();
	if (mainWindow != NULL && mainWindow->viewport != NULL)
		zoom = mainWindow->viewport->zoom;
	int numWindows = (int)(gWindowNextSlot - g_window_list);
	log_warning("n3ds perf: %u.%u fps; per frame: update %u.%u ms, paint %u.%u ms, present %u.%u ms; worst frame %u.%u ms; guests %u, zoom %d, park %dx%d, windows %d",
		fps / 10, fps % 10, update / 10, update % 10, paint / 10, paint % 10, present / 10, present % 10,
		worst / 10, worst % 10, (unsigned int)gNumGuestsInPark, zoom, parkWidth, parkHeight, numWindows);

	// The parts within those, in ms per frame; then what happened in the 10 seconds
	unsigned int ms[N3DS_PERF_COUNT];
	for (int i = 0; i < N3DS_PERF_COUNT; i++)
		ms[i] = n3ds_ticks_to_tenths_of_ms(_perfPartTicks[i] / _perfFrames);
	unsigned int ticks = _perfPartCalls[N3DS_PERF_LOGIC] * 10 / _perfFrames;
	unsigned int rects = _perfPartCalls[N3DS_PERF_DIRTY] * 10 / _perfFrames;
	unsigned int columns = _perfPartCalls[N3DS_PERF_GENERATE] * 10 / _perfFrames;
	#define MS(part) ms[part] / 10, ms[part] % 10
	#define TOTAL_MS(part) (unsigned int)(_perfPartTicks[part] * 1000 / SYSCLOCK_ARM11)
	log_warning("n3ds perf detail: update: events %u.%u, logic %u.%u in %u.%u ticks, windows %u.%u, input %u.%u; logic (also on the title screen): peeps %u.%u, vehicles %u.%u, rides %u.%u, sounds %u.%u; paint: views %u.%u, dirty %u.%u in %u.%u rectangles, other windows %u.%u, %u.%u columns: generate %u.%u, arrange %u.%u, sprites %u.%u; audio %u.%u, mixer wait %u.%u; in 10 s: %u music starts %u ms, %u file opens %u ms, %u columns out of paint structs",
		MS(N3DS_PERF_EVENTS), MS(N3DS_PERF_LOGIC), ticks / 10, ticks % 10, MS(N3DS_PERF_WINDOWS), MS(N3DS_PERF_INPUT),
		MS(N3DS_PERF_PEEPS), MS(N3DS_PERF_VEHICLES), MS(N3DS_PERF_RIDES), MS(N3DS_PERF_SOUNDS),
		MS(N3DS_PERF_VIEWS), MS(N3DS_PERF_DIRTY), rects / 10, rects % 10, MS(N3DS_PERF_UI), columns / 10, columns % 10,
		MS(N3DS_PERF_GENERATE), MS(N3DS_PERF_ARRANGE), MS(N3DS_PERF_SPRITES),
		MS(N3DS_PERF_AUDIO), MS(N3DS_PERF_MIXER_WAIT),
		(unsigned int)_perfPartCalls[N3DS_PERF_MUSIC_START], TOTAL_MS(N3DS_PERF_MUSIC_START),
		(unsigned int)_perfPartCalls[N3DS_PERF_FOPEN], TOTAL_MS(N3DS_PERF_FOPEN),
		(unsigned int)_perfPartCalls[N3DS_PERF_STRUCTS_FULL]);
	#undef MS
	#undef TOTAL_MS

	memset(_perfPartTicks, 0, sizeof(_perfPartTicks));
	memset(_perfPartCalls, 0, sizeof(_perfPartCalls));
	_perfLogStart = now;
	_perfWorstFrame = 0;
	_perfFrames = 0;
}

#pragma endregion

// Logs how much of the regular heap malloc has handed out. Call sites are marked "n3ds port".
void platform_n3ds_log_memory(const char *where)
{
	struct mallinfo mi = mallinfo();
	// The share of core 1 the application may use. SDL puts its audio thread on core 1 only if
	// APT grants it 30% (SDL_systhread.c); 0 here once the sound is on means it did not, and the
	// mixer then runs on the main thread's core. This depends on what started the game.
	// (Not asked, and 0, before the game is registered with the system: __appInit.)
	u32 core1Limit = 0;
	if (_registered)
		APT_GetAppCpuTimeLimit(&core1Limit);
	log_warning("n3ds memory at %s: malloc in use %lu KB of %lu KB heap; core 1 time limit %u%%",
		where, (unsigned long)(mi.uordblks / 1024), (unsigned long)(__ctru_heap_size / 1024),
		(unsigned int)core1Limit);
}

void platform_get_exe_path(utf8 *outPath, size_t outSize)
{
	safe_strcpy(outPath, N3DS_BASE_PATH, outSize);
}

bool platform_check_steam_overlay_attached()
{
	return false;
}

void platform_posix_sub_user_data_path(char *buffer, size_t size, const char *homedir)
{
	(void)homedir;
	safe_strcpy(buffer, N3DS_BASE_PATH "/user", size);
	path_end_with_separator(buffer, size);
}

void platform_posix_sub_resolve_openrct_data_path(utf8 *out, size_t size)
{
	safe_strcpy(out, N3DS_BASE_PATH "/data", size);
}

// Where the files of RollerCoaster Tycoon 1 are, if the player has put any on the card (its
// scenarios, for the scenario list). This version of the game has no setting for it.
void platform_n3ds_get_rct1_path(utf8 *out, size_t size)
{
	safe_strcpy(out, N3DS_BASE_PATH "/rct1", size);
}

bool platform_open_common_file_dialog(utf8 *outFilename, file_dialog_desc *desc, size_t outSize)
{
	(void)outFilename; (void)desc; (void)outSize;
	STUB();
	return false;
}

utf8 *platform_open_directory_browser(utf8 *title)
{
	(void)title;
	STUB();
	return NULL;
}

void platform_show_messagebox(utf8 *message)
{
	log_warning("%s", message);
}

// Fonts are not files here: the ones there are are built into the program, and n3ds_font.c
// knows them by the file name of their descriptor (TTF_OpenFont fails for any other).
bool platform_get_font_path(TTFFontDescriptor *font, utf8 *buffer, size_t size)
{
	safe_strcpy(buffer, font->filename, size);
	return true;
}

// newlib is built without basename(); posix.c only needs the last path component.
char *basename(char *path)
{
	char *slash = strrchr(path, '/');
	return slash != NULL ? slash + 1 : path;
}

#pragma region Locale from the system settings

static u8 n3ds_get_region(void)
{
	u8 region = CFG_REGION_EUR;
	if (R_SUCCEEDED(cfguInit())) {
		CFGU_SecureInfoGetRegion(&region);
		cfguExit();
	}
	return region;
}

uint16 platform_get_locale_language()
{
	u8 language = CFG_LANGUAGE_EN;
	if (R_SUCCEEDED(cfguInit())) {
		CFGU_GetSystemLanguage(&language);
		cfguExit();
	}
	switch (language) {
	case CFG_LANGUAGE_EN: return n3ds_get_region() == CFG_REGION_USA ? LANGUAGE_ENGLISH_US : LANGUAGE_ENGLISH_UK;
	case CFG_LANGUAGE_FR: return LANGUAGE_FRENCH;
	case CFG_LANGUAGE_DE: return LANGUAGE_GERMAN;
	case CFG_LANGUAGE_IT: return LANGUAGE_ITALIAN;
	case CFG_LANGUAGE_ES: return LANGUAGE_SPANISH;
	case CFG_LANGUAGE_NL: return LANGUAGE_DUTCH;
	case CFG_LANGUAGE_PT: return LANGUAGE_PORTUGUESE_BR;
	case CFG_LANGUAGE_KO: return LANGUAGE_KOREAN;	// its font is built in (n3ds_font.c)
	default:
		// Japanese, Chinese and Russian need TrueType fonts, which the 3DS build lacks.
		return LANGUAGE_ENGLISH_UK;
	}
}

uint8 platform_get_locale_currency()
{
	switch (n3ds_get_region()) {
	case CFG_REGION_JPN: return CURRENCY_YEN;
	case CFG_REGION_USA: return CURRENCY_DOLLARS;
	case CFG_REGION_KOR: return CURRENCY_WON;
	case CFG_REGION_CHN: return CURRENCY_YUAN;
	case CFG_REGION_TWN: return CURRENCY_TWD;
	case CFG_REGION_AUS: return CURRENCY_DOLLARS;
	default: return CURRENCY_EUROS;
	}
}

uint8 platform_get_locale_measurement_format()
{
	return n3ds_get_region() == CFG_REGION_USA ? MEASUREMENT_FORMAT_IMPERIAL : MEASUREMENT_FORMAT_METRIC;
}

#pragma endregion

#endif
