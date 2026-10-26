
#ifndef FS_H__
#define FS_H__

#include <stdio.h>
#include <stdint.h>

#ifdef GNW
/* Game & Watch: openAssetFile() returns one of these as a FILE * (see
   src/port/hd_fs.cpp): a file of a pack, in memory (external flash), or a
   real file streamed from the SD card (hod.paf). */
struct AssetFile {
	const uint8_t *data;	/* 0: streamed (fp) */
	uint32_t size, pos;
	FILE *fp;
	bool sectored;		/* v1.2 data, sector checksums removed */
	char name[16];		/* real files: reopened after a save state load */
};
#endif

struct FileSystem {

	const char *_dataPath;
	const char *_savePath;
	int _filesCount;
	char **_filesList;

	FileSystem(const char *dataPath, const char *savePath);
	~FileSystem();

	FILE *openAssetFile(const char *filename);
	FILE *openSaveFile(const char *filename, bool write);
	int closeFile(FILE *);

	void addFilePath(const char *path);
	void listFiles(const char *dir);
#ifdef GNW
	/* what tools/hod_pack.py found out (flags of SETUP.DAT) */
	enum { kGnwV1_2 = 2, kGnwV1_0 = 4, kGnwDemo = 8 };
	uint32_t gnwDataFlags();
#endif
};

#endif // FS_H__
