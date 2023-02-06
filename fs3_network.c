////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_netowork.c
//  Description    : This is the network implementation for the FS3 system.

//
//  Author         : Patrick McDaniel
//  Last Modified  : Thu 16 Sep 2021 03:04:04 PM EDT
//

// Includes
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cmpsc311_log.h>

// Project Includes
#include <fs3_network.h>
#include <math.h>
#include <fs3_controller.h>
#include <cmpsc311_util.h>
#include <string.h>

//
//  Global data
unsigned char     *fs3_network_address = NULL; // Address of FS3 server
unsigned short     fs3_network_port = 0;       // Port of FS3 server

int unusedbits = 0;
int socketfd;
struct sockaddr_in cadder;
int connected = -1;


//
// Network functions

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

FS3CmdBlk construct_network_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret){
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
	cmdBlock = cmdBlock | unusedbits;
	return cmdBlock;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : deconstruct_fs3_cmdblock
// Description  : deconstructs command block that was passed
//
// Inputs       : same parameters as construction function
// Outputs      : 0 if successful, -1 if failure

int deconstruct_network_fs3_cmdblock(FS3CmdBlk cmdBlock, uint8_t op, uint16_t sec, uint32_t trk, uint8_t ret){
	//Generate a destroyBlock that contains 0's in every place that we don't want the value of
	// then shift to get proper value
	//Getting ret value
	FS3CmdBlk destroyBlock = construct_network_fs3_cmdblock(0, 0, 0, 1);
	ret = (cmdBlock & destroyBlock) >> 11;

	//Getting trk value
	destroyBlock = construct_network_fs3_cmdblock(0, 0, pow(2.0, 32.0), 0);
	trk = (cmdBlock & destroyBlock) >> 12;

	//Getting sec value
	destroyBlock = construct_network_fs3_cmdblock(0, (uint16_t)pow(2.0,16.0), 0, 0);
	sec = (cmdBlock & destroyBlock) >> 44;

	//Getting op value
	op = cmdBlock >> 60;

	return op;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : network_fs3_syscall
// Description  : Perform a system call over the network
//
// Inputs       : cmd - the command block to send
//                ret - the returned command block
//                buf - the buffer to place received data in
// Outputs      : 0 if successful, -1 if failure

int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf)
{

	//MOUNT function called, so initializing network connection
	if (deconstruct_network_fs3_cmdblock(cmd, FS3_OP_MOUNT, 0, 0, 0) == 0){
		//Setting up address and port data
		if (fs3_network_address == NULL){
			fs3_network_address = (unsigned char *)FS3_DEFAULT_IP;
		}
		if (fs3_network_port == 0){
			fs3_network_port = FS3_DEFAULT_PORT;
		}

		//Creating the socket that will be used
		socketfd = socket(PF_INET, SOCK_STREAM, 0);
		if (socketfd == -1){
			//Socket creation was unsuccessful
			return (-1);
		}

		//Creating netowrk address data
		cadder.sin_family = AF_INET;
		cadder.sin_port = htons(fs3_network_port);
		if (inet_aton((const char *)fs3_network_address, &cadder.sin_addr) == 0){
			return (-1);
		}

		//Making connection to socket
		if (connect(socketfd, (const struct sockaddr*)&cadder, sizeof(cadder)) == -1){
			return (-1);
		}

		uint64_t cmdConvert = htonll64(cmd);
		if (write (socketfd, &cmdConvert, sizeof(cmdConvert)) != sizeof(cmdConvert)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		if (read(socketfd, ret, sizeof(FS3CmdBlk)) != sizeof(FS3CmdBlk)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		*ret = ntohll64(*ret);

		connected = 0;

		return (0);
	}

	//TSEEK function called, so need to send track information to the server
	if (deconstruct_network_fs3_cmdblock(cmd, FS3_OP_TSEEK, 0, 0, 0) == 1){
		if (connected != 0){
			return (-1);
		}
		uint64_t cmdConvert = htonll64(cmd);
		if (write(socketfd, &cmdConvert, sizeof(cmdConvert)) != sizeof(cmdConvert)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		if (read(socketfd, ret, sizeof(FS3CmdBlk)) != sizeof(FS3CmdBlk)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		*ret = ntohll64(*ret);

		return (0);
	}

	//RDSECT function called, so need to read the data in the sector and return it to the driver
	if (deconstruct_network_fs3_cmdblock(cmd, FS3_OP_RDSECT, 0, 0, 0) == 2){
		if (connected != 0){
			return (-1);
		}
		//Need to call read and fill in the buf with the data
		uint64_t cmdConvert = htonll64(cmd);
		if (write(socketfd, &cmdConvert, sizeof(cmdConvert)) != sizeof(cmdConvert)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		if (read(socketfd, ret, sizeof(FS3CmdBlk)) != sizeof(FS3CmdBlk)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}

		if (read(socketfd, buf, FS3_SECTOR_SIZE) != FS3_SECTOR_SIZE){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		*ret = ntohll64(*ret);

		return (0);
	}

	//WRSECT function called, so need to write the data into the sector
	if (deconstruct_network_fs3_cmdblock(cmd, FS3_OP_WRSECT, 0, 0, 0) == 3){
		if (connected != 0){
			return (-1);
		}
		//Need to call write and fill in the new data
		uint64_t cmdConvert = htonll64(cmd);
		if (write(socketfd, &cmdConvert, sizeof(cmdConvert)) != sizeof(cmdConvert)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}

		if (write(socketfd, buf, FS3_SECTOR_SIZE) != FS3_SECTOR_SIZE){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		if (read(socketfd, ret, sizeof(FS3CmdBlk)) != sizeof(FS3CmdBlk)){
			*ret = construct_network_fs3_cmdblock(0, 0, 0, 1);
			return (-1);
		}
		*ret = ntohll64(*ret);

		return (0);
	}

	//UMOUNT function called, so need to close the socket
	if (deconstruct_network_fs3_cmdblock(cmd, FS3_OP_UMOUNT, 0, 0, 0) == 4){
		if (connected != 0){
			return (-1);
		}
		//Need to close the socket
		close(socketfd);
    	socketfd = -1;
		
		*ret = construct_network_fs3_cmdblock(0, 0, 0, 0);

		return (0);
	}
    // Return successfully
    return (0);
}

