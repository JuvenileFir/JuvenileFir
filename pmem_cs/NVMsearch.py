import time
import timeit
import os

time_start = time.time()
# time1 = timeit.timeit()
# print(time1)

# TODO: timeit
width =32
code_width = "width_{}".format(width)

# compile file

max_val = 1 << width

targets = ""

for selectivity in range(0,100,10):
    target = int((max_val - 1) * selectivity / 100)
    targets += str(target)
    targets += ","

operator = "lt"
file= "data_1e6"

args = "-l {} -o {} -f {}".format(targets,operator,file)

output_file = "NVMsearch.log"
# print(args)
print("开始执行")
cmd = "sudo ./ct.out " + args  + " > " + output_file

os.system(cmd)

# time2 = timeit.timeit()
# print(time2)
time_end = time.time()


print("完成")
print("用时: %.2f秒"%(time_end - time_start))
