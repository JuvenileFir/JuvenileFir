import time
import timeit
import os


# targets1 = [19980309, 19980804, 19980408, 19980804, 20010112, 20010112, 20010602, 19990501, 19980318, 19980804, 20010112, 19990201, 19990501, 19980318, 20020530]

targets1 = [20010910, 20011116, 20010616,20010713]


def singleTest(target1, data_file, idx):
    time1 = timeit.timeit()

    # compile file

    operator = "eq"

    # args = "-l {} -o {} -s".format(targets,operator)
    args = "-l {} -o {} -f {}".format(target1, operator, data_file)

    output_file = "./tpc_search3/tpc_search_{}.log".format(idx)

    print(args)

    cmd = "sudo ./ct.out  " + args + " > " + output_file

    # os.system("ls")
    os.system(cmd)

    time2 = timeit.timeit()

    print("time uesd: ", time2 - time1)


def batchTest(target1, data_file):
    time1 = timeit.timeit()

    code_width = "width_32"

    # compile file

    operator = "eq"

    atarget1 = ""
    for t1 in target1:
        atarget1 += str(t1)
        atarget1 += ","

    # args = "-l {} -o {} -s".format(targets,operator)
    args = "-l {} -o {} -f {}".format(atarget1, operator, data_file)

    output_file = "./tpc_search3/tpc_search_batch.log"

    print(args)

    cmd = "sudo ./ct.out  " + args + " > " + output_file

    # os.system("ls")
    os.system(cmd)

    time2 = timeit.timeit()

    print("time uesd: ", time2 - time1)


rm = "rm ./tpc_search3/*"
os.system(rm)

for i in range(0, len(targets1)):
    t1 = targets1[i]
    data_file = "tpc-ds-dateData"
    singleTest(t1, data_file, i)

batchTest(targets1, "tpc-ds-dateData")
