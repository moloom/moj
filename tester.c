/*************************************************
 * 源文件：tester.c
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 ************************************************/
#include "tester.h"

/*
 * 局部函数声明
 */
static void tester_exit(struct caseout *csout);
static void tester_test_print(struct casein *csin,
		struct caseout *csout);

/*
 * 接口函数：tester_start
 * 功能：调用case_run_test进行每组测试，并检测每组的测试结果
 * 参数：cond见tester.h头文件定义
 * 返回值：无
 */
void
tester_start(struct condition *cond)
{
	int i, cnt;
	int infd, outfd;		/* 用户程序的输入和输出文件描述符 */
	int maxtime = 0;		/* 所有组测试结果中最大的时间 */
	int maxmemory = 0;		/* 所有则测试结果中最大内存 */
	const char *infile;		/* 用户测试输入文件 */
	const char *ansfile;	/* 用户测试的答案文件或答案程序 */
	char outfile[PATH_MAX]; /* 用户程序的临时输出文件 */
	struct casein csin;		/* 单组测试函数中的参数 */
	struct caseout csout;	/* 单组测试函数中的返回结果 */

	if (dd_init(cond->datadir, csout.msg) != 0) {
		csout.code = EXIT_EE;
		tester_exit(&csout);
	}

	/* 填充csin结构体 */
	csin.command = cond->command;
	csin.time = cond->time;
	csin.memory = cond->memory;
	csin.fsize = cond->fsize;
	csin.basedir = cond->basedir;
	csin.who = cond->who;

	/* 创建临时输出文件，权限由屏蔽字限制 */
	if (cond->basedir[strlen(cond->basedir) - 1] != '/')
		snprintf(outfile, PATH_MAX, "%s/%s.out", cond->basedir, cond->magic);
	else
		snprintf(outfile, PATH_MAX, "%s%s.out", cond->basedir, cond->magic);

	outfd = open(outfile, O_CREAT | O_RDWR, S_IRWXU | S_IRWXG | S_IRWXO);
	if (outfd == -1) {
		csout.code = EXIT_IE;
		snprintf(csout.msg, ERR_MSG_MAX, "**tester_start** open %s error: %s",
				outfile, strerror(errno));
		tester_exit(&csout);
	}
	unlink(outfile);

	/* 准备调用case_run_test */
	cnt = dd_get_count();
	for (i = 0; i < cnt; ++i) {
		infile = dd_get_input(i);
		ansfile = dd_get_answer(i);

		/* 文件指针置0，截断长度为0 */
		if (lseek(outfd, 0, SEEK_SET) != 0) {
			close(outfd);
			csout.code = EXIT_IE;
			snprintf(csout.msg, ERR_MSG_MAX,
					"**tester_start** lseek %s error: %s",
					outfile, strerror(errno));
			tester_exit(&csout);
		}
		if (ftruncate(outfd, 0) == -1) {
			close(outfd);
			csout.code = EXIT_IE;
			snprintf(csout.msg, ERR_MSG_MAX,
					"**tester_start** truncate %s error: %s",
					outfile, strerror(errno));
			tester_exit(&csout);
		}

		/* 打开用户程序的输入文件 */
		infd = open(infile, O_RDONLY);
		if (infd == -1) {
			close(outfd);
			csout.code = EXIT_IE;
			snprintf(csout.msg, ERR_MSG_MAX,
					"**tester_start** open %s error: %s",
					infile, strerror(errno));
			tester_exit(&csout);
		}

		/* 填充csin结构体的剩余字段，调用case_run_test */
		csin.infd = infd;
		csin.outfd = outfd;
		csin.ansfile = ansfile;

		case_run_test(&csin, &csout);
		/* tester_test_print(&csin, &csout); */

		/*
		 * 关闭单组数据输入文件，
		 * 如果结果不正确，则关闭临时文件并退出
		 * 结果正确，则记录最大时间和内存使用
		 */
		close(infd);
		if (csout.code == EXIT_AC) {
			maxtime = csout.time > maxtime ? csout.time : maxtime;
			maxmemory = csout.memory > maxmemory ?
				csout.memory : maxmemory;
		} else {
			close(outfd);
			tester_exit(&csout);
		}
	}

	/* 所有的输入都测试正确 */
	close(outfd);
	csout.time = maxtime;
	csout.memory = maxmemory;
	tester_exit(&csout);
}

/*
 * 局部函数：tester_exit
 * 功能：根据测试结果，填写相应参数调用exit_func退出整个程序
 * 参数：csout见case.h头文件定义
 * 返回值：无
 */
static void
tester_exit(struct caseout *csout)
{
	/* 释放data模块的资源 */
	dd_end();

	switch (csout->code) {
		case EXIT_AC : exit_func(csout->code, csout->time, csout->memory);
					   break;

		case EXIT_PE : 
		case EXIT_WA : 
		case EXIT_TLE :
		case EXIT_MLE :
		case EXIT_OLE : exit_func(csout->code);
						break;

		case EXIT_IE :
		case EXIT_EE :
		case EXIT_RE1 :
		case EXIT_RE2 : exit_func(csout->code, csout->msg);
						break;
		
		/* 未知的退出状态，由exit_func进行处理 */
		default :		exit_func(csout->code);
						break;
	}
}

static void
tester_test_print(struct casein *csin,
		struct caseout *csout)
{
	char * const *i;
	printf("infd = %d, outfd = %d\n", csin->infd, csin->outfd);
	printf("time = %d, memory = %d, fsize = %d\n", 
			csin->time, csin->memory, csin->fsize);
	printf("who = %d\n", csin->who);
	printf("basedir = %s\n", csin->basedir);
	printf("ansfile = %s\n", csin->ansfile);
	printf("command = ");
	for (i = csin->command; *i != NULL; ++i)
		printf("%s ", *i);
	printf("\n");
	printf("\n");

	csout->code = EXIT_AC;
	csout->time = 0;
	csout->memory = 0;
	strcpy(csout->msg, "");
}
