/**
 * @file    shell_cfg_user.h
 * @brief   本工程 letter-shell 用户配置（覆盖 Lib 默认值）
 *
 * @note
 *   - 文件由shell_cfg.h通过 `#include SHELL_CFG_USER` 引入，勿删库默认头。
 *   - Keil Define须写：`SHELL_CFG_USER=\"shell_cfg_user.h\"`（引号要转义）。
 *   - 头保护勿用默认库的 `__SHELL_CFG_H__`（会被库头挡住）。
 */
#ifndef __SHELL_CFG_USER_H__
#define __SHELL_CFG_USER_H__

#include <stddef.h>
#include "stm32h7xx_hal.h"

/**
 * @brief 是否使用 shellTask 内部 while 循环
 * @note  1：`shellTask()` 内死循环读输入（适合 OS 独立任务）
 *        0：调用一次处理一轮（适合裸机在 main 里反复调 `shellTask()`）
 *        裸机，保持 0。
 */
#define SHELL_TASK_WHILE            0

/**
 * @brief 是否使用命令导出方式（SHELL_EXPORT_CMD 等）
 * @note  1：用导出宏注册命令；0：使用命令表 `shellCommandList`
 */
#define SHELL_USING_CMD_EXPORT      1

/**
 * @brief 是否使用 shell 伴生对象（文件系统、日志等扩展）
 */
#define SHELL_USING_COMPANION       0

/**
 * @brief 支持尾行模式（提示符在行尾刷新等）
 */
#define SHELL_SUPPORT_END_LINE      1

/**
 * @brief help 列表是否列出用户 / 变量 / 按键
 */
#define SHELL_HELP_LIST_USER        0
#define SHELL_HELP_LIST_VAR         0
#define SHELL_HELP_LIST_KEY         0

/**
 * @brief help 是否显示命令权限
 */
#define SHELL_HELP_SHOW_PERMISSION  0

/**
 * @brief 回车触发：LF / CR 可同时开；CRLF 不能与前两者同时开
 */
#define SHELL_ENTER_LF              1
#define SHELL_ENTER_CR              1
#define SHELL_ENTER_CRLF            0

/**
 * @brief 允许 `exec [addr] [args]` 直接执行地址上的函数（地址错误可能跑飞）
 */
#define SHELL_EXEC_UNDEF_FUNC       0

/**
 * @brief 命令参数最大个数（含命令名）
 */
#define SHELL_PARAMETER_MAX_NUMBER  8

/**
 * @brief 历史命令条数
 */
#define SHELL_HISTORY_MAX_NUMBER    5

/**
 * @brief 双击 Tab 判定间隔 (ms)，配合长帮助 / 快速帮助
 */
#define SHELL_DOUBLE_CLICK_TIME     200

/**
 * @brief 快速帮助：双击 Tab 直接显示帮助，不做命令补全
 */
#define SHELL_QUICK_HELP            1

/**
 * @brief 保存上一次命令返回值到变量 RETVAL
 */
#define SHELL_KEEP_RETURN_VALUE     0

/**
 * @brief 同时管理的最大 shell 实例数
 */
#define SHELL_MAX_NUMBER            5

/**
 * @brief 格式化输出缓冲；0 表示不使用 shell 格式化输出
 */
#define SHELL_PRINT_BUFFER          0

/**
 * @brief 格式化输入缓冲；0 表示关闭（格式化输入会阻塞 shellTask）
 */
#define SHELL_SCAN_BUFFER           0

/**
 * @brief 获取系统时间 (ms)
 * @note  用于双击 Tab、超时锁定等
 */
#include "perf_counter.h"
#define SHELL_GET_TICK()            get_system_ms()

/**
 * @brief 是否使用 shell 锁（多任务下需自行实现 lock/unlock）
 */
#define SHELL_USING_LOCK            0

/**
 * @brief 伴生对象用的内存分配；未用伴生对象时可置 0
 */
#define SHELL_MALLOC(size)          0
#define SHELL_FREE(obj)             0

/**
 * @brief 登录后是否打印 shell 信息 / 是否清屏
 */
#define SHELL_SHOW_INFO             1
#define SHELL_CLS_WHEN_LOGIN        0

/**
 * @brief 默认用户名与密码；密码为 "" 表示不校验
 */
#define SHELL_DEFAULT_USER          "admin"
#define SHELL_DEFAULT_USER_PASSWORD ""

/**
 * @brief 自动锁定超时（单位同 SHELL_GET_TICK）；0 表示关闭
 */
#define SHELL_LOCK_TIMEOUT          0 * 60 * 1000

/**
 * @brief 使用函数签名做参数转换（声明命令时指定签名，不匹配则不执行）
 */
#define SHELL_USING_FUNC_SIGNATURE  0

/**
 * @brief 支持数组参数（如 `cmd [1,2,3]`）；需同时开启函数签名与 MALLOC/FREE
 */
#define SHELL_SUPPORT_ARRAY_PARAM   0

#endif /* __SHELL_CFG_USER_H__ */
