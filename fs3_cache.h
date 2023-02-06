#ifndef FS3_CACHE_INCLUDED
#define FS3_CACHE_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.h
//  Description    : This is the interface for the sector cache in the FS3
//                   filesystem.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun 17 Oct 2021 09:36:52 AM EDT
//

// Include
#include <fs3_controller.h>

// Defines
#define FS3_DEFAULT_CACHE_SIZE 2048 // 256 cache entries, by default

//
// Cache Functions

int init_helper();
    //Helper function for init function

int fs3_init_cache(uint16_t cachelines);
    // Initialize the cache with a fixed number of cache lines

int fs3_close_cache(void);
    // Close the cache, freeing any buffers held in it

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf);
    // Put an element in the cache

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct);
    // Get an element from the cache (returns NULL if not found)

int fs3_log_cache_metrics(void);
    // Log the metrics for the cache 

#endif
