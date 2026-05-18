#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Qt/C++ 代码量统计工具
用法：python count_lines.py [项目根目录]
"""

import os
import sys
from collections import defaultdict

# 需要统计的文件扩展名（Qt 常见类型）
CPP_EXTS = {'.cpp', '.cxx', '.cc', '.c', '.h', '.hpp', '.hxx', '.qml', '.ui', '.pro', '.pri'}

# 默认忽略的目录名
IGNORE_DIRS = {'.git', 'build', 'debug', 'release', '__pycache__', 'node_modules', '3rdparty', 'third_party'}

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

                # 空行
                if not stripped:
                    blank += 1
                    continue

                # 处理块注释
                if in_block_comment:
                    comment += 1
                    if '*/' in stripped:
                        in_block_comment = False
                    continue

                # 检查是否开始块注释
                if '/*' in stripped:
                    comment += 1
                    if '*/' not in stripped:
                        in_block_comment = True
                    continue

                # 单行注释 //
                if stripped.startswith('//'):
                    comment += 1
                    continue

                # 如果包含 // 但不是开头，算作代码（可能行尾注释，这里保守算代码）
                # 我们只统计整行注释，不拆分混合行，以避免复杂化
                # 剩下的都是代码行
    except Exception:
        return 0, 0, 0, 0

    code = total - blank - comment
    return total, blank, comment, code


def main():
    # 确定扫描根目录
    if len(sys.argv) > 1:
        root = sys.argv[1]
    else:
        root = os.getcwd()

    if not os.path.isdir(root):
        print(f"错误：目录不存在 - {root}")
        sys.exit(1)

    # 存储统计结果：扩展名 -> [文件数, 总行, 空行, 注释行, 代码行]
    stats = defaultdict(lambda: [0, 0, 0, 0, 0])

    total_files = 0
    total_all = 0
    total_blank = 0
    total_comment = 0
    total_code = 0

    print(f"正在扫描目录: {os.path.abspath(root)}")
    print("-" * 70)

    for dirpath, dirnames, filenames in os.walk(root):
        # 过滤忽略目录
        dirnames[:] = [d for d in dirnames if d not in IGNORE_DIRS]

        for filename in filenames:
            ext = os.path.splitext(filename)[1].lower()
            if ext not in CPP_EXTS:
                continue

            filepath = os.path.join(dirpath, filename)
            total, blank, comment, code = count_lines_in_file(filepath)

            # 更新按扩展名的统计
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
    print(f"{'扩展名':<10}{'文件数':>8}{'总行数':>10}{'空行数':>10}{'注释行':>10}{'代码行':>10}")
    print("-" * 70)
    for ext in sorted(stats.keys()):
        files, t, b, c, code = stats[ext]
        print(f"{ext:<10}{files:>8}{t:>10}{b:>10}{c:>10}{code:>10}")

    print("-" * 70)
    print(f"{'合计':<10}{total_files:>8}{total_all:>10}{total_blank:>10}{total_comment:>10}{total_code:>10}")
    print()
    if total_all > 0:
        code_percent = total_code / total_all * 100
        comment_percent = total_comment / total_all * 100
        blank_percent = total_blank / total_all * 100
        print(f"代码占比: {code_percent:.1f}%  |  注释占比: {comment_percent:.1f}%  |  空行占比: {blank_percent:.1f}%")
    print("统计完成！")


if __name__ == '__main__':
    main()