/***************************************************************
 * 文件名：data.h
 * 模块功能：读取输入和答案文件配置，为tester模块服务
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 **************************************************************/
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#ifndef DATA_H
#define DATA_H

/*
 * 数据目录相关
 */
int dd_init(const char *ddpath, char *errmsg);
void dd_end();
const char * dd_get_input(int index);
const char * dd_get_answer(int index);
int dd_get_count();

#endif
