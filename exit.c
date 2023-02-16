/*************************************************
 * 源文件：exit.c
 * 版本：v0.1.0
 * 最后修改：2012-08-18
 ************************************************/
#include "exit.h"

/*
 * 接口函数：exit_func
 * 功能：打印结果，退出程序
 * 参数：code表明退出状态，根据不同的状态，提取不同的参数
 * 返回值：无
 * 注意：应该先判断程序的退出值，0表示正常退出，否则出现了不可预知的错误
 */
void
exit_func(enum estatus code, ...)
{
	int time, memory;
	const char *msg;
	va_list ap;

	printf("%d\n", code);

	va_start(ap, code);
	switch (code) {
		/* 有三个参数 */
		case EXIT_AC :		printf("Accepted\n");
							time = va_arg(ap, int);
							memory = va_arg(ap, int);
							printf("%dms\n%dkb\n", time, memory);
							break;

		/* 只有一个参数 */
		case EXIT_PE : 		printf("Presentation Error\n"); 	break;
		case EXIT_WA :		printf("Wrong Answer\n"); 			break;
		case EXIT_TLE :		printf("Time Limit Exceeded\n"); 	break;
		case EXIT_MLE :		printf("Memory Limit Exceeded\n"); 	break;
		case EXIT_OLE : 	printf("Output Limit Exceeded\n"); 	break;

		/* 有两个参数 */
		case EXIT_IE :		printf("Internal Error\n%s\n",
									va_arg(ap, char *));
							break;
		case EXIT_EE :		printf("External Error\n%s\n",
									va_arg(ap, char *));
							break;
		case EXIT_RE1 :		printf("Runtime Error\n%s\n",
									va_arg(ap, char *));
							break;
		case EXIT_RE2 :		printf("Runtime Error\n%s\n",
									va_arg(ap, char *));
							break;
	}
	va_end(ap);

	/* 退出值为0表明程序正常退出 */
	exit(0);
}
