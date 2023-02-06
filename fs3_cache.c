////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_cache.c
//  Description    : This is the implementation of the cache for the 
//                   FS3 filesystem interface.
//
//  Author         : Patrick McDaniel
//  Last Modified  : Sun 17 Oct 2021 09:36:52 AM EDT
//

// Includes
#include <cmpsc311_log.h>

// Project Includes
#include <fs3_cache.h>
#include <malloc.h>
#include <string.h>
#include <fs3_common.h>

//
// Support Macros/Data

//Create structure that houses the data that we will need for the cache
typedef struct{
    char *buf;
    int cacheTrk;
    int cacheSec;
    int callsSinceAccessed;
}cacheData;

//Create a global array variable that will hold the data init above
cacheData *cache;
int cacheSize;

//Need a global variable for cache access
int cacheAccessNum = 0;

//Need global variables for cache stats
int hits;
int misses;
int attempts;

//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : init_helper
// Description  : Sets up the initialized cache
//
// Inputs       : None
// Outputs      : 0 when complete

int init_helper(){
    for (int i=0; i<cacheSize; i++){
        cache[i].buf = NULL;
        cache[i].cacheTrk = 0;
        cache[i].cacheSec = 0;
        cache[i].callsSinceAccessed = 0;
    }
    return (0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_init_cache
// Description  : Initialize the cache with a fixed number of cache lines
//
// Inputs       : cachelines - the number of cache lines to include in cache
// Outputs      : 0 if successful, -1 if failure

int fs3_init_cache(uint16_t cachelines) {
    //Initializing the cache and allocating the correct memory size
    if (cachelines > 0){
        cache = malloc(sizeof(cacheData) * cachelines);
    }
    else{
        cache = malloc(sizeof(cacheData)* FS3_DEFAULT_CACHE_SIZE);
    }
    cacheSize = cachelines;
    init_helper();
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_close_cache
// Description  : Close the cache, freeing any buffers held in it
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_close_cache(void)  {
    //Freeing all the memory that was used
    for(int i=0; i<cacheSize; i++){
        if (cache[i].buf != NULL){
            free(cache[i].buf);
            cache[i].buf = NULL;
        }
    }
    return(0);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_put_cache
// Description  : Put an element in the cache
//
// Inputs       : trk - the track number of the sector to put in cache
//                sct - the sector number of the sector to put in cache
// Outputs      : 0 if inserted, -1 if not inserted

int fs3_put_cache(FS3TrackIndex trk, FS3SectorIndex sct, void *buf) {
    //Checking if cache line is already in the cache, if it is then update the buffer
    for (int i=0; i<cacheSize; i++){
        if ((cache[i].cacheTrk == trk) && (cache[i].cacheSec == sct)){
            if (cache[i].buf != NULL){
                memcpy(cache[i].buf, buf, FS3_SECTOR_SIZE);
                cache[i].callsSinceAccessed = cacheAccessNum;
                cacheAccessNum++;
                return (0);
            }
        }
    }
    //The cache line is not already in the cache so looking for an open line that can be used
    for (int i=0; i<cacheSize; i++){
        if (cache[i].buf == NULL){
            cache[i].buf = (char *)malloc(FS3_SECTOR_SIZE * sizeof(char));
            memcpy(cache[i].buf, buf, FS3_SECTOR_SIZE);
            cache[i].cacheTrk = trk;
            cache[i].cacheSec = sct;
            cache[i].callsSinceAccessed = cacheAccessNum;
            cacheAccessNum++;
            return (0);
        }
    }
    //No open lines that can be used so we must eject the LRU line and put the new line in place of the old one
    int oldestAccess = 0;
    int line = -1;
    for (int i=0; i<cacheSize; i++){
        if (oldestAccess == 0){
            oldestAccess = cache[i].callsSinceAccessed;
            line = i;
        }
        else if (cache[i].callsSinceAccessed < oldestAccess){
            line = i;
            oldestAccess = cache[i].callsSinceAccessed;
        }
    }
    if (line != -1){
        memcpy(cache[line].buf, buf, FS3_SECTOR_SIZE);
        cache[line].cacheTrk = trk;
        cache[line].cacheSec = sct;
        cache[line].callsSinceAccessed = cacheAccessNum;
        cacheAccessNum++;
    }
    return(-1);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_get_cache
// Description  : Get an element from the cache (
//
// Inputs       : trk - the track number of the sector to find
//                sct - the sector number of the sector to find
// Outputs      : returns NULL if not found or failed, pointer to buffer if found

void * fs3_get_cache(FS3TrackIndex trk, FS3SectorIndex sct)  {
    attempts++;
    for (int i=0; i<cacheSize; i++){
        if ((cache[i].cacheTrk == trk) && (cache[i].cacheSec == sct)){
            if (cache[i].buf != NULL){
                hits++;
                cache[i].callsSinceAccessed = cacheAccessNum;
                cacheAccessNum++;
                return cache[i].buf;
            }
        }
    }
    misses++;
    return(NULL);
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : fs3_log_cache_metrics
// Description  : Log the metrics for the cache 
//
// Inputs       : none
// Outputs      : 0 if successful, -1 if failure

int fs3_log_cache_metrics(void) {
    //Writing the metrics of the cache to the terminal
    logMessage(FS3SimulatorLLevel, "** FS3 Cache Metrics **");
    logMessage(FS3SimulatorLLevel, " Cache Attempts = [     %d]", attempts);
    logMessage(FS3SimulatorLLevel, " Hits =           [     %d]", hits);
    logMessage(FS3SimulatorLLevel, " Misses =         [     %d]", misses);
    float hitRatio = 100 * (((float)hits) / ((float)attempts));
    logMessage(FS3SimulatorLLevel, " Hit Ratio =      [   %%%.2f]", hitRatio);
    return(0);
}

