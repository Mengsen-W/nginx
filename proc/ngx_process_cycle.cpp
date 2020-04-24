/*
 * @Author: Mengsen.Wang
 * @Date: 2020-04-24 19:50:07
 * @Last Modified by: Mengsen.Wang
 * @Last Modified time: 2020-04-24 19:50:28
 * @Description: 子进程
 */

#include <stdlib.h>
#include <signal.h>

static void ngx_start_worker_processes(int threadnums);
static int ngx_spawn_process(int threadnums, const char *pprocname);
static void ngx_worker_process_cycle(int inum, const char *pprocname);
static void ngx_worker_process_init(int inum);

static u_char master_process[] = "master process";

void ngx_master_process_cycle() {
  sigset_t set;

  sigemptyset(&set);
}