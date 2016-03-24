#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <windows.h>
#include "transport.h"

#define BUFSIZE 4096

extern FILE* from_typeperf;

void start_typeperf(int);
void ReadFromPipe(void);
void ErrorExit(PTSTR);
int init_stats(statistics *);
int init_win(int);
