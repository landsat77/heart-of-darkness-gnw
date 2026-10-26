/*
 * hode's FileSystem (fs.h) on the Game & Watch.
 *
 * Game data comes from the packs (tools/hod_pack.py): setup.hod (SETUP.DAT,
 * HOD.DEM) and one pack per level (rock.hod: ROCK_HOD.LVL/.SSS/.MST), each
 * mapped in memory by the platform (external flash on the console) and
 * read in place. hod.gwp (the cutscenes, tools/paf_convert.py) is streamed
 * from the data folder.
 * Saves (setup.cfg) are ordinary files in the save folder.
 */
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "fs.h"
#include "hd_plat.h"
#include "hd_fs.h"

static const int kMaxAssets = 8;
static AssetFile assets[kMaxAssets];
static bool used[kMaxAssets];

static uint32_t rd32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }

/* "ROCK_HOD.LVL" -> "rock", "SETUP.DAT" / "HOD.DEM" -> "setup" */
static void pack_name(const char *file, char *group, size_t size) {
	const char *u = strchr(file, '_');
	if (!u || strncasecmp(u, "_HOD.", 5) != 0) {
		snprintf(group, size, "setup");
		return;
	}
	size_t n = u - file;
	if (n >= size) n = size - 1;
	for (size_t i = 0; i < n; ++i) group[i] = tolower((unsigned char)file[i]);
	group[n] = 0;
}

/* A file of a pack (format: tools/hod_pack.py). */
static const uint8_t *pack_file(const uint8_t *pack, uint32_t pack_size, const char *name, uint32_t *size, uint32_t *flags = 0) {
	if (pack_size < 16 || memcmp(pack, "HODP", 4) != 0) return 0;
	const uint32_t count = rd32(pack + 8);
	for (uint32_t i = 0; i < count && 16 + 32 * (i + 1) <= pack_size; ++i) {
		const uint8_t *e = pack + 16 + 32 * i;
		char entry[17];
		memcpy(entry, e, 16);
		entry[16] = 0;
		if (strcasecmp(entry, name) == 0) {
			const uint32_t offset = rd32(e + 16), len = rd32(e + 20);
			if (offset + len > pack_size) return 0;
			*size = len;
			if (flags) *flags = rd32(e + 24);
			return pack + offset;
		}
	}
	return 0;
}

static FILE *new_asset(const uint8_t *data, uint32_t size, FILE *fp, bool sectored = false) {
	for (int i = 0; i < kMaxAssets; ++i) {
		if (!used[i]) {
			used[i] = true;
			assets[i].data = data;
			assets[i].size = size;
			assets[i].pos = 0;
			assets[i].fp = fp;
			assets[i].sectored = sectored;
			assets[i].name[0] = 0;
			return (FILE *)&assets[i];
		}
	}
	return 0;
}

static int asset_index(FILE *fp) {
	for (int i = 0; i < kMaxAssets; ++i) {
		if (used[i] && fp == (FILE *)&assets[i]) return i;
	}
	return -1;
}

FileSystem::FileSystem(const char *dataPath, const char *savePath)
	: _dataPath(dataPath), _savePath(savePath), _filesCount(0), _filesList(0) {
}

FileSystem::~FileSystem() {
}

FILE *FileSystem::openAssetFile(const char *name) {
	const size_t len = strlen(name);
	if (len > 4 && (strcasecmp(name + len - 4, ".paf") == 0 || strcasecmp(name + len - 4, ".gwp") == 0)) {
		char path[256];
		snprintf(path, sizeof(path), "%s/%s", _dataPath, name);
		FILE *fp = fopen(path, "rb");
		if (!fp) return 0;
		FILE *a = new_asset(0, 0, fp);
		if (!a) {
			fclose(fp);
		} else {
			snprintf(((AssetFile *)a)->name, sizeof(((AssetFile *)a)->name), "%s", name);
		}
		return a;
	}
	char group[16];
	pack_name(name, group, sizeof(group));
	uint32_t pack_size = 0;
	const uint8_t *pack = hd_plat_pack(group, &pack_size);
	if (!pack) return 0;
	uint32_t size = 0, flags = 0;
	const uint8_t *data = pack_file(pack, pack_size, name, &size, &flags);
	if (!data) return 0;
	return new_asset(data, size, 0, (flags & 1) != 0);
}

FILE *FileSystem::openSaveFile(const char *filename, bool write) {
	char path[256];
	snprintf(path, sizeof(path), "%s/%s", _savePath, filename);
	return fopen(path, write ? "wb" : "rb");
}

int FileSystem::closeFile(FILE *fp) {
	const int i = asset_index(fp);
	if (i >= 0) {
		if (assets[i].fp) fclose(assets[i].fp);
		used[i] = false;
		return 0;
	}
	const int err = ferror(fp);
	fclose(fp);
	return err;
}

uint32_t FileSystem::gnwDataFlags() {
	uint32_t pack_size = 0, size = 0, flags = 0;
	const uint8_t *pack = hd_plat_pack("setup", &pack_size);
	if (!pack || !pack_file(pack, pack_size, "SETUP.DAT", &size, &flags)) return 0;
	return flags;
}

void FileSystem::addFilePath(const char *path) {
	(void)path;
}

void FileSystem::listFiles(const char *dir) {
	(void)dir;
}

/* Save states (src/port/hd_main.c): the table of handles is part of the
   state, the real files (hod.gwp) are not. Before a load: close them; after:
   open them again (the cutscene player seeks before reading). */
extern "C" void hd_fs_close_files() {
	for (int i = 0; i < kMaxAssets; ++i) {
		if (used[i] && assets[i].fp) {
			fclose(assets[i].fp);
			assets[i].fp = 0;
		}
	}
}

extern "C" void hd_fs_reopen_files() {
	for (int i = 0; i < kMaxAssets; ++i) {
		if (used[i] && !assets[i].data && assets[i].name[0]) {
			char path[256];
			snprintf(path, sizeof(path), "%s/%s", hd_plat_data_dir(), assets[i].name);
			assets[i].fp = fopen(path, "rb");
			if (!assets[i].fp) {
				hd_plat_fatal("save state: a data file cannot be opened again");
			}
		}
	}
}
