/***************************************************************
 * 文件名：exit.h
 * 模块功能：打印结果并退出程序，模块提供的函数是程序的唯一出口
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 **************************************************************/
#include "global.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifndef EXIT_H
#define EXIT_H

void exit_func(enum estatus code, ...);

#endif
