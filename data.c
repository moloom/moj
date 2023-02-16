/*************************************************
 * 源文件：data.c
 * 版本：v0.1.0
 * 最后修改：2012-08-15
 ************************************************/
#include "data.h"

/*
 * 局部函数声明
 */
static int is_comment_line(const char *line);

/*
 * 局部数据：data_list和data_index
 * 作用：存储输入和答案文件路径，data_index为data_list中的元素个数
 * 被使用：dd_前缀的相关函数
 */
static char **data_list;
static int data_index;

/*
 * 局部函数：is_comment_line
 * 功能：判断字符串line是否是一个注释行
 * 参数：用户提供的字符串，不做参数检查
 * 返回值：是注释行返回1，否返回0
 * 判断方法：字符串首字符是'#'或者是'\n'则认为是注释行
 */
static int
is_comment_line(const char *line)
{
	if (line[0] == '#' || line[0] == '\n')
		return 1;
	else
		return 0;
}

/*
 * 接口函数：dd_init
 * 功能：从布局描述文件读取输入和答案文件的布局配置
 * 参数：ddpath为布局文件所在的目录，errmsg用于接收错误信息
 * 返回值：成功返回0，出错返回-1
 */
int
dd_init(const char *ddpath, char *errmsg)
{
	FILE *fd;
	int len, n, cnt = 0;	/* cnt为当前处理行号，从1开始 */
	int rest;  				/* 有多少行有效数据	*/
	char file[PATH_MAX];
	char *line, *tmp;
	char flag = 0;			/* 读到第一个有效数据行的标记 */
	
	if (strlen(ddpath) + strlen("/data.conf") >= PATH_MAX) {
		sprintf(errmsg,
				"**dd_init** %s too long.", ddpath);
		return -1;
	}

	if (ddpath[strlen(ddpath) - 1] == '/')
		sprintf(file, "%s%s", ddpath, "data.conf");
	else
		sprintf(file, "%s/%s", ddpath, "data.conf");

	fd = fopen(file, "r");
	if (fd == NULL) {
		snprintf(errmsg, ERR_MSG_MAX,
				"**dd_init** fopen %s error: %s",
				file, strerror(errno));
		return -1;
	}

	while (1) {
		len = getline(&line, &n, fd);
		if (len == -1)
			break;

		++cnt;

		if (is_comment_line(line))
			continue;

		/* 去掉最后的换行符 */
		line[len - 1] = '\0';
		--len;

		/* 读到第一行有效数据，表明接下来还有多少行有效数据 */
		if (flag == 0) {
			flag = 1;
			
			rest = atoi(line);
			data_list = (char **)malloc(sizeof(char *) * rest);
			if (data_list == NULL) {
				sprintf(errmsg, "**dd_init** malloc[1] error.");
				fclose(fd);
				free(line);
				return -1;
			}
			memset(data_list, 0, sizeof(sizeof(char *) * rest));
			continue;
		}

		/* 如果配置文件指定的行数跟实际的有效行数不同 */
		if (data_index >= rest)
			continue;

		tmp = (char *)malloc(len + 1);
		if (tmp == NULL) {
			sprintf(errmsg, "**dd_init** malloc[2] error.");
			fclose(fd);
			free(line);
			return -1;
		}

		strcpy(tmp, line);
		data_list[data_index++] = tmp;
	}

	fclose(fd);
	free(line);
	return 0;
}

/*
 * 接口函数：dd_end
 * 功能：释放为data_list分配的空间
 * 参数：无
 * 返回值：无
 */
void
dd_end()
{
	int i;

	if (data_list == NULL)
		return;

	for (i = 0; i < data_index; ++i)
		free(data_list[i]);
	free(data_list);
}

/*
 * 接口函数：dd_get_input
 * 功能：获取输入文件的文件名
 * 参数：index为匹配对的序号，每个匹配对由输入和答案文件组成
 * 返回值：如果序号有效，则返回字符串指针，否则返回空指针
 */
const char * 
dd_get_input(int index)
{
	if (index >= data_index / 2 || index < 0)
		return NULL;
	return data_list[index * 2];
}

/*
 * 接口函数：dd_get_answer
 * 功能：获取答案文件的文件名
 * 参数：index为匹配对的序号，每个匹配对由输入和答案文件组成
 * 返回值：如果序号有效，则返回字符串指针，否则返回空指针
 */
const char * 
dd_get_answer(int index)
{
	if (index >= data_index / 2 || index < 0)
		return NULL;
	return data_list[index * 2 + 1];
}

/*
 * 接口函数：dd_get_count
 * 功能：获取输入和答案的匹配对的个数
 * 参数：无
 * 返回值：返回匹配对个数
 */
int
dd_get_count()
{
	return data_index / 2;
}

/*
 * 测试函数：dd_test_print
 * 功能：打印从布局文件读取到的输入和答案文件
 * 参数：ddpath为布局文件路径
 * 返回值：无
 */
void
dd_test_print(const char *ddpath)
{
	int i;
	char errmsg[ERR_MSG_MAX];

	if (dd_init(ddpath, errmsg) != 0) {
		printf("%s\n", errmsg);
		dd_end();
		return;
	}	

	for (i = 0; i < dd_get_count(); ++i) {
		printf("%s\n", dd_get_input(i));
		printf("%s\n", dd_get_answer(i));
	}

	dd_end();
}
