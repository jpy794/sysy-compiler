import os
import sys
import timeit
import shutil
import argparse
import subprocess

test_dir = os.path.dirname(os.path.abspath(__file__))  # 当前文件夹路径 tests/5-bonus
cminus = os.path.join(test_dir, '../../build/cminusfc')  # ===可修改===
testfile_dir = os.path.join(test_dir, './testcases')
output_file_name = os.path.join(test_dir, './test_result')
io = os.path.join(test_dir, '../../src/io/io.c')

total_failed_count = 0


class Outputer:
    def __init__(self, console=False, filename="test_result") -> None:
        self.console = console
        self.fd = open(filename, "a")

    def write(self, msg):
        if self.console:
            print(msg, end="")
            sys.stdout.flush()
        self.fd.write(msg)
        self.fd.flush()

    def __del__(self) -> None:
        self.fd.close()


def eval(console=False, test_dir=testfile_dir, use_clang=False):

    output_file = Outputer(console, output_file_name)
    failed_count = 0
    succ_count = 0
    total_time = 0

    single_begin = timeit.default_timer()

    testfiles = os.listdir(testfile_dir)
    #  testfiles.sort()
    # 过滤出以.cminus结尾的file
    testfiles = filter(lambda s: s.endswith('.cminus'), testfiles)
    testfiles = list(testfiles)

    test_count = len(testfiles)

    testtime = []

    for count, file_name in enumerate(testfiles):
        start_time = timeit.default_timer()

        testtime.append(-1)

        # 超时，跳过
        if start_time - total_start > 30 * 60 or start_time - single_begin > 30 * 60:
            output_file.write(f"[{count+1}/{test_count}] " + file_name +
                              ': skipped due to exceeded total time limit\n')
            continue
        # 未超时
        output_file.write(f"[{count+1}/{test_count}] " + file_name + ': ')
        filepath = os.path.join(testfile_dir, file_name)
        outpath = os.path.join(testfile_dir, file_name[:-7] + '.out')

        ### 编译 ###
        if not use_clang:
            try:
                # ===可修改===
                compile_res = subprocess.run([cminus, '-mem2reg', filepath, '-S',  'a.s'],
                                             stdout=subprocess.PIPE,
                                             stderr=subprocess.PIPE,
                                             timeout=300)
            except subprocess.TimeoutExpired as _:
                output_file.write('compile-1 timeout\n')
                failed_count += 1
                continue
            except Exception as e:
                output_file.write("compile-1 failed with an unexcept error\n")
                output_file.write(str(e))
                failed_count += 1
                continue

            try:
                compile_res = subprocess.run(['gcc', 'a.s', io, '-o', 'a.out'],
                                             stdout=subprocess.PIPE,
                                             stderr=subprocess.PIPE,
                                             timeout=300)
            except subprocess.TimeoutExpired as _:
                output_file.write('compile-2 timeout\n')
                failed_count += 1
                continue
            except Exception:
                output_file.write("compile-2 failed with an unexcept error\n")
                failed_count += 1
                continue

        else:
            try:
                cfilepath = filepath.replace(".cminus", ".c")
                shutil.move(filepath, cfilepath)
                compile_res = subprocess.run(["clang", cfilepath, io, "-o", "a.out"],
                                             stdout=subprocess.PIPE,
                                             stderr=subprocess.PIPE,
                                             timeout=300)
                shutil.move(cfilepath, filepath)
            except subprocess.TimeoutExpired as _:
                output_file.write('compile timeout\n')
                failed_count += 1
                continue
            except Exception as e:
                output_file.write("compile failed with an unexcept error\n")
                output_file.write(e)
                failed_count += 1
                continue

        ### 运行 ###
        try:
            input_option = None
            inpath = os.path.join(testfile_dir, file_name[:-7] + '.in')

            if os.path.exists(inpath):  # testfile存在输入文件
                with open(inpath, 'rb') as fin:
                    input_option = fin.read()

            # 记录运行时间
            start = timeit.default_timer()
            for _ in range(10):
                exe_res = subprocess.run(['./a.out'],
                                         input=input_option,
                                         stdout=subprocess.PIPE,
                                         stderr=subprocess.PIPE,
                                         timeout=100)
            end = timeit.default_timer()

        except subprocess.TimeoutExpired:
            output_file.write("executable time limit exceeded\n")
            failed_count += 1
            continue
        except Exception as e:
            output_file.write('executable runtime error\n')
            output_file.write(str(e) + '\n')
            failed_count += 1
            continue

        # 输出
        with open(outpath, 'r') as fout:
            ref = fout.read().replace(' ', '').replace('\n', '')

            try:
                actual = exe_res.stdout.decode('utf-8').replace(' ', '').replace('\n', '').replace('\r', '') + str(
                    exe_res.returncode)
            except UnicodeDecodeError:
                output_file.write('executable output illegal characters\n')
                failed_count += 1
                continue

            if ref == actual or use_clang:
                time = (end - start) / 10
                total_time += time
                output_file.write(f'pass, costs {time:.6f}s\n')
                succ_count += 1
                testtime[count] = time
            else:
                # 因为退出码也会作为输出的一部分，因此输出和答案不同可能是程序崩溃造成的
                output_file.write(
                    'output is different from standard answer, this may be caused by wrong return code\n')
                output_file.write("\t" + ref + "\n")
                output_file.write("\t" + actual + "\n")
                failed_count += 1

    output_file.write(f"{failed_count} tests failed\n")
    output_file.write(
        f"total time is {total_time}s\navg time is {total_time/succ_count if succ_count>0 else 0}s\n{succ_count} tests finishes in time limit\n"
    )

    output_file.write('testcase')
    output_file.write('\t\t\tyour_cminus')
    for count, file_name in enumerate(testfiles):
        output_file.write('{:<20}'.format(file_name))
        output_file.write(
            '\t\t %.6f' % testtime[count] if testtime[count] != -1 else '\t\t  None  ')

    output_file.write(
        "===================================================================\n")


if __name__ == "__main__":
    total_start = timeit.default_timer()

    parser = argparse.ArgumentParser(description="functional test")

    parser.add_argument("--console", action="store_true",
                        help="specify whether to output the result to console")
    parser.add_argument("--clang", action="store_true",
                        help="estimate runtime when compile with clang")
    args = parser.parse_args()

    eval(args.console, testfile_dir, use_clang=args.clang)
