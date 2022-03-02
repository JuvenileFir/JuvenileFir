# sudo ./bindex -i -u > _split_baseline.log
# sudo ./bindex -i -u -j > _split_batch_insert.log
# sudo ./bindex -i -u -b -j > _split_batch_both.log

# sudo ./bindex -qi -u -j > _1bk_baseline.log
# sudo ./bindex -qi -u > _1bk_batch_insert.log
# sudo ./bindex -qi -u -b > _1bk_batch_both.log

# sudo ./bindex -qi -u -j > _1b_baseline.log
# sudo ./bindex -qi -u > _1b_batch_insert.log
# sudo ./bindex -qi -u -b > _1b_batch_both.log

# sudo ./bindex -i > _split_unsorted_nobatch.log
# sudo ./bindex -i -b > _split_unsorted_batch.log


sudo ./bindex -qi -u -j > _data_baseline.log
sudo ./bindex -qi -u > _data_batch_insert.log
sudo ./bindex -qi -u -b > _data_batch_both.log

# sudo ./bindex -i > _insert_unsorted_nobatch.log
# sudo ./bindex -i -j > _insert_unsorted_batch.log