import time
import timeit
import os

time_start = time.time()
# time1 = timeit.timeit()
# print(time1)

# TODO: timeit
# for width in (8,16,32):
width =32
code_width = "width_{}".format(width)

# compile file

max_val = 1 << width

targets = ""
file = "data_1e6"
afile = "data_1e6_2"
operator = "lt"
output_file = "append_{}.log".format(width)

# print(args)
print("开始执行")

for selectivity in range(10,101,10):
    args = "-o {} -f {} -i {} -p {}".format(operator,file,afile,selectivity)
    cmd = "./ct.out " + args  + " > " + output_file
    os.system(cmd)
    


# time2 = timeit.timeit()
# print(time2)
time_end = time.time()


print("完成")
print("用时: %.2f秒"%(time_end - time_start))
