#ifndef FS3_DRIVER_INCLUDED
#define FS3_DRIVER_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.h
//  Description    : This is the header file for the standardized IO functions
//                   for used to access the FS3 storage system.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun 19 Sep 2021 08:12:43 AM PDT
//

// Include files
#include <stdint.h>

// Defines
#define FS3_MAX_TOTAL_FILES 1024 // Maximum number of files ever
#define FS3_MAX_PATH_LENGTH 128 // Maximum length of filename length

//
// Interface functions

int32_t fs3_mount_disk(void);
	// FS3 interface, mount/initialize filesystem

int32_t fs3_unmount_disk(void);
	// FS3 interface, unmount the disk, close all files

int16_t fs3_open(char *path);
	// This function opens a file and returns a file handle

int16_t fs3_close(int16_t fd);
	// This function closes a file

int32_t fs3_read(int16_t fd, void *buf, int32_t count);
	// Reads "count" bytes from the file handle "fh" into the buffer  "buf"

int32_t fs3_write(int16_t fd, void *buf, int32_t count);
	// Writes "count" bytes to the file handle "fh" from the buffer  "buf"

int32_t fs3_seek(int16_t fd, uint32_t loc);
	// Seek to specific point in the file
	
int findFile(int fd, char *fileName);
	//Function used to find a file based on its fileName

int updateSpace();
	//Function used to update the global variables for disk space available

int fileLocation(int fd, uint16_t localSec, uint_fast32_t localTrk);
	//Function used to find which trk/sec a file is stored on

int fileLocationRead(int fd, uint16_t localSec, uint_fast32_t localTrk);
	//Function sued during read calls to find where the file currently is held on the disk

#endif
