#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt/C++ 代码量统计工具
用法：python count_lines.py [项目根目录]
"""

import os
import sys
import unicodedata
from collections import defaultdict

# 需要统计的文件扩展名（Qt 常见类型）
CPP_EXTS = {'.cpp', '.cxx', '.cc', '.c', '.h', '.hpp', '.hxx', '.qml', '.ui', '.pro', '.pri'}

# 默认忽略的目录名
IGNORE_DIRS = {'.git', 'build', 'debug', 'release', '__pycache__', 'node_modules', '3rdparty', 'third_party'}


def str_display_width(s):
    """计算字符串的终端显示宽度（中文/全角字符占 2 列）"""
    w = 0
    for ch in s:
        if unicodedata.east_asian_width(ch) in ('F', 'W'):
            w += 2
        else:
            w += 1
    return w


def ljust(s, width):
    """左对齐，按显示宽度补齐空格"""
    return s + ' ' * max(0, width - str_display_width(s))


def rjust(s, width):
    """右对齐，按显示宽度补齐空格"""
    return ' ' * max(0, width - str_display_width(s)) + s


def count_lines_in_file(filepath):
    """
    统计单个文件的行数
    返回: (总行数, 空行数, 注释行数, 代码行数)
    """
    total = 0
    blank = 0
    comment = 0
    in_block_comment = False

    try:
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                total += 1
                stripped = line.strip()

                if not stripped:
                    blank += 1
                    continue

                if in_block_comment:
                    comment += 1
                    if '*/' in stripped:
                        in_block_comment = False
                    continue

                if '/*' in stripped:
                    comment += 1
                    if '*/' not in stripped:
                        in_block_comment = True
                    continue

                if stripped.startswith('//'):
                    comment += 1
                    continue
    except Exception:
        return 0, 0, 0, 0

    code = total - blank - comment
    return total, blank, comment, code


def main():
    if len(sys.argv) > 1:
        root = sys.argv[1]
    else:
        root = os.getcwd()

    if not os.path.isdir(root):
        print(f"错误：目录不存在 - {root}")
        sys.exit(1)

    stats = defaultdict(lambda: [0, 0, 0, 0, 0])

    total_files = 0
    total_all = 0
    total_blank = 0
    total_comment = 0
    total_code = 0

    print(f"正在扫描目录: {os.path.abspath(root)}")

    # 列宽定义
    W_EXT = 10
    W_NUM = 10

    separator = '-' * (W_EXT + W_NUM * 5)
    print(separator)

    # 表头
    print(
        ljust('扩展名', W_EXT)
        + rjust('文件数', W_NUM)
        + rjust('总行数', W_NUM)
        + rjust('空行数', W_NUM)
        + rjust('注释行', W_NUM)
        + rjust('代码行', W_NUM)
    )
    print(separator)

    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]

        for filename in filenames:
            ext = os.path.splitext(filename)[1].lower()
            if ext not in CPP_EXTS:
                continue

            filepath = os.path.join(dirpath, filename)
            total, blank, comment, code = count_lines_in_file(filepath)

            ext_stats = stats[ext]
            ext_stats[0] += 1
            ext_stats[1] += total
            ext_stats[2] += blank
            ext_stats[3] += comment
            ext_stats[4] += code

            total_files += 1
            total_all += total
            total_blank += blank
            total_comment += comment
            total_code += code

    # 输出分类统计
    for ext in sorted(stats.keys()):
        files, t, b, c, code = stats[ext]
        print(
            ljust(ext, W_EXT)
            + rjust(str(files), W_NUM)
            + rjust(str(t), W_NUM)
            + rjust(str(b), W_NUM)
            + rjust(str(c), W_NUM)
            + rjust(str(code), W_NUM)
        )

    print(separator)
    print(
        ljust('合计', W_EXT)
        + rjust(str(total_files), W_NUM)
        + rjust(str(total_all), W_NUM)
        + rjust(str(total_blank), W_NUM)
        + rjust(str(total_comment), W_NUM)
        + rjust(str(total_code), W_NUM)
    )
    print()
    if total_all > 0:
        code_percent = total_code / total_all * 100
        comment_percent = total_comment / total_all * 100
        blank_percent = total_blank / total_all * 100
        print(f"代码占比: {code_percent:.1f}%  |  注释占比: {comment_percent:.1f}%  |  空行占比: {blank_percent:.1f}%")
    print("统计完成！")


if __name__ == '__main__':
    main()