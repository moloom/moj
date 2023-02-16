/*************************************************
 * 源文件：child.c
 * 版本：v0.1.0
 * 最后修改：2012-08-18
 ************************************************/
#include "child.h"

/*
 * 局部函数声明
 */
static int child_redirect_io(int infd, int outfd, char *errmsg);
static int child_set_directory(const char *basedir, char *errmsg);
static int child_set_rlimit(int time, int fsize, char *errmsg);
static int child_set_permission(int who, char *errmsg);

/*
 * 接口函数：child_run_process
 * 功能：在父进程fork之后，接管子进程，做相应设置后，执行用户程序
 * 参数：chd包含了该模块所需要的全部数据，见child.h的结构体定义
 * 返回值：无
 * 退出值：在execve之前出错，退出值为1，错误信息写入管道；
 * 	 execve错误，退出值为2
 *	 execve执行成功，则无退出值。
 */
void
child_run_process(struct childin *chd)
{
	char errmsg[ERR_MSG_MAX];
	struct rlimit reslim;

	close(chd->pfd[0]);

	/* 调用下面四个函数对子进程进行相应设置 */
	if (child_redirect_io(chd->infd, chd->outfd, errmsg) != 0)
		goto errexit;
	if (child_set_directory(chd->basedir, errmsg) != 0) 
		goto errexit;
	if (child_set_rlimit(chd->time, chd->fsize, errmsg) != 0)
		goto errexit;
	if (child_set_permission(chd->who, errmsg) != 0)
		goto errexit;

	/* 声明被父进程跟踪 */
	if (ptrace(PTRACE_TRACEME, 0, 0, 0) == -1) {
		snprintf(errmsg, ERR_MSG_MAX, 
				"**child_run_process** ptrace error: %s",
				strerror(errno));
		goto errexit;
	}

	close(chd->pfd[1]);
	
	/* 执行用户程序，函数返回则代表出错 */
	execvp(chd->command[0], chd->command);
	exit(2);

errexit:
	write(chd->pfd[1], errmsg, strlen(errmsg));
	close(chd->pfd[1]);
	exit(1);
}

/*
 * 局部函数：child_redirect_io
 * 功能：对标准输入，标准输出进行重定向，关闭标准出错
 * 参数：infd, outfd用于输入输出的重定向，errmsg接收错误信息
 * 返回值：成功返回0，出错返回-1，错误信息写到errmsg
 */
static int
child_redirect_io(int infd, int outfd, char *errmsg)
{
	if (dup2(infd, STDIN_FILENO) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_redirect_io** dup2[1] error: %s",
				strerror(errno));
		return -1;
	}
	close(infd);

	if (dup2(outfd, STDOUT_FILENO) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_redirect_io** dup2[2] error: %s",
				strerror(errno));
		return -1;
	}
	close(outfd);

	close(STDERR_FILENO);

	return 0;
}

/*
 * 局部函数：child_set_directory
 * 功能：设置子进程的工作目录和根目录
 * 参数：basedir即是工作目录也是跟目录，errmsg接收错误信息
 * 返回值：成功返回0，错误返回-1，错误信息写道errmsg
 */
static int
child_set_directory(const char *basedir, char *errmsg)
{
	if (chdir(basedir) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_directory** chdir error: %s",
				strerror(errno));
		return -1;
	}

	/* 切换到超级用户，注意父进程中是暂时放弃了超级用户权限的 */
	/*
	if (setreuid(geteuid(), getuid()) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_directory** setreuid[1] error: %s",
				strerror(errno));
		return -1;
	}
	*/

	/* chroot需要超级权限 */
	/*
	if (chroot(basedir) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_directory** chroot error: %s",
				strerror(errno));
		return -1;
	}
	*/

	/* 切换回到普通用户 */
	/*
	if (setreuid(geteuid(), getuid()) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_directory** setreuid[2] error: %s",
				strerror(errno));
		return -1;
	}
	*/

	return 0;
}

/*
 * 局部函数：child_set_rlimit
 * 功能：设置子进程资源限制，包括：CPU, FIZE, CORE
 * 参数：time为用户进程的时间限制，fsize为输出文件限制，errmsg接收错误
 * 返回值：成功返回0，错误返回-1，错误信息写到errmsg
 */
static int
child_set_rlimit(int time, int fsize, char *errmsg)
{
	struct rlimit reslim;
	
	/* 不产生core文件 */
	reslim.rlim_cur = reslim.rlim_max = 0;
	if (setrlimit(RLIMIT_CORE, &reslim) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_rlimit** setrlimit[1] error: %s",
				strerror(errno));
		return -1;
	}

	/* 输出文件大小限制，fsize的单位为kb，转换为bytes */
	reslim.rlim_cur = reslim.rlim_max = fsize * 1024;
	if (setrlimit(RLIMIT_FSIZE, &reslim) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_rlimit** setrlimit[2] error: %s",
				strerror(errno));
		return -1;
	}

	/* 软限制必须比设置值大，因为最后要减去execl之前的时间 */
	if (time % 1000 == 0)
		reslim.rlim_cur = time / 1000 + 1;
	else
		reslim.rlim_cur = time / 1000 + 2;

	/* 比软限制大1秒，因为不希望接收到SIGKILL */
	reslim.rlim_max = reslim.rlim_cur + 1;
	if (setrlimit(RLIMIT_CPU, &reslim) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_rlimit** setrlimit[3] error: %s",
				strerror(errno));
		return -1;
	}

	return 0;
}

/*
 * 接口函数：child_set_perminssion
 * 功能：设置子进程的用户ID和组ID到一个低权限账户
 * 参数：who为用户ID和组ID，errmsg接收错误信息
 * 成功返回0，错误返回-1，错误写到errmsg
 * 注意：由于转换到超级用户，实际ID，有效ID和保存ID都会被设置
 */
static int
child_set_permission(int who, char *errmsg)
{
	/* 转换到超级用户 */
	if (setreuid(geteuid(), getuid()) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_permission** setreuid error: %s",
				strerror(errno));
		return -1;
	}

	/* 首先设置组ID，否则在用户ID设置后，无法设置组ID */
	if (setgid(who) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_permission** setgid error: %s",
				strerror(errno));
		return -1;
	}
	if (setuid(who) == -1) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**child_set_permission** setuid error: %s",
				strerror(errno));
		return -1;
	}

	return 0;
}
