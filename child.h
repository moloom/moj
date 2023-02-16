/*******************************************************************
 * 文件名：child.h
 * 模块功能：该模块在fork之后马上接管子进程，做完设置后执行用户程序
 * 版本：v0.1.0
 * 最后修改：2012-08-14
 *******************************************************************/
#include "global.h"
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/resource.h>

#ifndef CHILD_H
#define CHILD_H

/*
 * childin包含了该模块所需要的全部输入数据
 */
struct childin
{
	int infd;				/* 输入文件描述符 */
	int outfd;				/* 输出文件描述符 */
	int pfd[2];				/* 跟父进程通讯的管道 */
	int time;				/* 用户程序时间限制 */
	int fsize;				/* 用户程序文件输出限制 */
	const char *basedir;	/* 用户程序的工作目录和根目录 */
	char * const *command;	/* execve的参数 */
	int who;				/* 执行用户程序的uid和gid */
};
void child_run_process(struct childin *chd);

#endif
