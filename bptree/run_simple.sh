g++ -std=c++11 simple_bptree.cc -pthread -fopenmp -mavx2 -D_GLIBCXX_PARALLEL -o simple_bpt 2> ./logs/make.log
# numactl --membind=0 ./simple_bpt -s 0.1,0.9 -r 1 -o eq > ./logs/simple_eq.log
# numactl --membind=0 ./simple_bpt -s 0.4999999 -r 2 -o bt > ./logs/simple_bt.log
numactl --membind=0 ./simple_bpt -s 0.99998 -r 2 -o gt
numactl --membind=0 ./simple_bpt -s 2e-5 -r 2 -o lt