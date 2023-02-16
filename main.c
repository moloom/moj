/*************************************************
 * 源文件：main.c
 * 模块功能：程序入口，进行参数解释和模块初始化
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 ************************************************/
#include "global.h"
#include "exit.h"
#include "tester.h"
#include <limits.h>

/*
 * 局部函数声明
 */
static int parse_arguments(int argc, char *argv[],
		struct condition *cond, char *errmsg);
static int
check_arguments(struct condition *cond, char *errmsg);

/*
 * 主函数：main
 * 功能：检查程序的权限，进行参数解释，调用tester模块
 * 参数：不解释
 * 返回值：无
 */
int
main(int argc, char *argv[])
{
	struct condition cond;
	char errmsg[ERR_MSG_MAX];

	/* 保证euid为0，egid不为0 */
	if (geteuid() != 0) {
		sprintf(errmsg, "**main** euid != 0.");
		exit_func(EXIT_EE, errmsg);
	}
	if (getegid() == 0) {
		sprintf(errmsg, "**main** egid == 0.");
		exit_func(EXIT_EE, errmsg);
	}

	/* 保证ruid不为0，rgid不为0 */
	if (getuid() == 0) {
		sprintf(errmsg, "**main** uid == 0.");
		exit_func(EXIT_EE, errmsg);
	}
	if (getgid() == 0) {
		sprintf(errmsg, "**main** gid == 0.");
		exit_func(EXIT_EE, errmsg);
	}

	/* 暂时放弃超级权限 */
	if (setreuid(geteuid(), getuid()) == -1) {
		sprintf(errmsg, "**main** setreuid error: %s", strerror(errno));
		exit_func(EXIT_IE, errmsg);
	}

	/* 参数解释和检测 */
	if (parse_arguments(argc, argv, &cond, errmsg) != 0)
		exit_func(EXIT_EE, errmsg);

	tester_start(&cond);

	/* 程序不应该由这里结束 */
	return 1;
}

/*
 * 局部函数：parse_arguments
 * 功能：解释命令行参数
 * 参数：argc, argv同main函数的参数
 * 		cond为需要填充的结构体，见tester.h
 * 		errmsg为错误信息
 * 返回值：正确返回0，错误返回1，错误信息写入errmsg
 */
static int
parse_arguments(int argc, char *argv[],
		struct condition *cond, char *errmsg)
{
	int i;
	
	memset(cond, 0, sizeof(struct condition));
	for (i = 0; i < argc - 1; ++i) {
		if (strcmp(argv[i], "-t") == 0)
			cond->time = atoi(argv[++i]);
		else if (strcmp(argv[i], "-m") == 0)
			cond->memory = atoi(argv[++i]);
		else if (strcmp(argv[i], "-f") == 0)
			cond->fsize = atoi(argv[++i]);
		else if (strcmp(argv[i], "--who") == 0)
			cond->who = atoi(argv[++i]);

		else if (strcmp(argv[i], "--basedir") == 0)
			cond->basedir = argv[++i];
		else if (strcmp(argv[i], "--datadir") == 0)
			cond->datadir = argv[++i];
		else if (strcmp(argv[i], "--magic") == 0)
			cond->magic = argv[++i];
		
		else if (strcmp(argv[i], "--end") == 0)
			cond->command = argv + i + 1;
	}
	return check_arguments(cond, errmsg);
}

/*
 * 局部函数check_arguments
 * 功能：辅助parse_arguments进行参数检查
 * 参数：同parse_arguments
 * 返回值：同parse_arguments
 */
static int
check_arguments(struct condition *cond, char *errmsg)
{
	if (cond->time <= 0) {
		sprintf(errmsg, "**check_arguments** -t argument error.");
		return 1;
	}

	if (cond->memory <= 0) {
		sprintf(errmsg, "**check_arguments** -m argument error.");
		return 1;
	}

	if (cond->fsize <= 0) {
		sprintf(errmsg, "**check_arguments** -f argument error.");
		return 1;
	}

	if (cond->who <= 0) {
		sprintf(errmsg, "**check_arguments** --who argument error.");
		return 1;
	}

	if (cond->basedir == NULL) {
		sprintf(errmsg, "**check_arguments** --basedir argument error.");
		return 1;
	}

	if (cond->datadir == NULL) {
		sprintf(errmsg, "**check_arguments** --datadir argument error.");
		return 1;
	}

	if (cond->magic == NULL) {
		sprintf(errmsg, "**check_arguments** --magic argument error.");
		return 1;
	}

	if (cond->command == NULL) {
		sprintf(errmsg, "**check_arguments** --end argument error.");
		return 1;
	}

	return 0;
}
