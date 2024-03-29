#ifndef FS3_NETWORK_INCLUDED
#define FS3_NETWORK_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : fs3_network.h
//  Description   : This is the network definitions for the FS3 system.
//
//  Author        : Patrick McDaniel
//  Last Modified : Sun Oct 30 07:48:47 EDT 2016
//

// Include Files

// Project Include Files
#include <fs3_controller.h>

// Defines
#define FS3_MAX_BACKLOG 5
#define FS3_NET_HEADER_SIZE sizeof(FS3CmdBlk)
#define FS3_DEFAULT_IP "127.0.0.1"
#define FS3_DEFAULT_PORT 22887


// Global data
extern unsigned char *fs3_network_address;     // Address of FS3 server
extern unsigned short fs3_network_port;        // Port of FS3 server

//
// Functional Prototypes

int network_fs3_syscall(FS3CmdBlk cmd, FS3CmdBlk *ret, void *buf);
	// This is the client/network system call for communicating with controller


FS3CmdBlk construct_network_fs3_cmdblock(uint8_t op, uint16_t sec, uint_fast32_t trk, uint8_t ret);
	// Constructs the correct command block to be sent back to the driver

int deconstruct_network_fs3_cmdblock(FS3CmdBlk cmdBlock, uint8_t op, uint16_t sec, uint32_t trk, uint8_t ret);
	// Deconstructs the command block that was sent by the driver
#endif
