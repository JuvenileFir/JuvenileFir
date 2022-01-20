# rm search.xls
rm search_merge.xls
rm /mnt/pmem0/testpool
for i in {1..1}
do
python3 NVMsearch.py 
done