/*************************************************
 * 源文件：case.c
 * 版本：v0.1.0
 * 最后修改：2012-08-14
 ************************************************/
#include "case.h"

/*
 * 本模块的三个主要流程函数
 * case_wait_child, case_monitor_child, 和case_compare_answer
 * 使用该结构来存储临时结果
 */
struct chdstatus
{
	enum estatus code;
	char chdmsg[ERR_MSG_MAX];
};

/*
 * 提供给流程函数case_wait_child的数据
 */
struct waitin
{
	pid_t child;
	int msgfd[2];		/* 跟子进程通讯的管道描述符	*/
	int pre_time;		/* 子进程在执行execl之前使用的时间 */
	int pre_memory;		/* 子进程在执行execl之前使用的内存 */
	int lmt_memory;		/* 对用户程序限制的内存 */
};

/*
 * 提供给流程函数case_monitor_child的数据
 */
struct monitorin
{
	pid_t child;
	int lmt_time;		/* 对用户程序的时间限制	*/
	int lmt_memory;		/* 对用户程序的内存限制 */
	int lst_time;		/* 子进程总共使用的时间 */
	int lst_memory;		/* 子进程总共使用的内存 */
};

/*
 * 提供给流程函数case_compare_answer的数据
 */
struct comparein
{
	int outfd;				/* 用户程序临时输出文件描述符 */
	const char *ansfile;	/* 用户程序的答案文件（程序）路径 */
};

/*
 * 模块全局变量定义
 */
static jmp_buf jbuf;

/*
 * 局部函数声明
 */
static void case_wait_child(struct waitin *win, struct chdstatus *chds);
static void case_monitor_child(struct monitorin *min,
		struct chdstatus *chds);
static void case_compare_answer(struct comparein *cin,
		struct chdstatus *chds);

static void case_kill_child(pid_t child);
static int case_vmsize_ok(pid_t child, int memory);

static int case_signal_ok(int signo, enum estatus *code, char *errmsg);
static void case_sigalrm_handler(int signo);
static int case_memory_syscall(int scno);

static void case_compare_static(struct comparein *cin,
		struct chdstatus *chds);
static void case_compare_dynamic(struct comparein *cin,
		struct chdstatus *chds);
static enum estatus case_compare_string(const char *str1, int len1,
		const char *str2, int len2);
static int case_is_nonprint(char c);

/*
 * 接口函数：case_run_test
 * 功能：运行一次用户程序，对一组输入进行一次测试
 * 参数：csin, csout见case.h的定义
 * 返回值：无
 */
void
case_run_test(struct casein *csin, struct caseout *csout)
{
	pid_t pid;
	int pfd[2];
	int used_time;			/* 在该次测试中用户程序使用的时间 */
	int used_memory;		/* 在该次测试中用户程序使用的内存 */

	struct childin chdin; 	/* 该结构体是提供给child模块的数据 */
	struct waitin win;
	struct monitorin min;
	struct comparein cin;
	struct chdstatus chds;
	
	/* 创建一个和子进程通讯的管道 */
	if (pipe(pfd) == -1) {
		csout->code = EXIT_IE;
		snprintf(csout->msg, ERR_MSG_MAX,
				"**case_run_test** pipe error: %s",
				strerror(errno));
		return;
	}

	/* 填充chdin结构体，在fork之后马上调用child_run_process */
	chdin.infd = csin->infd;
	chdin.outfd = csin->outfd;
	chdin.pfd[0] = pfd[0];
	chdin.pfd[1] = pfd[1];
	chdin.time = csin->time;
	chdin.fsize = csin->fsize;
	chdin.basedir = csin->basedir;
	chdin.command = csin->command;
	chdin.who = csin->who;
	if ((pid = fork()) == -1) {
		csout->code = EXIT_IE;
		snprintf(csout->msg, ERR_MSG_MAX,
				"**case_run_test** fork error: %s",
				strerror(errno));
		close(pfd[0]);
		return;
	} else if (pid == 0) {
		child_run_process(&chdin);
	}

	/* 父进程，填充win结构体，调用case_wait_child */
	win.child = pid;
	win.msgfd[0] = pfd[0];
	win.msgfd[1] = pfd[1];
	win.lmt_memory = csin->memory;
	case_wait_child(&win, &chds);
	if (chds.code != EXIT_AC) {
		csout->code = chds.code;
		/* 不能确定chds是否有消息写入，只能全部内容拷贝过去 */
		memcpy(csout->msg, chds.chdmsg, ERR_MSG_MAX);
		return;
	}

	/* 填充min结构体，调用case_monitor_child */
	min.child = pid;
	min.lmt_time = csin->time;
	min.lmt_memory = csin->memory;
	case_monitor_child(&min, &chds);
	if (chds.code != EXIT_AC) {
		csout->code = chds.code;
		memcpy(csout->msg, chds.chdmsg, ERR_MSG_MAX);
		return;
	}

	/* 填充cin结构体，调用case_compare_answer */
	cin.outfd = csin->outfd;
	cin.ansfile = csin->ansfile;
	case_compare_answer(&cin, &chds);
	if (chds.code != EXIT_AC) {
		csout->code = chds.code;
		memcpy(csout->msg, chds.chdmsg, ERR_MSG_MAX);
		return;
	}

	/* 三个流程函数都得到了AC的结果，最后判断是否超时超内存 */
	used_time = min.lst_time - win.pre_time;
	if (used_time > csin->time) {
		csout->code = EXIT_TLE;
		return;
	}
	used_memory = min.lst_memory - win.pre_memory;
	if (used_memory > csin->memory) {
		csout->code = EXIT_MLE;
		return;
	}

	csout->code = EXIT_AC;
	csout->time = used_time;
	csout->memory = used_memory;
	return;
}

/*
 * 局部函数：case_wait_child
 * 功能：监控子进程在调用execl之前的状态，
 *   当子进程因为execve系统调用成功而停止的时候，该函数任务完成
 * 参数：win和chds见本文件的结构体定义
 * 返回值：无
 * 注意：该函数是模块三个主要流程函数之一
 */
static void 
case_wait_child(struct waitin *win, struct chdstatus *chds)
{
	int status; 					/* 子进程状态 */
	struct rusage used;				/* 子进程资源使用 */
	struct user_regs_struct preg;	/* sys/user.h	*/
	char errbuf[ERR_MSG_MAX];

	close(win->msgfd[1]);

	/* 只等待一次子进程状态，期待是由于execve而被SIGTRAP信号停止 */
	if (wait3(&status, 0, &used) == -1) {
		chds->code = EXIT_IE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_wait_child** wait3 error: %s",
				strerror(errno));
		case_kill_child(win->child);
		close(win->msgfd[0]);
		return;
	}

	/* 判断子进程状态 */
	if (WIFSTOPPED(status)) {
		/* 停止状态，获取系统调用号，判断停止信号 */
		if (ptrace(PTRACE_GETREGS, win->child, NULL, &preg) == -1) {
			chds->code = EXIT_IE;
			snprintf(chds->chdmsg, ERR_MSG_MAX,
					"**case_wait_child** ptrace[1] error: %s",
					strerror(errno));
			case_kill_child(win->child);
			close(win->msgfd[0]);
			return;
			
		}

		if (WSTOPSIG(status) != SIGTRAP || preg.orig_eax != __NR_execve) {
			chds->code = EXIT_IE;
			sprintf(chds->chdmsg,
					"**case_wait_child** child stopped: signal = %d, syscall = %ld",
					WSTOPSIG(status), preg.orig_eax); 
			case_kill_child(win->child);
			close(win->msgfd[0]);
			return;
		}

		/* 检测一次内存使用，因为可能在数据段超内存限制 */
		if (case_vmsize_ok(win->child, win->lmt_memory) == 0) {
			chds->code = EXIT_MLE;
			case_kill_child(win->child);
			close(win->msgfd[0]);
			return;
		}
		
		/* 继续子进程运行 */
		if (ptrace(PTRACE_SYSCALL, win->child, 0, 0) == -1) {
			chds->code = EXIT_IE;
			snprintf(chds->chdmsg, ERR_MSG_MAX,
					"**case_wait_child** ptrace[2] error: %s",
					strerror(errno));
			case_kill_child(win->child);
			close(win->msgfd[0]);
			return;
		}

		/* 子进程由于execve的成功调用而暂停了 */
		win->pre_time = used.ru_utime.tv_sec * 1000 +
			used.ru_utime.tv_usec / 1000 +
			used.ru_stime.tv_sec * 1000 +
			used.ru_stime.tv_usec / 1000;
		win->pre_memory = used.ru_minflt * getpagesize() / 1024;
		chds->code = EXIT_AC;
		
	} else if (WIFEXITED(status)) {
		/* 如果子进程退出，获取其退出值（定义在child.h）和退出信息 */
		chds->code = EXIT_IE;
		switch (WEXITSTATUS(status)) {
			case 1 :
				read(win->msgfd[0], chds->chdmsg, ERR_MSG_MAX);
				break;
			case 2 :
				sprintf(chds->chdmsg, "**case_wait_child** execve error.");
				break;
			default :
				sprintf(chds->chdmsg,
						"**case_wait_child** child exited: value = %d",
						WEXITSTATUS(status));
				break;
		}

	} else if (WIFSIGNALED(status)) {
		/* 如果子进程被信号终止 */
		chds->code = EXIT_IE;
		sprintf(chds->chdmsg,
				"**case_wait_child** child teminated: signal = %d",
				WTERMSIG(status));

	} else {
		/* 未知的子进程状态 */
		chds->code = EXIT_IE;
		sprintf(chds->chdmsg,
				"**case_wait_child** unknow child status: status = %d",
				status);
		case_kill_child(win->child);
	}

	close(win->msgfd[0]);
	return;
}

/*
 * 局部函数：case_monitor_child
 * 功能：在子进程execl用户程序之后，监控用户进程
 * 参数：min和chds见本文件中结构体的定义
 * 返回值：无
 * 注意：该函数是三个主要流程函数之一
 */
static void 
case_monitor_child(struct monitorin *min, struct chdstatus *chds)
{
	int status; 					/* 用户进程状态 */
	int endflag = 1; 				/* 系统调用退出标志，0为进入，1为退出 */
	int signo;						/* 用户进程收到的信号 */
	struct rusage used;				/* 用户进程资源使用 */
	struct user_regs_struct preg;

	/* 注册闹钟信号和定义超时闹钟，其值比用户设定大2.x秒 */
	if (signal(SIGALRM, case_sigalrm_handler) == SIG_ERR) {
		chds->code = EXIT_IE;
		sprintf(chds->chdmsg,
				"**case_monitor_child** signal SIGALARM error.");
		return;
	}

	if (min->lmt_time % 1000 == 0)
		alarm(min->lmt_time / 1000 + 2);
	else
		alarm(min->lmt_time / 1000 + 3);
	
	/* 闹钟信号函数回跳到这里 */
	if (setjmp(jbuf) != 0) {
		chds->code = EXIT_TLE;
		return;
	}

	/* 循环等待用户进程状态 */
	while (1) {
		if (wait3(&status, 0, &used) == -1) {
			alarm(0);
			case_kill_child(min->child);
			chds->code = EXIT_IE;
			snprintf(chds->chdmsg, ERR_MSG_MAX,
					"**case_monitor_child** wait3 error: %s",
					strerror(errno));
			return;
		}

		/* 判断用户进程状态 */
		if (WIFSIGNALED(status)) {
			alarm(0);
			chds->code = EXIT_RE2;
			sprintf(chds->chdmsg,
					"**case_monitor_child** child killed[1]: signal = %d",
					WTERMSIG(status));
			return;

		} else if (WIFSTOPPED(status) && WSTOPSIG(status) == SIGTRAP) { 
			endflag ^= 1;

			/* 如果是被SIGTRAP信号停止，则获取其系统调用号 */
			if (ptrace(PTRACE_GETREGS, min->child, NULL, &preg) == -1) {
				alarm(0);
				case_kill_child(min->child);
				chds->code = EXIT_IE;
				snprintf(chds->chdmsg, ERR_MSG_MAX,
						"**case_monitor_child** ptrace[1] error: %s",
						strerror(errno));
				return;
			}

			/* 只在进入系统调用的时候判断是否合法 */
			if (endflag == 0 && !syscall_is_valid(preg.orig_eax)) {
				alarm(0);
				case_kill_child(min->child);
				chds->code = EXIT_RE2;
				sprintf(chds->chdmsg,
					"**case_monitor_child** child killed[2]: syscall = %ld",
						preg.orig_eax);
				return;
			}

			/* 如果是系统调用退出并且是内存有关的系统调用 */
			if (endflag == 1 && case_memory_syscall(preg.orig_eax) &&
					!case_vmsize_ok(min->child, min->lmt_memory)) {
				alarm(0);
				case_kill_child(min->child);
				chds->code = EXIT_MLE;
				return;
			}

			/* 继续用户进程，无信号传递 */
			if (ptrace(PTRACE_SYSCALL, min->child, 0, 0) == -1) {
				alarm(0);
				case_kill_child(min->child);
				chds->code = EXIT_IE;
				snprintf(chds->chdmsg, ERR_MSG_MAX,
						"**case_monitor_child** ptrace[2] error: %s",
						strerror(errno));
				return;
			}

		} else if (WIFSTOPPED(status)) {
			/* 由信号停止 */
			signo = WSTOPSIG(status);
			if (!case_signal_ok(signo, &chds->code, chds->chdmsg)) {
				alarm(0);
				case_kill_child(min->child);
				return;
			}
			
			/* 继续用户进程，同样不传递信号 */
			if (ptrace(PTRACE_SYSCALL, min->child, 0, 0) == -1) {
				alarm(0);
				case_kill_child(min->child);
				chds->code = EXIT_IE;
				snprintf(chds->chdmsg, ERR_MSG_MAX,
						"**case_monitor_child** ptrace[3] error: %s",
						strerror(errno), signo);
				return;
			}

		} else if (WIFEXITED(status)) {
			/* 用户进程退出，记录资源使用 */
			alarm(0);
			min->lst_time = used.ru_utime.tv_sec * 1000 +
				used.ru_utime.tv_usec / 1000 +
				used.ru_stime.tv_sec * 1000 +
				used.ru_stime.tv_usec / 1000;
			min->lst_memory = used.ru_minflt * getpagesize() / 1024;
			chds->code = EXIT_AC;
			return; 

		} else {
			/* 未知的子进程状态 */
			alarm(0);
			case_kill_child(min->child);
			chds->code = EXIT_RE2;
			sprintf(chds->chdmsg,
					"**case_monitor_child** child killed: unknow status");
			return;
		} /* 子进程状态判断完毕 */
	} /* while循环结束 */
}

/*
 * 局部函数：case_compare_answer
 * 功能：判断用户程序的输出是否正确
 * 参数：cin和chds见本文件结构体定义
 * 返回值：无
 * 注意：该函数是模块三个主要流程函数之一，由两个辅助函数完成
 */
static void 
case_compare_answer(struct comparein *cin, struct chdstatus *chds)
{
	/* 如果是答案程序 */
	if (strcmp(cin->ansfile + strlen(cin->ansfile) - 4, ".exe") == 0)
		case_compare_dynamic(cin, chds);
	else
		case_compare_static(cin, chds);
}

/*
 * 局部函数：case_kill_child
 * 功能：发送SIGKILL信号结束子进程
 * 参数：child为子进程ID
 * 返回值：无
 * 注意：如果setreuid错误，则可能出现严重的后果；
 *   另SIGKILL不会被ptrace跟踪而使子进程暂定
 */
static void
case_kill_child(pid_t child)
{
	/* 无权限则提权再杀 */
	if (kill(child, SIGKILL) == -1 && errno == EPERM) {
		setreuid(geteuid(), getuid());
		kill(child, SIGKILL);
		setreuid(geteuid(), getuid());
	}
	return;
}

/*
 * 局部函数：case_vmsize_ok
 * 功能：读取子进程的虚拟内存使用，判断是否超过内存限制
 * 参数：child为子进程ID，memory为限制内存
 * 返回值：超内存则返回0，没有则返回1
 * 注意：如果打开/proc/<pid>/statm出错，则认为超内存，这个真不会处理
 */
static int
case_vmsize_ok(pid_t child, int memory)
{
	FILE *fd;
	int vmsize;
	char tmpbuf[64];
	
	sprintf(tmpbuf, "/proc/%d/statm", child);
	if ((fd = fopen(tmpbuf, "r")) == NULL)
		return 0;

	fscanf(fd, "%d", &vmsize);
	if (vmsize * getpagesize() / 1024 > memory) {
		fclose(fd);
		return 0;
	}

	fclose(fd);
	return 1;
}

/*
 * 局部函数：case_signal_ok
 * 功能：判断用户进程接收到的信号是否需要允许传递
 * 参数：signo信号编号，chds为流程函数的临时结果
 * 返回值：信号允许则返回1，不允许则返回0
 * 注意：不在该函数列出的信号都是默认允许的
 */
static int
case_signal_ok(int signo, enum estatus *code, char *errmsg)
{
	switch (signo) {
		case SIGXCPU : *code = EXIT_TLE; break;
		case SIGXFSZ : *code = EXIT_OLE; break;
		case SIGFPE  : *code = EXIT_RE1;
					   strcpy(errmsg, "Floating point exception");
					   break;
		case SIGSEGV : *code = EXIT_RE1;
					   strcpy(errmsg, "Invalid memory reference");
					   break;
		default : return 1;
	}

	return 0;
}

/*
 * 局部函数：case_sigalrm_handler
 * 功能：闹钟超时信号处理函数，回跳到case_monitor_child函数
 * 参数：signo为信号编号
 * 返回值：无
 * 注意：如果不是超时信号，则什么也不做
 */
static void
case_sigalrm_handler(int signo)
{
	if (signo == SIGALRM)
		longjmp(jbuf, 1);
}

/*
 * 局部函数：case_memory_syscall
 * 功能：判断一个系统调用号是否会改变虚拟内存大小
 * 函数：scno为系统调用号
 * 返回值：会改变内存大小则返回1，否则返回0
 * 注意：不在该函数指定的系统调用号都认为是不会改变内存大小
 */
static int
case_memory_syscall(int scno)
{
	switch (scno) {
		case __NR_mmap :
		case __NR_mmap2 :
		case __NR_munmap :
		case __NR_brk :
		case __NR_mremap : return 1;

		default : return 0;
	}
}

/*
 * 局部函数：case_compare_string
 * 功能：将两个字符串进行比较，判断是AC，PE还是WA
 * 参数：字符串str1长度为len1，字符串str2长度为len2
 * 返回值：enum estatus（定义在exit.h）中的AC，PE或者WA
 */
static enum estatus
case_compare_string(const char *str1, int len1, const char *str2, int len2)
{
	int i, j;

	/* 去掉字符串尾部的不可打印字符 */
	for (i = len1 - 1; i >= 0; --i)
		if (!case_is_nonprint(str1[i]))
				break;
	if (i < 0)
		return EXIT_WA;
	else
		len1 = i + 1;

	for (j = len2 - 1; j >= 0; --j)
		if (!case_is_nonprint(str2[j]))
			break;
	if (j < 0)
		return EXIT_WA;
	else
		len2 = j + 1;

	/* 进行一次整体比较，如果全部相同，则AC */
	for (i = 0, j = 0; i < len1 && j < len2; ++i, ++j)
		if (str1[i] != str2[j])
			break;
	if (i >= len1 && j >= len2)
		return EXIT_AC;

	/* 跳过不可打印字符进行一次比较，相同则PE，否则WA */
	for (i = 0, j = 0; i < len1 && j < len2;) {
		if (case_is_nonprint(str1[i])) {
			++i;
			continue;
		}
		if (case_is_nonprint(str2[j])) {
			++j;
			continue;
		}

		if (str1[i] != str2[j])
			return EXIT_WA;
		
		++i;
		++j;
	}

	/* 全部比较完才是PE，否则其中之一是子串，WA */
	if (i >= len1 && j >= len2)
		return EXIT_PE;
	
	return EXIT_WA;
}

/*
 * 局部函数：case_is_nonprint
 * 功能：判断一个字符是否是可打印字符
 * 参数：c为待判断的字符
 * 返回值：不可打印字符返回1，可打印返回0
 * 注意：仅tab，空格和换行视为不可打印字符
 */
static int
case_is_nonprint(char c)
{
	if (c == '\n' || c == '\t' || c == ' ')
		return 1;
	return 0;
}

/*
 * 局部函数：case_compare_dynamic
 * 功能：将用户程序的输出作为答案程序的输入，由答案程序判断结果
 * 参数：cin和chds见本文件结构体定义
 * 返回值：无
 * 注意：答案程序只能输出0，1，2分别代表AC，PE和WA；
 *   另外不应该使用标准出错；
 *   该函数是模块流程函数case_compare_answer的分支辅助函数
 */
static void
case_compare_dynamic(struct comparein *cin, struct chdstatus *chds)
{
	int pfd[2];		/* 用管道重定向答案程序的标准输出 */
	int status;		/* 答案程序的状态 */
	int rdcnt;		/* 父进程从管道读取到的字节数 */
	pid_t pid;		/* 答案程序的进程ID */
	char ret;		/* 答案程序的输出值 */

	if (pipe(pfd) == -1) {
		chds->code = EXIT_IE;
		sprintf(chds->chdmsg,
				"**case_compare_dynamic** pipe error: %s",
				strerror(errno));
		return;
	}

	if ((pid = fork()) == -1) {
		chds->code = EXIT_IE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_compare_dynamic** fork error: %s",
				strerror(errno));
		return;

	} else if (pid == 0) {
		close(pfd[0]);
		close(STDERR_FILENO);

		/* 权限设置，去掉隐含的超级特权，组权限不用改变 */
		if (setuid(getuid()) == -1) {
			write(pfd[1], "3", 1);
			exit(1);
		}

		/* 重定向输入和输出，注意不能关闭cin->outfd */
		if (dup2(cin->outfd, STDIN_FILENO) == -1) {
			write(pfd[1], "3", 1);
			exit(1);
		}

		if (dup2(pfd[1], STDOUT_FILENO) == -1) {
			write(pfd[1], "3", 1);
			exit(1);
		}
		
		close(pfd[1]);
		if (execl(cin->ansfile, cin->ansfile, (char *)0) == -1) {
			write(STDOUT_FILENO, "3", 1);
			exit(1);
		}
	}
	

	/* 父进程等待答案进程状态，期待是终止状态 */
	close(pfd[1]);

	if (setjmp(jbuf) != 0) {
		close(pfd[0]);
		kill(pid, SIGKILL);
		chds->code = EXIT_EE;
		sprintf(chds->chdmsg,
				"**case_compare_dynamic** answer program error: "
				"output too much");
		return;
	}

	/* 如果答案程序输出大于管道容量，则wait阻塞，直到闹钟超时 */
	alarm(5);
	wait(&status);
	alarm(0);
	
	/* 如果暂停了，则杀掉答案程序 */
	if (WIFSTOPPED(status))
		kill(pid, SIGKILL);

	/* 此时答案程序已经终止，如果没有数据，则会遇到结束符 */
	if ((rdcnt = read(pfd[0], &ret, 1)) != 1) {
		close(pfd[0]);
		chds->code = EXIT_EE;
		sprintf(chds->chdmsg,
				"**case_compare_dynamic** answer program error: no output");
		return;
	}

	switch (ret) {
		case '0' : chds->code = EXIT_AC; break;
		case '1' : chds->code = EXIT_PE; break;
		case '2' : chds->code = EXIT_WA; break;
		case '3' : chds->code = EXIT_IE;
				   sprintf(chds->chdmsg,
						   "**case_compare_dynamic** "
						   "answer program error: before execl");
				   break;
		default : chds->code = EXIT_EE;
				  sprintf(chds->chdmsg,
						   "**case_compare_dynamic** "
						   "answer program error: output unregonisable");
				  break;
				
	}
	close(pfd[0]);
	return;
}

/*
 * 局部函数：case_compare_static
 * 功能：用户程序的输出跟静态答案文件进行对比
 * 参数：cin和chds见本文件的结构体定义
 * 返回值：无
 * 注意：该函数是case_compare_answer的分支辅助函数，
 *   调用case_compare_string
 */
static void
case_compare_static(struct comparein *cin, struct chdstatus *chds)
{
	/* str1和st1是关于用户程序输出，str2和st2是答案文件 */
	int ansfd;
	char *str1, *str2;
	struct stat st1, st2;

	/* 打开答案文件错误的话，则只是简单的认为是外部错误 */
	if ((ansfd = open(cin->ansfile, O_RDONLY)) == -1) {
		chds->code = EXIT_EE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_compare_static** open %s error: %s",
				cin->ansfile, strerror(errno));
		return;
	}

	/* 获取文件大小 */
	if (fstat(cin->outfd, &st1) == -1) {
		chds->code = EXIT_IE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_compare_static** stat[1] error: %s",
				strerror(errno));
		return;
	}
	if (fstat(ansfd, &st2) == -1) {
		chds->code = EXIT_IE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_compare_static** stat[2] error: %s",
				strerror(errno));
		return;
	}

	/* 检测文件大小，如果用户程序无输出判WA，答案文件空判EE */
	if (st1.st_size == 0) {
		chds->code = EXIT_WA;
		return;
	}
	if (st2.st_size == 0) {
		chds->code = EXIT_EE;
		sprintf(chds->chdmsg,
				"**case_compare_static** no data in %s.", cin->ansfile);
		return;
	}

	/* 文件内存映射，假设了munmap总是成功的 */
	str1 = mmap(NULL, st1.st_size, PROT_READ, MAP_PRIVATE, cin->outfd, 0);
	if (str1 == MAP_FAILED) {
		chds->code = EXIT_IE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_compare_static** mmap[1] error: %s",
				strerror(errno));
		return;
	}
	str2 = mmap(NULL, st2.st_size, PROT_READ, MAP_PRIVATE, ansfd, 0);
	if (str2 == MAP_FAILED) {
		chds->code = EXIT_IE;
		snprintf(chds->chdmsg, ERR_MSG_MAX,
				"**case_compare_static** mmap[2] error: %s",
				strerror(errno));
		munmap(str1, st1.st_size);
		return;
	}

	chds->code = case_compare_string(str1, st1.st_size,
			str2, st2.st_size);

	munmap(str1, st1.st_size);
	munmap(str2, st2.st_size);
	return;
}
