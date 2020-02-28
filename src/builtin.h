#ifndef __builtin_H
#define __builtin_H

/*
 * builtin functions
 */
 
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include "list.h"
#include "esh.h"
#include "jobs.h"

/* forward declaration (compiler warning)*/
struct esh_pipeline;

/* checks if command is built in */
bool is_builtin(char** cmd);

/* runs built in commands */
void run_builtin(struct esh_pipeline *pipe);

/* built-in fg command: fg <jid> */
void fg_builtin(int jobId);

/* built-in bg command: bg <jid>*/
void bg_builtin(int jobId);

/* built-in kill command: kill <jid>*/
void kill_builtin(int jobId);

/* built-in stop command: stop <jid> */
void stop_builtin (int jobId);

#endif //__BUILTIN_H
