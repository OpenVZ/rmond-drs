#ifndef TRANSPORT_H
#define TRANSPORT_H
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
enum stat_type{
	INTEGER_TYPE = 0,
	DOUBLE_TYPE
};

typedef struct {
	char name[50];
	enum stat_type type;
	union {
		long value_int;
		double value_double;
	};
} statistics;

int get_stats(statistics**, int);
#endif
