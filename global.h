/*******************************************************************
 * 文件名：global.h
 * 模块功能：该头文件包含全局宏定义 
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 *******************************************************************/
#ifndef GLOBAL_H
#define GLOBAL_H

/*
 * 错误缓冲区长度
 */
#define ERR_MSG_MAX 1024

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/*
 * 退出状态
 */
enum estatus
{
	EXIT_AC,		
	EXIT_PE,		
	EXIT_WA,
	EXIT_RE1,		/* 带提示并返回给用户的运行时错误 */
	EXIT_RE2,		/* 带提示不返回给用户的运行时错误 */
	EXIT_TLE,		
	EXIT_MLE,		
	EXIT_OLE,		

	EXIT_IE,		/* 程序内部错误					  */
	EXIT_EE,		/* 程序外部错误，如配置错误		  */
};

#endif
