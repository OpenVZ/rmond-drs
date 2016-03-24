#ifndef TRANSPORT_H
#define TRANSPORT_H
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
