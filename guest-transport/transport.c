#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "transport.h"
#include <unistd.h>
#include <string.h>

enum LINUX_STATS{
	loadavg_currExisting = 0,
	loadavg_runQJobs01,
	diskstats_ms_writing,
	diskstats_IOs_in_process,
	meminfo_Writeback,
	meminfo_SUnreclaim,
	meminfo_PageTables,
	meminfo_Mapped,
	meminfo_Dirty,
	TOTAL_LINUX_STATS
};

statistics stats[TOTAL_LINUX_STATS] = {
		{"loadavg_currExisting", INTEGER_TYPE, .value_int = -1},
		{"loadavg_runQJobs01", DOUBLE_TYPE, .value_double = -1},
		{"diskstats_ms_writing", INTEGER_TYPE, .value_int = -1},
		{"diskstats_IOs_in_process", INTEGER_TYPE, .value_int = -1},
		{"meminfo_Writeback",INTEGER_TYPE, .value_int = -1},
		{"meminfo_SUnreclaim",INTEGER_TYPE, .value_int = -1},
		{"meminfo_PageTables",INTEGER_TYPE, .value_int = -1},
		{"meminfo_Mapped",INTEGER_TYPE, .value_int = -1},
		{"meminfo_Dirty",INTEGER_TYPE, .value_int = -1},
		};

int get_stats(statistics** pstats, int period){
	FILE* proc_loadavg;
	FILE* proc_diskstats;
	FILE* proc_meminfo;
	long temp_diskstats_write_ms = 0, temp_diskstats_IOs_in_process = 0;
	char buf[1024], *tok;
	usleep (period * 1000000);
	proc_loadavg = fopen("/proc/loadavg","r");
//start parse loadavg
	fscanf(proc_loadavg,"%lf %*f %*f %*i/%li %*i",
		&(stats[loadavg_runQJobs01].value_double),
		&(stats[loadavg_currExisting].value_int)
		);
	fclose(proc_loadavg);
//end parse loadavg
//start parse diskstat
	proc_diskstats = fopen("/proc/diskstats","r");
	stats[diskstats_ms_writing].value_int = 0;
	stats[diskstats_IOs_in_process].value_int = 0;
	while(!feof(proc_diskstats)){
		fscanf(proc_diskstats,"%*i%*i%*s%*[ ]%*i%*i%*i%*i%*i%*i%*i%li%li%*i%*i",
		&temp_diskstats_write_ms, &temp_diskstats_IOs_in_process);
		stats[diskstats_ms_writing].value_int += temp_diskstats_write_ms;
		stats[diskstats_IOs_in_process].value_int += temp_diskstats_IOs_in_process;
	}
	fclose(proc_diskstats);
//end parse diskstat
//start parse meminfo
	proc_meminfo = fopen("/proc/meminfo","r");
	while(!feof(proc_meminfo)){
		fgets(buf, 1024, proc_meminfo);
		tok = strtok(buf, " ");
		if(strcmp("Writeback:", tok) == 0){
			tok = strtok(NULL, " ");
			stats[meminfo_Writeback].value_int = strtol(tok, NULL, 10);
		}else if(strcmp("SUnreclaim:", tok) == 0){
			tok = strtok(NULL, " ");
			stats[meminfo_SUnreclaim].value_int = strtol(tok, NULL, 10);
		}else if(strcmp("PageTables:", tok) == 0){
			tok = strtok(NULL, " ");
			stats[meminfo_PageTables].value_int = strtol(tok, NULL, 10);
		}else if(strcmp("Mapped:", tok) == 0){
			tok = strtok(NULL, " ");
			stats[meminfo_Mapped].value_int = strtol(tok, NULL, 10);
		}else if(strcmp("Dirty:", tok) == 0){
			tok = strtok(NULL, " ");
			stats[meminfo_Dirty].value_int = strtol(tok, NULL, 10);
		}
	}
//end parse meminfo
	*pstats = stats;
	return TOTAL_LINUX_STATS;
}

int main(int argc, char* argv[]){
	statistics *stats = NULL;
	int period;
	int stats_count, i;
	if (argc == 22){
		period = atoi(argv[1]);
	}else{
		return 1;
	}
	while (1){
		stats_count = get_stats(&stats, period);
		for (i = 0; i < stats_count; i++){
			if (stats[i].type == INTEGER_TYPE){
				printf("%s:%li ",stats[i].name,stats[i].value_int);
			}else if (stats[i].type == DOUBLE_TYPE){
				printf("%s:%lf ",stats[i].name,stats[i].value_double);
			}
		}
		printf("\n");
		fflush(stdout);
	}
	return 1;
}
