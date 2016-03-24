#include <io.h>
#include <stdio.h>
#include <fcntl.h>
#include <windows.h>
#include "transport.h"

#define BUFSIZE 4096

void start_typeperf(int);
void ReadFromPipe(void);
void ErrorExit(PTSTR);
int init_stats(statistics *);
int init_win(int);
char* _fgets(char* _buf, size_t size, FILE* stream);

HANDLE g_hChildStd_IN_Rd = NULL;
HANDLE g_hChildStd_IN_Wr = NULL;
HANDLE g_hChildStd_OUT_Rd = NULL;
HANDLE g_hChildStd_OUT_Wr = NULL;
FILE* from_typeperf = NULL;


#define TOTAL_WINDOWS_STATS 28
statistics stats[TOTAL_WINDOWS_STATS];

int get_stats(statistics** pstats, int period){
	int idx = 0;
	static int stats_count = 0;
	char buf[1024], *tok;
	if(from_typeperf == NULL){
		memset(stats, 0, sizeof(statistics)*TOTAL_WINDOWS_STATS);
		init_win(period);
		stats_count = init_stats(stats);
	}
	assert(stats_count);

	_fgets(buf,4096,from_typeperf);
	//omit timestamp
	tok = strtok(buf, "\",");
	tok = strtok(NULL, "\",");
	while (tok){
		if(0 < sscanf(tok,"%lf",&(stats[idx].value_double))){
			idx++;
		}
		tok = strtok(NULL, "\",");
	}
	assert(idx == stats_count);
	*pstats = stats;
	return idx;
}

char* _fgets(char* _buf, size_t size, FILE* stream)
{
	char * buf = fgets(_buf, size, stream);
	int len = strlen(buf);

	while (buf[len-1] != '\n'){
		Sleep(100);
		buf += len;
		buf = fgets(buf, size, stream);
		len = strlen(buf);
	}

	return _buf;
}

void alpha_strcpy(char* src, char* dest)
{
	char* a;
	int backslashes = 0;
	while (backslashes <3){
		if (*src == '\\'){
			backslashes++;
		}
		src++;
	}
	for (a = src; *a; a++){
		if (isalpha(*a)){
			*dest = *a;
			dest++;
		}
	}
	*dest = 0;
}

int init_stats(statistics * stats)
{
	char buf[4096], *tok;
	int idx = 0;
	//read empty line
	fgets(buf,4096,from_typeperf);
	//read line with columns
	fgets(buf,4096,from_typeperf);
	//omit timestamp column
	tok = strtok(buf, ",");
	tok = strtok(NULL, ",");
	while (tok){
		stats[idx].type = DOUBLE_TYPE;
		alpha_strcpy(tok, stats[idx++].name);
		tok = strtok(NULL, ",");
	}
	return idx;
}

int init_win(int period)
{
	SECURITY_ATTRIBUTES saAttr;

	// Set the bInheritHandle flag so pipe handles are inherited.

	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;

	// Create a pipe for the child process's STDOUT.

	if ( ! CreatePipe(&g_hChildStd_OUT_Rd, &g_hChildStd_OUT_Wr, &saAttr, 0) )
		ErrorExit(TEXT("StdoutRd CreatePipe"));
	//VERY DARK HANDLE-to-FD-to-FILESTRUCTURE magic
	from_typeperf = _fdopen(_open_osfhandle((long)g_hChildStd_OUT_Rd, _O_RDONLY), "r");

	// Ensure the read handle to the pipe for STDOUT is not inherited.

	if ( ! SetHandleInformation(g_hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0) )
		ErrorExit(TEXT("Stdout SetHandleInformation"));

	// Create a pipe for the child process's STDIN.

	if (! CreatePipe(&g_hChildStd_IN_Rd, &g_hChildStd_IN_Wr, &saAttr, 0))
		ErrorExit(TEXT("Stdin CreatePipe"));

	// Ensure the write handle to the pipe for STDIN is not inherited.

	if ( ! SetHandleInformation(g_hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0) )
		ErrorExit(TEXT("Stdin SetHandleInformation"));

	// Create the child process.

	start_typeperf(period);

	// The remaining open handles are cleaned up when this process terminates.
	// To avoid resource leaks in a larger application, close handles explicitly.

	return 0;
}

void start_typeperf(int period)
{
	char szFormatCmdline[]="typeperf \
\"\\IPv4\\Datagrams/sec\" \
\"\\Memory\\% Committed Bytes In Use\" \
\"\\Memory\\Committed Bytes\" \
\"\\Memory\\Demand Zero Faults/sec\" \
\"\\Memory\\Page Faults/sec\" \
\"\\Objects\\Semaphores\" \
\"\\Processor(_Total)\\% C1 Time\" \
\"\\Processor(_Total)\\% DPC Time\" \
\"\\Processor(_Total)\\% Interrupt Time\" \
\"\\Processor(_Total)\\% Privileged Time\" \
\"\\Processor(_Total)\\% Processor Time\" \
\"\\Processor(_Total)\\% User Time\" \
\"\\Processor(_Total)\\C1 Transitions/sec\" \
\"\\Processor(_Total)\\DPC Rate\" \
\"\\Processor(_Total)\\DPCs Queued/sec\" \
\"\\Processor(_Total)\\Interrupts/sec\" \
\"\\System\\Context Switches/sec\" \
\"\\System\\File Control Operations/sec\" \
\"\\System\\File Data Operations/sec\" \
\"\\System\\File Read Operations/sec\" \
\"\\System\\File Write Operations/sec\" \
\"\\System\\System Calls/sec\" \
\"\\System\\Threads\" \
\"\\System\\Processor Queue Length\" \
\"\\TCPv4\\Connections Established\" \
\"\\TCPv4\\Segments/sec\" \
\"\\WFPv4\\Active Inbound Connections\" \
\"\\WFPv4\\Active Outbound Connections\" \
";
	char szCmdline[sizeof(szFormatCmdline)+14];
	snprintf(szCmdline, sizeof(szCmdline), "%s -si %i", szFormatCmdline, period);
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	BOOL bSuccess = FALSE;

	ZeroMemory( &piProcInfo, sizeof(PROCESS_INFORMATION) );

	ZeroMemory( &siStartInfo, sizeof(STARTUPINFO) );
	siStartInfo.cb = sizeof(STARTUPINFO);
	siStartInfo.hStdError = g_hChildStd_OUT_Wr;
	siStartInfo.hStdOutput = g_hChildStd_OUT_Wr;
	siStartInfo.hStdInput = g_hChildStd_IN_Rd;
	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
	bSuccess = CreateProcess(NULL,
		szCmdline,	   // command line
		NULL,		   // process security attributes
		NULL,		   // primary thread security attributes
		TRUE,		   // handles are inherited
		0,			   // creation flags
		NULL,		   // use parent's environment
		NULL,		   // use parent's current directory
		&siStartInfo,  // STARTUPINFO pointer
		&piProcInfo);  // receives PROCESS_INFORMATION

// If an error occurs, exit the application.
if ( ! bSuccess )
	ErrorExit(TEXT("CreateProcess"));
else
{
	CloseHandle(piProcInfo.hProcess);
	CloseHandle(piProcInfo.hThread);
}
}

void ErrorExit(PTSTR message)
{
	fprintf(stderr,"error: %s", message);
	ExitProcess(1);
}
