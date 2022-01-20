rm append.xls
rm /mnt/pmem0/testpool
for i in {1..1}
do
python3 NVMappend.py 
done