/* hode's files on the Game & Watch (hd_fs.cpp): for the save states */
#ifndef HD_FS_H
#define HD_FS_H

#ifdef __cplusplus
extern "C" {
#endif

/* real files (hod.gwp) of the handle table: closed before a state is
   loaded, opened again after */
void hd_fs_close_files(void);
void hd_fs_reopen_files(void);

#ifdef __cplusplus
}
#endif

#endif
