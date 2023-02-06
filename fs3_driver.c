//VERSION 1.0 OF ASSIGNMENT 4


////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_driver.c
//  Description    : This is the implementation of the standardized IO functions
//                   for used to access the FS3 storage system.
//
//   Author        : *** Kyle George ***
//   Last Modified : *** 11/15/21 ***
//

// Includes
#include <string.h>
#include <cmpsc311_log.h>
#include <stdbool.h>
#include <math.h>

// Project Includes
#include <fs3_driver.h>
#include <fs3_controller.h>
#include <fs3_cache.h>
#include <fs3_network.h>

// Defines
#define SECTOR_INDEX_NUMBER(x) ((int)(x/FS3_SECTOR_SIZE))

//
// Static Global Variables

//Variables for a singular file, will be the basis for the structure that will handle multiple files
int mounted = 0;
int unusedBits = 0;
int nextHandle = 10;
int nextSecAvailable = 0;
int nextTrkAvailable = 0;
uint16_t sec;
uint_fast32_t trk;
int recursion = 0;
int tempPos;
int tempLen;
int totalBytes;

struct fileData{
	int fileHandle;
	char *fileName;
	bool fileOpen;
	int fileLen;
	int filePos;
	int fileStorage[FS3_MAX_TRACKS][FS3_TRACK_SIZE];
	int secNums;
};

struct fileData files[1000];
int filesLen = 1000;

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : findFile
// Description  : Finds the file that have been created based on fileName
//
// Inputs       : fd - fileHandle of file to look for
//				  fileName - fileName of file to look for
//				  len - length of array of files
//
// Outputs      : fileHandle if file is found, -1 if failure

int findFile(int fd, char *fileName){
	//Invalid fileHandle inputted
	if (fd > filesLen){
		return (-1);
	}
	//The file might already be open, but open was called again so need to find a file with the same fileName
	if (fd == 0){
		int i;
		for (i=10; i<filesLen; i++){
			if (files[i].fileName == fileName){
				files[i].fileOpen = true;
				fs3_seek(files[i].fileHandle, 0);
				return (files[i].fileHandle);
			}
		}
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : updateSPace
// Description  : Updates the global variables for available space
//
// Inputs       : None
//
// Outputs      : 0 if update was a success, -1 if failure

int updateSpace(){
	if ((nextSecAvailable == 0) && (nextTrkAvailable == 0)){
		nextSecAvailable++;
		return (0);
	}
	if ((nextSecAvailable + 1) > (FS3_TRACK_SIZE - 1)){
		nextTrkAvailable++;
		nextSecAvailable = 0;
		return (0);
	}
	else{
		nextSecAvailable++;
		return (0);
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fileLocation
// Description  : Finds where a file has data on the disk
//
// Inputs       : fd - fileHandle of file to look for
//				  localSec - sector to start at
//				  localTrk - track to start at
//
// Outputs      : 0 if successful search, -1 if failure

int fileLocation(int fd, uint16_t localSec, uint_fast32_t localTrk){
	int i; //equals sector
	int j; //equals track
	//Loop for track
	for (j=localTrk; j<FS3_MAX_TRACKS; j++){
		//Loop for sector in track
		for (i=localSec; i<FS3_TRACK_SIZE; i++){
			if (files[fd].fileStorage[j][i] == 1){
				sec = i;
				trk = j;
				return (0);
			}
		}
		localSec = 0;
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : construct_fs3_cmdblock
// Description  : constructs the command block that will be used
//
// Inputs       : op - operator code
//				  sec - sector number
//				  trk - track number
//				  ret - return value (typically zero)
// Outputs      : the constructed command block

FS3CmdBlk construct_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret){
	//create variables for command blocks that will be used
	uint64_t cmdBlock = 0;
	uint64_t temp = 0;

	//repeated process -> make temp have the different components in the correct spot and or "|" it with cmdBlock
	//  each part will be surrounded by 0's which when or'd with cmdBlock will give the correct cmdBlock back
	cmdBlock = op;
	cmdBlock = cmdBlock << 60;
	temp = sec;
	temp = temp << 44;
	cmdBlock = cmdBlock | temp;
	temp = trk;
	temp = temp << 12;
	cmdBlock = cmdBlock | temp;
	temp = ret;
	temp = temp << 11;
	cmdBlock = cmdBlock | temp;
	cmdBlock = cmdBlock | unusedBits;
	return cmdBlock;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : deconstruct_fs3_cmdblock
// Description  : 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int deconstruct_fs3_cmdblock(FS3CmdBlk *cmdBlock, uint8_t op, uint16_t sec, uint32_t trk, uint8_t ret){
	//Generate a destroyBlock that contains 0's in every place that we don't want the value of
	// then shift to get proper value
	//Getting ret value
	FS3CmdBlk destroyBlock = construct_fs3_cmdblock(0, 0, 0, 1);
	ret = (*cmdBlock & destroyBlock) >> 11;

	//Getting trk value
	destroyBlock = construct_fs3_cmdblock(0, 0, (uint32_t)pow(2.0, 32.0), 0);
	trk = (*cmdBlock & destroyBlock) >> 12;

	//Getting sec value
	destroyBlock = construct_fs3_cmdblock(0, (uint16_t)pow(2.0,16.0), 0, 0);
	sec = (*cmdBlock & destroyBlock) >> 44;

	//Getting op value
	destroyBlock = construct_fs3_cmdblock((uint8_t)pow(2.0, 4.0), 0, 0, 0);
	op = (*cmdBlock & destroyBlock) >> 60;

	return ret;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_mount_disk
// Description  : FS3 interface, mount/initialize filesystem
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_mount_disk(void) {
	//mount the disk
	if (mounted == 0){
		FS3CmdBlk cmdBlock = construct_fs3_cmdblock(FS3_OP_MOUNT, 0, 0, 0);
		FS3CmdBlk *rtnBlock = &cmdBlock;
		network_fs3_syscall(cmdBlock, rtnBlock, NULL);
		
		//value returend here will be the ret value that fs3_syscall gave back
		int32_t retValue = deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_MOUNT, 0, 0, 0);
		if (retValue == 0){
			mounted = 1;
		}
		return retValue;
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_unmount_disk
// Description  : FS3 interface, unmount the disk, close all files
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_unmount_disk(void) {
	//unmount disk
	if (mounted == 1){
		FS3CmdBlk cmdBlock = construct_fs3_cmdblock(FS3_OP_UMOUNT, 0, 0, 0);
		FS3CmdBlk *rtnBlock = &cmdBlock;
		network_fs3_syscall(cmdBlock, rtnBlock, NULL);

		//value returned here will be the ret value that fs3_syscall gave back
		int32_t retValue = deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_UMOUNT, 0, 0, 0);
		if (retValue == 0){
			mounted = 0;
		}
		return retValue;
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_open
// Description  : This function opens the file and returns a file handle
//
// Inputs       : path - filename of the file to open
// Outputs      : file handle if successful, -1 if failure

int16_t fs3_open(char *path) {
	//Checking if disk is mounted
	if (mounted == 0){
		return (-1);
	}

	//Find file to see if it is already open, then handle accordingly
	int fileHandleRtn = findFile(0, path);
	if (fileHandleRtn != -1){
		return (fileHandleRtn);
	}
	else{
		//File is not open yet, so we need to open one and create its' data
		files[nextHandle].fileName = path;
		files[nextHandle].fileHandle = nextHandle;
		files[nextHandle].fileOpen = true;
		files[nextHandle].filePos = 0;
		files[nextHandle].fileLen = 0;
		files[nextHandle].fileStorage[nextTrkAvailable][nextSecAvailable] = 1;
		files[nextHandle].secNums++;
		updateSpace();
		fileHandleRtn = nextHandle;
		nextHandle++;
		return (fileHandleRtn);
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close
// Description  : This function closes the file
//
// Inputs       : fd - the file descriptor
// Outputs      : 0 if successful, -1 if failure

int16_t fs3_close(int16_t fd) {
	//Checking if disk is mounted
	if (mounted ==0){
		return (-1);
	}
	//Checking if fd is valid
	if (fd > filesLen){
		return (-1);
	}
	if (fd < 10){
		return (-1);
	}
	//Closing the file that is opened (if it is open)
	if (files[fd].fileHandle == fd){
		if (files[fd].fileOpen == true){
			files[fd].fileOpen = false;
			return (0);
		}
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_read
// Description  : Reads "count" bytes from the file handle "fh" into the 
//                buffer "buf"
//
// Inputs       : fd - filename of the file to read from
//                buf - pointer to buffer to read into
//                count - number of bytes to read
// Outputs      : bytes read if successful, -1 if failure

int32_t fs3_read(int16_t fd, void *buf, int32_t count) {
	//Checking if disk is mounted
	if (mounted ==0){
		return (-1);
	}
	//Checking if the count that is inputted is good
	if (count <= 0){
		if (recursion == 1){
			recursion = 0;
			return totalBytes;
		}
		return (-1);
	}
	//Check if the fileHandle is good
	if (fd > filesLen){
		return (-1);
	}
	if (fd < 10){
		recursion = 0;
		return (-1);
	}
	if (fd == files[fd].fileHandle){
		//Check if file is open
		if (files[fd].fileOpen == true){
			//Need to find where the file is located
			if (recursion == 0){
				sec = 0;
				trk = 0;
				fileLocation(files[fd].fileHandle, sec, trk);
			}
			//Count might be too large from our current position
			if ((files[fd].filePos + count) > files[fd].fileLen){
				count = files[fd].fileLen - files[fd].filePos;
			}

			int numSec;
			//The current file takes up more than one sector so we need to know where we currently are
			if (files[fd].fileLen >= FS3_SECTOR_SIZE){
				//numSec will give us a way of determining which sector we are in according to the file's data (filePos)
				numSec = (files[fd].filePos / FS3_SECTOR_SIZE);
				//tempPos will tell us where in that file we are currently
				tempPos = (files[fd].filePos - (FS3_SECTOR_SIZE * (numSec)));
			}
			else{
				tempPos = files[fd].filePos;
			}
			//If the pos that were are starting from is in a different sector than the first one so we need to know 
			//what pos that would be in relation to the sector size      ie pos=1025 means we are at pos 1 in next sector
			if ((tempPos < FS3_SECTOR_SIZE) && (tempPos != files[fd].filePos)){
				sec = 0;
				trk = 0;
				int i;
				for(i = 0; i <= numSec; i++){
					if ((sec + 1) > FS3_TRACK_SIZE -1){
						sec = 0;
						trk++;
					}
					else if ((sec == 0) && (i != 0)){
						sec++;
					}
					else if (sec != 0){
						sec++;
					}
					if (fileLocation(files[fd].fileHandle, sec, trk) == -1){
						i = numSec+1;
					}
				}

			}
			char fixedBuf[FS3_SECTOR_SIZE];
			//The position we are currently in might not be in the same sector as the end of the file 
			//so if read is called for more bytes than what are left in the sector then we need to do 2 read syscalls
			if (((tempPos + count) > FS3_SECTOR_SIZE) && (FS3_SECTOR_SIZE < files[fd].fileLen)){
				int spaceAvailable = FS3_SECTOR_SIZE - tempPos;
				
				//Before we make a syscall we need to check if the line is in the cache
				void *tempBuf = fs3_get_cache(trk, sec);
				if (tempBuf != NULL){
					memcpy(fixedBuf, tempBuf, FS3_SECTOR_SIZE);
				}
				FS3CmdBlk cmdBlock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0);
				FS3CmdBlk *rtnBlock = &cmdBlock;
				network_fs3_syscall(cmdBlock, rtnBlock, NULL);
				if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_TSEEK, 0, trk, 0) != 0){
					return (-1);
				}
				cmdBlock = construct_fs3_cmdblock(FS3_OP_RDSECT, sec, 0, 0);
				network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
				if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_RDSECT, sec, 0 ,0) != 0){
					return (-1);
				}
				if (recursion == 1){
					memcpy(buf+totalBytes, &fixedBuf[tempPos], spaceAvailable);
					files[fd].filePos += spaceAvailable;
					totalBytes += spaceAvailable;
				}
				else{
					memcpy(buf, &fixedBuf[tempPos], spaceAvailable);
					files[fd].filePos += spaceAvailable;
					totalBytes = spaceAvailable;
				}
				//We still have more bytes we need to read so we need to call read again and continue on
				spaceAvailable = count - spaceAvailable;
				recursion = 1;
				if (tempBuf == NULL){
					fs3_put_cache(trk, sec, fixedBuf);
				}
				return (fs3_read(files[fd].fileHandle, buf, spaceAvailable));
			}
			
			//File is read for count bytes
			else{
				if (recursion == 1){
					
					//Before we make a syscall we need to check if the buffer is in the cache
					void *tempBuf = fs3_get_cache(trk, sec);
					if (tempBuf != NULL){
						memcpy(fixedBuf, tempBuf, FS3_SECTOR_SIZE);
					}
					FS3CmdBlk cmdBlock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0);
					FS3CmdBlk *rtnBlock = &cmdBlock;
					network_fs3_syscall(cmdBlock, rtnBlock, NULL);
					if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_TSEEK, 0, trk, 0) != 0){
						return (-1);
					}
					cmdBlock = construct_fs3_cmdblock(FS3_OP_RDSECT, sec, 0, 0);
					network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
					if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_RDSECT, sec, 0, 0) != 0){
						return (-1);
					}
					memcpy(buf+totalBytes, fixedBuf+tempPos, count);
					files[fd].filePos += count;
					totalBytes += count;
					recursion = 0;
					if (tempBuf == NULL){
						fs3_put_cache(trk, sec, fixedBuf);
					}
					return totalBytes;
				}

				//Before we make a syscall we need to check if the buffer is in the cache
				void *tempBuf = fs3_get_cache(trk, sec);
				if (tempBuf != NULL){
					memcpy(fixedBuf, tempBuf, FS3_SECTOR_SIZE);
				}
				FS3CmdBlk cmdBlock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0);
				FS3CmdBlk *rtnBlock = &cmdBlock;
				network_fs3_syscall(cmdBlock, rtnBlock, NULL);
				if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_TSEEK, 0, trk, 0) != 0){
					return (-1);
				}
				cmdBlock = construct_fs3_cmdblock(FS3_OP_RDSECT, sec, 0, 0);
				network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
				if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_RDSECT, sec, 0, 0) != 0){
					return (-1);
				}
				memcpy(buf, &fixedBuf[tempPos], count);
				files[fd].filePos += count;
				if (recursion == 1){
					totalBytes += count;
					recursion = 0;
					if (tempBuf == NULL){
						fs3_put_cache(trk, sec, fixedBuf);
					}
					return totalBytes;
				}
				if (tempBuf == NULL){
					fs3_put_cache(trk, sec, fixedBuf);
				}
				return(count);
			}
		}
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_write
// Description  : Writes "count" bytes to the file handle "fh" from the 
//                buffer  "buf"
//
// Inputs       : fd - filename of the file to write to
//                buf - pointer to buffer to write from
//                count - number of bytes to write
// Outputs      : bytes written if successful, -1 if failure

int32_t fs3_write(int16_t fd, void *buf, int32_t count) {
	//Checking if disk is mounted
	if (mounted == 0){
		return (-1);
	}
	//Checking if the buffer contains data
	if (buf == NULL){
		if (recursion == 1){
			recursion = 0;
			return totalBytes;
		}
		return (-1);
	}
	//Validating count number
	if (count <= 0){
		if (recursion == 1){
			recursion = 0;
			return totalBytes;
		}
		return (-1);
	}
	//Validating fileHandle
	if (fd > filesLen){
		return (-1);
	}
	if (fd < 10){
		return (-1);
	}
	//Validating file contents
	if (fd == files[fd].fileHandle){
		if (files[fd].fileOpen == true){
			//Need to find where the file is located
			if (recursion == 0){
				sec = 0;
				trk = 0;
				fileLocation(files[fd].fileHandle, sec, trk);
			}

			char fixedBuf[FS3_SECTOR_SIZE];
			FS3CmdBlk cmdBlock;

			//Can't read the file if there is nothing in it yet
			if ((files[fd].fileLen == 0) && (files[fd].filePos == 0)){
				memcpy(fixedBuf, buf, count);
				cmdBlock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0);
				FS3CmdBlk *rtnBlock = &cmdBlock;
				network_fs3_syscall(cmdBlock, rtnBlock, NULL);
				if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_TSEEK, 0, trk, 0) != 0){
					return (-1);
				}
				cmdBlock = construct_fs3_cmdblock(FS3_OP_WRSECT, sec , 0, 0);
				network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
				
				//Checking if syscall was successful
				if ((deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_WRSECT, sec, 0, 0)) == 0){
					files[fd].fileLen += count;
					files[fd].filePos = files[fd].fileLen;
					recursion = 0;
					fs3_put_cache(trk, sec, fixedBuf);
					return (count);
				}
				else{
					recursion = 0;
					return (-1);
				}
			}

			
			//If the write bytes is going to fill up current sector and go beyond then need to handle accordingly

			int numSec;
			//The current file takes up more than one sector so we need to know where we currently are
			if (files[fd].fileLen >= FS3_SECTOR_SIZE){
				//numSec will give us a way of determining which sector we are in according to the file's data (filePos)
				numSec = (files[fd].filePos / FS3_SECTOR_SIZE);

				//tempPos will tell us where in that file we are currently
				tempPos = (files[fd].filePos - (FS3_SECTOR_SIZE * (numSec)));
			}else{
				//File is only on one sector currently so we can handle as though it is a normal write call
				tempPos = files[fd].filePos;
			}
			//If the pos that were are starting from is in a different sector than the first one so we need to know 
			//what pos that would be in relation to the sector size      ie pos=1025 means we are at pos 1 in next sector
			if ((tempPos < FS3_SECTOR_SIZE) && (tempPos != files[fd].filePos)){
				//Need to loop through until we find the next sector that we can write to
				sec = 0;
				trk = 0;
				int i;
				for(i = 0; i <= numSec; i++){
					if ((sec + 1) > FS3_TRACK_SIZE -1){
						sec = 0;
						trk++;
					}
					else if ((sec == 0) && (i != 0)){
						sec++;
					}
					else if (sec != 0){
						sec++;
					}
					if (fileLocation(files[fd].fileHandle, sec, trk) == -1){
						//If we reach this point then the file is not on any other sector so we need to add a new one to its list and read/write there
						files[fd].fileStorage[nextTrkAvailable][nextSecAvailable] = 1;
						files[fd].secNums++;
						sec = nextSecAvailable;
						trk = nextTrkAvailable;
						updateSpace();
						i = numSec + 1;
					}
				}

			}
			//If this block gets avtivated then that means we are going to need to perform more than one write call to write all the count bytes
			if ((tempPos + count) > FS3_SECTOR_SIZE){
				int spaceAvailable = FS3_SECTOR_SIZE - tempPos;

				//We need to write for spaceAvailable number of bytes and then call write again to finish the last of the bytes

				//Before we call syscall we need to check if the trk/sec is already in the cache
				void *tempBuf = NULL; //fs3_get_cache(trk, sec);
				if (tempBuf != NULL){
					memcpy(fixedBuf, tempBuf, FS3_SECTOR_SIZE);
				}
				//Syscall read into fixedBuf since cache does not have the trk/sec
				cmdBlock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0);
				FS3CmdBlk *rtnBlock = &cmdBlock;
				network_fs3_syscall(cmdBlock, rtnBlock, NULL);
				if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_TSEEK, 0, trk, 0) != 0){
					return (-1);
				}
				cmdBlock = construct_fs3_cmdblock(FS3_OP_RDSECT, sec, 0, 0);
				network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
				if ((deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_RDSECT, sec, 0, 0) != 0)){
					return (-1);
				}
				// Either syscall was successful or trk/sec was in cache so now we need to write in spaceAvailable number of bytes
				memcpy(&fixedBuf[tempPos], buf, spaceAvailable);
				
				cmdBlock = construct_fs3_cmdblock(FS3_OP_WRSECT, sec, 0, 0);
				rtnBlock = &cmdBlock;
				network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
				if ((deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_WRSECT, sec, 0, 0) == 0)){
					//Syscall successfully wrote spaceAvailable number of bytes so now we need to update filePos and fileLen
					if (files[fd].filePos == files[fd].fileLen){
						files[fd].fileLen += spaceAvailable;
						files[fd].filePos = files[fd].fileLen;
					}
					else{
						files[fd].filePos += spaceAvailable;
						if (files[fd].filePos > files[fd].fileLen){
							files[fd].fileLen = files[fd].filePos;
						}
					}

					//We still have bytes left to write so now we need to call write again with the count number updated
					totalBytes = spaceAvailable;
					spaceAvailable = count - spaceAvailable;
					recursion = 1;
					//Creating a var to get the contents that are in buf
					char *tempBuf = (char *)buf;
					//Now we need to move the contents of buf that we still need to a newBuf that we can then pass into write again
					char newBuf[spaceAvailable];
					for (int i=0; i<=spaceAvailable; i++){
						newBuf[i] = tempBuf[i+totalBytes];
					}
					fs3_put_cache(trk, sec, fixedBuf);
					return (fs3_write(files[fd].fileHandle, newBuf, spaceAvailable));
				}
			}

			//Before we call syscall we need to check if the trk/sec is already in the cache
			void *tempBuf = fs3_get_cache(trk, sec);
			if (tempBuf != NULL){
				memcpy(fixedBuf, tempBuf, FS3_SECTOR_SIZE);
			}
			//Sycall read into fixedBuf
			cmdBlock = construct_fs3_cmdblock(FS3_OP_TSEEK, 0, trk, 0);
			FS3CmdBlk *rtnBlock = &cmdBlock;
			network_fs3_syscall(cmdBlock, rtnBlock, NULL);
			if (deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_TSEEK, 0, trk, 0) != 0){
				return (-1);
			}
			cmdBlock = construct_fs3_cmdblock(FS3_OP_RDSECT, sec, 0, 0);
			network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
			if ((deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_RDSECT, sec, 0, 0) != 0)){
				return (-1);
			}
			//Syscall was successful so now need to memcpy what the controller gave back into the buf that controller initially passed
			memcpy(&fixedBuf[tempPos], buf, count);
			cmdBlock = construct_fs3_cmdblock(FS3_OP_WRSECT, sec, 0, 0);
			rtnBlock = &cmdBlock;
			network_fs3_syscall(cmdBlock, rtnBlock, fixedBuf);
			if ((deconstruct_fs3_cmdblock(rtnBlock, FS3_OP_WRSECT, sec, 0, 0) == 0)){
				//Syscall successfully wrote to sector, now need to update internal metadata

				//First block is checking if the written data was appended onto the end of the file
				if (files[fd].filePos == files[fd].fileLen){
					files[fd].fileLen += count;
					files[fd].filePos = files[fd].fileLen;
					if (recursion == 1){
						totalBytes += count;
						recursion = 0;
						fs3_put_cache(trk, sec, fixedBuf);
						return totalBytes;
					}
					fs3_put_cache(trk, sec, fixedBuf);
					return (count);
				}

				//Second block is checking if the file's new position is passed the end of the fileLen or not (if it is then updating that data as well)
				else{
					files[fd].filePos += count;
					if (files[fd].filePos > files[fd].fileLen){
						files[fd].fileLen = files[fd].filePos;
						if (recursion == 1){
							totalBytes += count;
							recursion = 0;
							fs3_put_cache(trk, sec, fixedBuf);
							return (totalBytes);
						}
						fs3_put_cache(trk, sec, fixedBuf);
						return (count);
					}
					if (recursion == 1){
						totalBytes += count;
						recursion = 0;
						fs3_put_cache(trk, sec, fixedBuf);
						return (totalBytes);
					}
					fs3_put_cache(trk, sec, fixedBuf);
					return (count);
				}
			}
		}
	}
	return (-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_seek
// Description  : Seek to specific point in the file
//
// Inputs       : fd - filename of the file to write to
//                loc - offfset of file in relation to beginning of file
// Outputs      : 0 if successful, -1 if failure

int32_t fs3_seek(int16_t fd, uint32_t loc) {
	//Checking if disk is mounted
	if (mounted == 0){
		return (-1);
	}
	//Validating loc
	if (loc < 0){
		return (-1);
	}
	//Validating fileHandle
	if (fd > filesLen){
		return (-1);
	}
	if (fd < 10){
		return (-1);
	}
	//Setting current file's pointer to location inputted
	if (fd == files[fd].fileHandle){
		if (loc > files[fd].fileLen){
			return (-1);
		}
		files[fd].filePos = loc;
		return (0);
	}
	return (-1);
}
