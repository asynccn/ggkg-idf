#ifndef RA_FILTER_H
#define RA_FILTER_H

#include <stddef.h>

typedef struct
{
    size_t size;  //number of values used for filtering
    size_t index; //current value index
    size_t count; //value count
    int sum;
    int *values; //array to be filled with values
} ra_filter_t;

#endif
