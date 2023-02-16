/***************************************************************
 * 文件名：tester.h
 * 模块功能：准备用户程序的每组测试，并检测其测试结果
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 **************************************************************/
#include "global.h"
#include "case.h"
#include "data.h"
#include "exit.h"
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef TESTER_H
#define TESTER_H

/*
 * 该结构体包含了测试用户程序的所有输入条件
 */
struct condition
{
	int time;				/* 时间限制，单位毫秒 */ 
	int memory;				/* 内存限制，单位kb */
	int fsize;				/* 输出限制，单位kb */
	int who;
	const char *basedir;	/* 工作和根目录 */
	const char *datadir;	/* 数据目录，相对basedir */
	const char *magic;		/* 用于临时文件名 */
	char * const *command;	/* 待测试的命令 */
};
void tester_start(struct condition *cond);

#endif
