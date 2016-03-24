#include "transport.h"

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
