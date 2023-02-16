/*******************************************************************
 * 文件名：case.h
 * 模块功能：该模块负责对单组输入数据进行测试，并返回测试结果
 * 版本：v0.1.0
 * 最后修改：2012-08-14
 *******************************************************************/
#include "global.h"
#include "child.h"
#include "syscall_rule.h"
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <setjmp.h>
#include <sys/types.h>
#include <sys/ptrace.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <sys/user.h>

#ifndef CASE_H
#define CASE_H

/*
 * 提供给单组测试的数据
 */
struct casein
{
	int infd;				/* 用户程序输入文件描述符 */
	int outfd;				/* 用户程序临时输入文件描述符 */
	int time;				/* 时间限制，单位毫秒 */
	int memory;				/* 内存限制，单位kb */
	int fsize;				/* 输出限制，单位kb */
	int who;				/* 执行用户程序的uid和gid */
	const char *basedir;	/* 用户程序的工作和根目录 */
	char * const *command;	/* execve的参数 */
	const char *ansfile;	/* 用户程序答案文件路径 */
};

/*
 * 单组测试返回的结果
 */
struct caseout
{
	enum estatus code;		/* 退出代号，定义在exit.h */
	int time;				/* 单组测试中用户程序使用的时间 */
	int memory;				/* 单组测试中用户程序使用的内存 */
	char msg[ERR_MSG_MAX];		
};

void case_run_test(struct casein *csin, struct caseout *csout);

#endif
