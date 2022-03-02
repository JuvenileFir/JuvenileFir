import time
import timeit
import os


remove_number = []

with open("data_1e6.dat","rb") as f:
    for i in range(0,20):
        data = f.read(4)
        remove_number.append(int.from_bytes(data,"big"))
        



# time_start = time.time()
time1 = timeit.timeit()


# TODO: timeit


# compile file

max_val = 1 << 32

targets = ""

for target in remove_number:
    targets += str(target)
    targets += ","


targets = "464991503,436629184,216128874,2074191240,219235280,1096552648,1357213483,246522733,1777343703,212689257,987880815,801891781,377747449,1318382002,257845909,388306909,1523441031,2004319851,2075245555,1200758691,"

args = "-d {}".format(targets)

output_file = "remove.log"

build_file = "data_1e6.dat"

print(args)

cmd = "sudo ./bindex -f " + build_file + " -q " + args

# os.system("ls")
os.system(cmd)


time2 = timeit.timeit()
# time_end = time.time()

print("time uesd: ",time2 - time1)
