#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define MAXN 3000
#define MAXLEN 32
#define MAXVAR 3000

typedef enum {
    IR_ASSIGN,
    IR_BINARY,
    IR_PRINT
} IRKind;

typedef struct {
    IRKind kind;
    char result[MAXLEN];
    char arg1[MAXLEN];
    char op[4];
    char arg2[MAXLEN];
    int deleted;
} IR;

IR code[MAXN];
int n;

/* =========================================================
 * 工具函数
 * ========================================================= */

/* 判断字符串是否为十进制整数 */
int is_number(const char *s) {
    int i = 0;

    if (s == NULL || s[0] == '\0')
        return 0;

    if (s[0] == '+' || s[0] == '-') {
        i = 1;
        if (s[i] == '\0')
            return 0;
    }

    for (; s[i] != '\0'; i++) {
        if (!isdigit((unsigned char)s[i]))
            return 0;
    }

    return 1;
}

/* 安全判断一个字符串是否是变量名 */
int is_variable(const char *s) {
    int i;

    if (s == NULL || s[0] == '\0')
        return 0;

    if (!(isalpha((unsigned char)s[0]) || s[0] == '_'))
        return 0;

    for (i = 1; s[i] != '\0'; i++) {
        if (!(isalnum((unsigned char)s[i]) || s[i] == '_'))
            return 0;
    }

    return 1;
}

/* 将整数转换成字符串 */
void int_to_str(long long x, char *s) {
    sprintf(s, "%lld", x);
}

/* 判断两个字符串是否相同 */
int same_string(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* =========================================================
 * 输出
 * ========================================================= */

void print_program(void) {
    for (int i = 0; i < n; i++) {
        if (code[i].deleted)
            continue;

        if (code[i].kind == IR_ASSIGN) {
            printf("%s = %s\n",
                   code[i].result,
                   code[i].arg1);
        } else if (code[i].kind == IR_BINARY) {
            printf("%s = %s %s %s\n",
                   code[i].result,
                   code[i].arg1,
                   code[i].op,
                   code[i].arg2);
        } else if (code[i].kind == IR_PRINT) {
            printf("print %s\n", code[i].arg1);
        }
    }
}

/* =========================================================
 * 变量表
 * ========================================================= */

typedef struct {
    char name[MAXLEN];

    /* 是否已知为常量 */
    int is_const;
    long long const_value;

    /*
     * 是否是拷贝：
     * 例如 a = b，则 copy_target[a] = "b"
     */
    int is_copy;
    char copy_target[MAXLEN];
} VarInfo;

VarInfo vars[MAXVAR];
int var_count;

/* 清空环境 */
void clear_env(void) {
    var_count = 0;
}

/* 查找变量 */
int find_var(const char *name) {
    for (int i = 0; i < var_count; i++) {
        if (strcmp(vars[i].name, name) == 0)
            return i;
    }

    return -1;
}

/* 获取变量，不存在则创建 */
int get_var(const char *name) {
    int id = find_var(name);

    if (id != -1)
        return id;

    if (var_count >= MAXVAR)
        return -1;

    strcpy(vars[var_count].name, name);
    vars[var_count].is_const = 0;
    vars[var_count].const_value = 0;
    vars[var_count].is_copy = 0;
    vars[var_count].copy_target[0] = '\0';

    return var_count++;
}

/* 让变量失去原来的常量/拷贝信息 */
void kill_var(const char *name) {
    int id = get_var(name);

    if (id == -1)
        return;

    vars[id].is_const = 0;
    vars[id].is_copy = 0;
    vars[id].copy_target[0] = '\0';
}

/*
 * 当 source 被重新赋值时：
 *
 * a = b
 * b = 5
 *
 * 原来的 a -> b 拷贝关系失效。
 */
void invalidate_copies_from(const char *source) {
    for (int i = 0; i < var_count; i++) {
        if (vars[i].is_copy &&
            strcmp(vars[i].copy_target, source) == 0) {
            vars[i].is_copy = 0;
            vars[i].copy_target[0] = '\0';
        }
    }
}

/* 设置变量为常量 */
void set_const(const char *name, long long value) {
    int id = get_var(name);

    if (id == -1)
        return;

    /*
     * name 自己重新定义，因此原来的：
     * name = xxx
     * 必须失效。
     */
    vars[id].is_copy = 0;
    vars[id].copy_target[0] = '\0';

    vars[id].is_const = 1;
    vars[id].const_value = value;
}

/* 设置变量为拷贝 */
void set_copy(const char *name, const char *source) {
    int id = get_var(name);

    if (id == -1)
        return;

    vars[id].is_const = 0;

    if (strcmp(name, source) == 0) {
        vars[id].is_copy = 0;
        vars[id].copy_target[0] = '\0';
    } else {
        vars[id].is_copy = 1;
        strcpy(vars[id].copy_target, source);
    }
}

/*
 * 获取变量的常量值。
 * 返回 1 表示当前能确定为常量。
 */
int get_const_value(const char *name, long long *value) {
    int id = find_var(name);

    if (id == -1)
        return 0;

    if (!vars[id].is_const)
        return 0;

    *value = vars[id].const_value;
    return 1;
}

/*
 * 获取拷贝后的最终变量。
 *
 * 例如：
 * a = b
 * c = a
 *
 * 那么 c 可以传播成 b。
 */
int resolve_copy(const char *name, char *result) {
    char current[MAXLEN];
    int visited[MAXVAR] = {0};

    strcpy(current, name);

    while (1) {
        int id = find_var(current);

        if (id == -1 || !vars[id].is_copy) {
            strcpy(result, current);
            return 1;
        }

        if (id >= MAXVAR || visited[id]) {
            /*
             * 理论上不会出现拷贝环，
             * 这里防止异常情况导致死循环。
             */
            strcpy(result, current);
            return 1;
        }

        visited[id] = 1;
        strcpy(current, vars[id].copy_target);
    }
}

/* =========================================================
 * 解析输入
 * ========================================================= */

void parse_line(char *line, int index) {
    char lhs[MAXLEN];
    char arg1[MAXLEN];
    char op[MAXLEN];
    char arg2[MAXLEN];

    int cnt;

    code[index].deleted = 0;
    code[index].result[0] = '\0';
    code[index].arg1[0] = '\0';
    code[index].op[0] = '\0';
    code[index].arg2[0] = '\0';

    /*
     * 先判断 print。
     */
    if (sscanf(line, " print %31s", arg1) == 1) {
        code[index].kind = IR_PRINT;
        strcpy(code[index].arg1, arg1);
        return;
    }

    /*
     * 判断二元运算：
     * x = y op z
     */
    cnt = sscanf(line,
                 " %31s = %31s %31s %31s",
                 lhs, arg1, op, arg2);

    if (cnt == 4) {
        code[index].kind = IR_BINARY;

        strcpy(code[index].result, lhs);
        strcpy(code[index].arg1, arg1);
        strcpy(code[index].op, op);
        strcpy(code[index].arg2, arg2);

        return;
    }

    /*
     * 否则一定是：
     * x = y
     */
    cnt = sscanf(line,
                 " %31s = %31s",
                 lhs, arg1);

    if (cnt == 2) {
        code[index].kind = IR_ASSIGN;

        strcpy(code[index].result, lhs);
        strcpy(code[index].arg1, arg1);

        return;
    }
}

/* =========================================================
 * 1. 常量传播
 * ========================================================= */

/*
 * 在当前顺序扫描中维护：
 *
 * a = 10
 * b = a + 1
 *
 * b 中的 a 可以替换成 10。
 *
 * 注意：
 * 常量传播只负责传播常量，
 * 不负责计算 10 + 1。
 * 常量折叠在后面的步骤完成。
 */
int constant_propagation(void) {
    int changed = 0;

    clear_env();

    for (int i = 0; i < n; i++) {
        if (code[i].deleted)
            continue;

        if (code[i].kind == IR_PRINT) {

            if (is_variable(code[i].arg1)) {
                long long value;

                if (get_const_value(code[i].arg1, &value)) {
                    char tmp[MAXLEN];
                    int_to_str(value, tmp);

                    if (strcmp(code[i].arg1, tmp) != 0) {
                        strcpy(code[i].arg1, tmp);
                        changed = 1;
                    }
                }
            }

        } else if (code[i].kind == IR_ASSIGN) {

            if (is_variable(code[i].arg1)) {
                long long value;

                if (get_const_value(code[i].arg1, &value)) {
                    char tmp[MAXLEN];
                    int_to_str(value, tmp);

                    if (strcmp(code[i].arg1, tmp) != 0) {
                        strcpy(code[i].arg1, tmp);
                        changed = 1;
                    }

                    /*
                     * 当前赋值结果已经是确定常量。
                     */
                    invalidate_copies_from(code[i].result);
                    set_const(code[i].result, value);

                } else {
                    /*
                     * x = y，y 不是常量。
                     *
                     * x 自己重新定义，因此原来的 x 信息失效。
                     */
                    invalidate_copies_from(code[i].result);
                    kill_var(code[i].result);
                }
            } else if (is_number(code[i].arg1)) {

                long long value = atoll(code[i].arg1);

                invalidate_copies_from(code[i].result);
                set_const(code[i].result, value);

            } else {
                invalidate_copies_from(code[i].result);
                kill_var(code[i].result);
            }

        } else if (code[i].kind == IR_BINARY) {

            /*
             * 只传播操作数，不在这里做常量计算。
             */
            if (is_variable(code[i].arg1)) {
                long long value;

                if (get_const_value(code[i].arg1, &value)) {
                    char tmp[MAXLEN];
                    int_to_str(value, tmp);

                    if (strcmp(code[i].arg1, tmp) != 0) {
                        strcpy(code[i].arg1, tmp);
                        changed = 1;
                    }
                }
            }

            if (is_variable(code[i].arg2)) {
                long long value;

                if (get_const_value(code[i].arg2, &value)) {
                    char tmp[MAXLEN];
                    int_to_str(value, tmp);

                    if (strcmp(code[i].arg2, tmp) != 0) {
                        strcpy(code[i].arg2, tmp);
                        changed = 1;
                    }
                }
            }

            /*
             * 二元运算的结果在当前阶段还不能认为是常量，
             * 因为常量折叠属于下一阶段。
             */
            invalidate_copies_from(code[i].result);
            kill_var(code[i].result);
        }
    }

    return changed;
}

/* =========================================================
 * 2. 拷贝传播
 * ========================================================= */

/*
 * 处理：
 *
 * a = b
 * c = a + 1
 *
 * => c = b + 1
 *
 * 如果 b 后来被重新赋值，则 a -> b 的关系会失效。
 */
int copy_propagation(void) {
    int changed = 0;

    clear_env();

    for (int i = 0; i < n; i++) {
        if (code[i].deleted)
            continue;

        if (code[i].kind == IR_PRINT) {

            if (is_variable(code[i].arg1)) {
                char tmp[MAXLEN];

                resolve_copy(code[i].arg1, tmp);

                if (strcmp(tmp, code[i].arg1) != 0) {
                    strcpy(code[i].arg1, tmp);
                    changed = 1;
                }
            }

        } else if (code[i].kind == IR_ASSIGN) {

            /*
             * 先传播右值。
             */
            if (is_variable(code[i].arg1)) {
                char tmp[MAXLEN];

                resolve_copy(code[i].arg1, tmp);

                if (strcmp(tmp, code[i].arg1) != 0) {
                    strcpy(code[i].arg1, tmp);
                    changed = 1;
                }
            }

            /*
             * 当前语句重新定义 result，
             * 所以 result 之前作为别人 source 的关系失效。
             */
            invalidate_copies_from(code[i].result);

            if (is_variable(code[i].arg1)) {
                /*
                 * x = y
                 */
                set_copy(code[i].result, code[i].arg1);
            } else {
                /*
                 * x = 常量
                 * 拷贝传播阶段不负责建立常量信息。
                 */
                kill_var(code[i].result);
            }

        } else if (code[i].kind == IR_BINARY) {

            if (is_variable(code[i].arg1)) {
                char tmp[MAXLEN];

                resolve_copy(code[i].arg1, tmp);

                if (strcmp(tmp, code[i].arg1) != 0) {
                    strcpy(code[i].arg1, tmp);
                    changed = 1;
                }
            }

            if (is_variable(code[i].arg2)) {
                char tmp[MAXLEN];

                resolve_copy(code[i].arg2, tmp);

                if (strcmp(tmp, code[i].arg2) != 0) {
                    strcpy(code[i].arg2, tmp);
                    changed = 1;
                }
            }

            /*
             * 二元运算重新定义 result。
             */
            invalidate_copies_from(code[i].result);
            kill_var(code[i].result);
        }
    }

    return changed;
}

/* =========================================================
 * 3. 常量折叠
 * ========================================================= */

/*
 * 计算：
 * a + b
 * a - b
 * a * b
 * a / b
 */
long long calculate(long long a, long long b, const char *op) {
    if (strcmp(op, "+") == 0)
        return a + b;

    if (strcmp(op, "-") == 0)
        return a - b;

    if (strcmp(op, "*") == 0)
        return a * b;

    if (strcmp(op, "/") == 0)
        return a / b;

    return 0;
}

int constant_folding(void) {
    int changed = 0;

    for (int i = 0; i < n; i++) {
        if (code[i].deleted)
            continue;

        if (code[i].kind != IR_BINARY)
            continue;

        if (is_number(code[i].arg1) &&
            is_number(code[i].arg2)) {

            long long a = atoll(code[i].arg1);
            long long b = atoll(code[i].arg2);

            long long value = calculate(a, b, code[i].op);

            char tmp[MAXLEN];
            int_to_str(value, tmp);

            /*
             * 二元运算变成普通赋值：
             *
             * x = 2 + 3
             * =>
             * x = 5
             */
            code[i].kind = IR_ASSIGN;
            strcpy(code[i].arg1, tmp);
            code[i].op[0] = '\0';
            code[i].arg2[0] = '\0';

            changed = 1;
        }
    }

    return changed;
}

/* =========================================================
 * 4. 代数化简
 * ========================================================= */

/*
 * 按题目给出的规则进行：
 *
 * x = y + 0 -> x = y
 * x = 0 + y -> x = y
 *
 * x = y - 0 -> x = y
 *
 * x = y * 1 -> x = y
 * x = 1 * y -> x = y
 *
 * x = y * 0 -> x = 0
 * x = 0 * y -> x = 0
 *
 * x = y / 1 -> x = y
 *
 * 特别注意：
 * 0 / y 不做化简。
 */
int algebraic_simplification(void) {
    int changed = 0;

    for (int i = 0; i < n; i++) {
        if (code[i].deleted)
            continue;

        if (code[i].kind != IR_BINARY)
            continue;

        char *a = code[i].arg1;
        char *b = code[i].arg2;
        char *op = code[i].op;

        if (strcmp(op, "+") == 0) {

            if (strcmp(b, "0") == 0) {
                /*
                 * x = y + 0
                 * => x = y
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, a);
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;
            } else if (strcmp(a, "0") == 0) {
                /*
                 * x = 0 + y
                 * => x = y
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, b);
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;
            }

        } else if (strcmp(op, "-") == 0) {

            if (strcmp(b, "0") == 0) {
                /*
                 * x = y - 0
                 * => x = y
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, a);
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;
            }

            /*
             * 注意：
             * 0 - y 不能变成 y。
             */

        } else if (strcmp(op, "*") == 0) {

            if (strcmp(b, "1") == 0) {
                /*
                 * x = y * 1
                 * => x = y
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, a);
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;

            } else if (strcmp(a, "1") == 0) {
                /*
                 * x = 1 * y
                 * => x = y
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, b);
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;

            } else if (strcmp(a, "0") == 0 ||
                       strcmp(b, "0") == 0) {
                /*
                 * x = y * 0
                 * => x = 0
                 *
                 * x = 0 * y
                 * => x = 0
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, "0");
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;
            }

        } else if (strcmp(op, "/") == 0) {

            if (strcmp(b, "1") == 0) {
                /*
                 * x = y / 1
                 * => x = y
                 */
                code[i].kind = IR_ASSIGN;
                strcpy(code[i].arg1, a);
                code[i].op[0] = '\0';
                code[i].arg2[0] = '\0';

                changed = 1;
            }

            /*
             * 0 / y 不做处理。
             */
        }
    }

    return changed;
}

/* =========================================================
 * 5. 死代码删除
 * ========================================================= */

/*
 * 判断变量是否已经在 live 集合中。
 */
int is_live(char live[][MAXLEN], int live_count, const char *name) {
    for (int i = 0; i < live_count; i++) {
        if (strcmp(live[i], name) == 0)
            return 1;
    }

    return 0;
}

/* 加入 live 集合 */
void add_live(char live[][MAXLEN], int *live_count, const char *name) {
    if (!is_variable(name))
        return;

    if (is_live(live, *live_count, name))
        return;

    if (*live_count >= MAXVAR)
        return;

    strcpy(live[*live_count], name);
    (*live_count)++;
}

/* 从 live 集合删除 */
void remove_live(char live[][MAXLEN], int *live_count, const char *name) {
    for (int i = 0; i < *live_count; i++) {
        if (strcmp(live[i], name) == 0) {

            for (int j = i; j < *live_count - 1; j++) {
                strcpy(live[j], live[j + 1]);
            }

            (*live_count)--;
            return;
        }
    }
}

int dead_code_elimination(void) {
    char live[MAXVAR][MAXLEN];
    int live_count = 0;
    int changed = 0;

    /*
     * 从后往前扫描。
     */
    for (int i = n - 1; i >= 0; i--) {
        if (code[i].deleted)
            continue;

        if (code[i].kind == IR_PRINT) {

            /*
             * print 是有副作用的，绝对不能删除。
             * 但是 print 使用的变量必须加入 live。
             */
            add_live(live, &live_count, code[i].arg1);

        } else if (code[i].kind == IR_ASSIGN) {

            /*
             * 如果 result 后面没有使用，
             * 那么这个赋值就是死代码。
             */
            if (!is_live(live, live_count, code[i].result)) {

                code[i].deleted = 1;
                changed = 1;

            } else {

                /*
                 * 当前定义覆盖了 result 的旧值。
                 */
                remove_live(live, &live_count, code[i].result);

                /*
                 * 当前赋值使用 arg1，
                 * 所以 arg1 必须保留。
                 */
                add_live(live, &live_count, code[i].arg1);
            }

        } else if (code[i].kind == IR_BINARY) {

            if (!is_live(live, live_count, code[i].result)) {

                /*
                 * result 没有后续使用，
                 * 整条计算可以删除。
                 */
                code[i].deleted = 1;
                changed = 1;

            } else {

                /*
                 * 当前定义覆盖 result。
                 */
                remove_live(live, &live_count, code[i].result);

                /*
                 * 两个操作数都需要。
                 */
                add_live(live, &live_count, code[i].arg1);
                add_live(live, &live_count, code[i].arg2);
            }
        }
    }

    return changed;
}

/* =========================================================
 * 主函数
 * ========================================================= */

int main(void) {
    scanf("%d", &n);

    /*
     * 消耗第一行后面的换行符。
     */
    getchar();

    for (int i = 0; i < n; i++) {
        char line[256];

        if (fgets(line, sizeof(line), stdin) == NULL)
            break;

        parse_line(line, i);
    }

    /*
     * 按题目规定的顺序反复优化：
     *
     * 1. 常量传播
     * 2. 拷贝传播
     * 3. 常量折叠
     * 4. 代数化简
     * 5. 死代码删除
     *
     * 直到整轮没有任何变化。
     */
    while (1) {
        int changed = 0;

        changed |= constant_propagation();
        changed |= copy_propagation();
        changed |= constant_folding();
        changed |= algebraic_simplification();
        changed |= dead_code_elimination();

        if (!changed)
            break;
    }

    print_program();

    return 0;
}