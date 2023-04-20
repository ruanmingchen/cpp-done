#ifndef CPU_LOAD_H
#define CPU_LOAD_H
#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
struct cpu_stat_info {
    long unsigned utime, ntime, stime, itime;
    long unsigned iowtime, irqtime, sirqtime;
    long unsigned totaltime;
};
/**
 * * 获取系统cpu 负载
 * @param last_cpu_stat_p 上一次cpu负载信息
 * @return cpu 负载
 * **/
inline float getSystemCpuLoad(struct cpu_stat_info* last_cpu_stat_p)
{
    float cpu_load = 0.0;
    FILE* file;
    do {
        if (NULL == last_cpu_stat_p) {
            fprintf(stderr, "paramter is invaild.");
            break;
        }
        struct cpu_stat_info cur_cpu_stat;

        file = fopen("/proc/stat", "r");
        if (!file) {
            fprintf(stderr, "Could not open /proc/stat.");
            break;
        }

        fscanf(file, "cpu  %lu %lu %lu %lu %lu %lu %lu", &cur_cpu_stat.utime, &cur_cpu_stat.ntime, &cur_cpu_stat.stime,
               &cur_cpu_stat.itime, &cur_cpu_stat.iowtime, &cur_cpu_stat.irqtime, &cur_cpu_stat.sirqtime);
        fclose(file);
        cur_cpu_stat.totaltime = cur_cpu_stat.utime + cur_cpu_stat.ntime + cur_cpu_stat.stime + cur_cpu_stat.itime
                                 + cur_cpu_stat.iowtime + cur_cpu_stat.irqtime + cur_cpu_stat.sirqtime;

        cpu_load = (1
                    - (float)(cur_cpu_stat.itime - last_cpu_stat_p->itime)
                          / (cur_cpu_stat.totaltime - last_cpu_stat_p->totaltime))
                   * 100;
        (*last_cpu_stat_p) = cur_cpu_stat;
    } while (0);
    return cpu_load;
}
#ifdef __cplusplus
}
#endif
#endif  // CPU_LOAD_H
