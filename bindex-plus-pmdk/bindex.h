#ifndef BINDEX_H_
#define BINDEX_H_

#include <assert.h>
#include <immintrin.h>
#include <limits.h>
#include <omp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <algorithm>
#include <iostream>
#include <parallel/algorithm>
#include <random>
#include <sstream>
#include <string>
#include <thread>

// pmdk lib
#include <libpmemobj++/make_persistent.hpp>
#include <libpmemobj++/make_persistent_atomic.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/transaction.hpp>


#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <malloc.h>



using namespace std;
namespace pobj = pmem::obj;

#define DEBUG 0

#define MIN_POOL ((size_t)((size_t)1024 * (size_t)1024 * (size_t)1024 * (size_t)16))   //10 G

#define THREAD_NUM 36   // 36

#if !defined(WIDTH_4) && !defined(WIDTH_8) && !defined(WIDTH_12) && !defined(WIDTH_16) && !defined(WIDTH_20) && \
    !defined(WIDTH_24) && !defined(WIDTH_28) && !defined(WIDTH_32)
#define WIDTH_32
#endif

#ifndef VAREA_N
#define VAREA_N 128   // 128
#endif

#ifndef DATA_N
#define DATA_N 1e6  // 1e9  1e6
#endif

#define ROUNDUP_DIVIDE(x, n) ((x + n - 1) / n)
#define ROUNDUP(x, n) (ROUNDUP_DIVIDE(x, n) * n)
#define PRINT_EXCECUTION_TIME(msg, code)           \
  do {                                             \
    struct timeval t1, t2;                         \
    double elapsed;                                \
    gettimeofday(&t1, NULL);                       \
    do {                                           \
      code;                                        \
    } while (0);                                   \
    gettimeofday(&t2, NULL);                       \
    elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0;    \
    elapsed += (t2.tv_usec - t1.tv_usec) / 1000.0; \
    printf(msg " time: %f ms\n", elapsed);         \
  } while (0);


/*
  optional code width
*/
#ifdef WIDTH_4
typedef uint8_t CODE;
#define MAXCODE ((int8_t)((1 << 4) - 1))
#define MINCODE (int8_t)0
#define CODEWIDTH 4
#define LOADSHIFT 4
#endif

#ifdef WIDTH_8
typedef uint8_t CODE;
#define MAXCODE (int8_t) INT8_MAX
#define MINCODE (int8_t)0
#define CODEWIDTH 8
#define LOADSHIFT 0
#endif

#ifdef WIDTH_12
typedef uint16_t CODE;
#define MAXCODE ((int8_t)((1 << 12) - 1))
#define MINCODE (int16_t)0
#define CODEWIDTH 12
#define LOADSHIFT 4
#endif

#ifdef WIDTH_16
typedef uint16_t CODE;
#define MAXCODE (int16_t) INT16_MAX
#define MINCODE (int16_t)0
#define CODEWIDTH 16
#define LOADSHIFT 0
#endif

#ifdef WIDTH_20
typedef uint32_t CODE;
#define MAXCODE ((int8_t)((1 << 20) - 1))
#define MINCODE (int32_t)0
#define CODEWIDTH 20
#define LOADSHIFT 12
#endif

#ifdef WIDTH_24
typedef uint32_t CODE;
#define MAXCODE ((int8_t)((1 << 24) - 1))
#define MINCODE (int32_t)0
#define CODEWIDTH 24
#define LOADSHIFT 8
#endif

#ifdef WIDTH_28
typedef uint32_t CODE;
#define MAXCODE ((int8_t)((1 << 28) - 1))
#define MINCODE (int32_t)0
#define CODEWIDTH 28
#define LOADSHIFT 4
#endif

#ifdef WIDTH_32
typedef uint32_t CODE;
#define MAXCODE (int32_t) INT32_MAX
#define MINCODE (int32_t)0
#define CODEWIDTH 32
#define LOADSHIFT 0
#endif



#define LAYOUT "BinDexLayout"


typedef int POSTYPE;  // Data type for positions
// typedef int CODE;           // Codes are stored as int
typedef unsigned int BITS;  // 32 0-1 bit results are stored in a BITS
typedef uint16_t SLOT;
enum OPERATOR {
  EQ = 0,  // x == a
  LT,      // x < a
  GT,      // x > a
  LE,      // x <= a
  GE,      // x >= a
  BT,      // a < x < b
};

const int RUNS = 20;

const int BITSWIDTH = sizeof(BITS) * 8;
const int BITSSHIFT = 5;  // x / BITSWIDTH == x >> BITSSHIFT
const int SIMD_ALIGEN = 32;
const int SIMD_JOB_UNIT = 8;  // 8 * BITSWIDTH == __m256i

const int blockInitSize = 2048;  // TODO: tunable parameter for optimization 32 256 2048
const int blockMaxSize = blockInitSize * 2;
const int K = VAREA_N;  // Number of virtual areas
const int N = (int)DATA_N;
const int blockNumMax = (N / (K * blockInitSize)) * 4 * 85;

extern int prefetch_stride;


extern std::vector<CODE> target_numbers_l;  // left target numbers
extern std::vector<CODE> target_numbers_r;  // right target numbers

extern BITS *result1;
extern BITS *result2;

extern CODE *data_sorted;

extern bool BITMAP_USED;
extern bool BATCH_SPLIT;
extern bool SORTED;
extern bool BATCH_BLOCK_INSERT;

class pos_block {
    public:
    pobj::persistent_ptr<POSTYPE[]> pos;
    pobj::persistent_ptr<CODE[]> val;
    pobj::p<int> length;
    pobj::p<int> free_idx;   // point to the first free place in this block
    pobj::p<CODE> start_value;

    // CONTROL Unit
    pobj::persistent_ptr<BITS[]> bitmap_pos;   // blockMaxSize / BITWIDTH    
    // pobj::persistent_ptr<SLOT[]> slot_array;    // blockMaxSize


    int idx = -1;


    // build functions
    void init_pos_block(CODE *val_f, POSTYPE *pos_f, int n);
    CODE block_start_value();
    void rebuild_pos_block(CODE *val_f, POSTYPE *pos_f, int n);
    void rebuild_bitmap_pos(int n);
    void init_free_pos_block();


    // insert functions
    int insert_to_block(CODE *val_f, POSTYPE *pos_f, int n);
    int new_insert_to_block(CODE val_f, POSTYPE pos_f);    // one by one
    void reverse_bit(int position);
    void split_bitmap_pos();

    int insert_element_to_free_space(CODE *val_f, POSTYPE *pos_f, int n);
    int find_first_space();
    int fill_first_space(int insert_position, POSTYPE pos_f, CODE val_f);
    void insert_freepos_to_free_list(int new_free_idx);

    // remove functions
    POSTYPE modify_block(CODE val_f, POSTYPE new_pos_f, CODE new_val_f);
    void refine_start_value();  // when delete startvalue, we need to move the smallest value to startvalue

    // search functions
    int on_which_pos(CODE compare);
    vector<POSTYPE> find_refine_array(CODE compare);
    vector<POSTYPE> find_not_refine_array(CODE compare);


    // debug functions
    void display_block();
    void check_pos_block();

    // TODO: 这部分需要用delpersistent重写,area和Bindex同理
    // void free_pos_block();
};


class Area {
    public:
    pobj::persistent_ptr<pos_block> blocks[blockNumMax];
    pobj::p<int> blockNum;
    pobj::p<int> length;

    int idx = -1;

    // build functions
    void init_area(CODE *val, POSTYPE *pos, int n);
    void rebuild_area(CODE *val, POSTYPE *pos, int n);
    CODE area_start_value();
    
    pobj::persistent_ptr<pos_block> getFreeBlock();
    
    // insert functions
    void insert_to_area(CODE *val, POSTYPE *pos, int n);
    void insert_to_area_fixed(CODE *val, POSTYPE *pos, int n);
    void insert_to_area_batch(CODE *val, POSTYPE *pos, int n);
    void insert_to_area_batch_exec(CODE *val, POSTYPE *pos, int n,  int startBlockIdx, int endBlockIdx);
    void vec_insert_to_area(CODE *raw_data, vector <POSTYPE>&val, int n);
    void single_insert_to_area(CODE val, POSTYPE pos);
    void area_split_block(int block_idx);
    void area_split_block_sorted(int block_idx);
    void area_split_several_blocks(int blockIdx, CODE *val, POSTYPE *pos, int n);

    // remove functions
    POSTYPE *remove_from_area(CODE *data_to_remove, POSTYPE n);

    // search functions
    int in_which_block(CODE compare);


    // debug functions
    void display_area();
    void show_volume();
    void check_area();
};



class BinDex {
    friend Area;
    public:
    pobj::persistent_ptr<Area> areas[K];
    pobj::persistent_ptr<BITS[]> filterVectors[K - 1];
    pobj::persistent_ptr<CODE[]> raw_data;
    POSTYPE area_counts[K];
    POSTYPE length;


    // build functions
    void init_bindex(CODE *raw_data_in, POSTYPE n);
    void rebuild_bindex(CODE *raw_data_in, POSTYPE n);
    void load_bindex(CODE *raw_data_in, POSTYPE n);


    // insert functions
    void append_to_bindex(CODE *new_data, POSTYPE n);
    void append_to_bindex_unsorted(CODE *new_data, POSTYPE n);
    void single_append_to_bindex_unsorted(CODE *new_data, POSTYPE n);
    void append_to_bindex_and_rebuild(CODE *new_data, POSTYPE n);

    // remove functions
    void remove_from_bindex(CODE *data_to_remove, POSTYPE n);
    void reverse_filter_vector(int vector_idx, POSTYPE *positions, POSTYPE n);
    void reverse_bit(int vector_idx, POSTYPE position);

    // search functions
    int in_which_area(CODE compare);


    // debug functions
    void display_bindex();
    void show_volume();

    void check_bindex();
};



// semaphore
#include <mutex>
#include <condition_variable>

// free block list class
class FreeBlockList
{
private:
  static const int maxBlockNum_ = 2000000;
  static const int reloadBlockNum_ = 10;
  pobj::persistent_ptr<pos_block> freeblocks[maxBlockNum_];

  // semaphore
  std::mutex mutex_;
  std::condition_variable condition_;
  

public:
unsigned long count_; // Initialized as locked.
  void initFreeBlockList(int initCount = 10);
  ~FreeBlockList();
  pobj::persistent_ptr<pos_block> getFreeBlock();


  // semaphore
  void release();
  void acquire();
  bool try_acquire();
};

struct BinDexLayout {
  pobj::persistent_ptr<BinDex> bindex_;
  pobj::persistent_ptr<FreeBlockList> freeBlockList_;
};


extern pobj::pool<BinDexLayout> pop;
extern pobj::persistent_ptr<BinDex> bindex;
extern pobj::persistent_ptr<FreeBlockList> freeBlockList;



// search
void copy_bitmap(BITS *result, BITS *ref, int n, int t_id);
void copy_bitmap_not(BITS *result, BITS *ref, int start_n, int end_n, int t_id);
void copy_bitmap_bt(BITS *result, BITS *ref_l, BITS *ref_r, int start_n, int end_n, int t_id);
void copy_bitmap_bt_simd(BITS *to, BITS *from_l, BITS *from_r, int bitmap_len, int t_id);
void copy_bitmap_simd(BITS *to, BITS *from, int bitmap_len, int t_id);
void copy_bitmap_not_simd(BITS *to, BITS *from, int bitmap_len, int t_id);
void memset_numa0(BITS *p, int val, int n, int t_id);
void memset_mt(BITS *p, int val, int n);
void copy_filter_vector(BinDex *bindex, BITS *result, int k);
void copy_filter_vector_not(BinDex *bindex, BITS *result, int k);
void copy_filter_vector_bt(BinDex *bindex, BITS *result, int kl, int kr);
void copy_bitmap_xor_simd(BITS *to, BITS *bitmap1, BITS *bitmap2, int bitmap_len, int t_id);
void copy_filter_vector_xor(BinDex *bindex, BITS *result, int kl, int kr);

void bindex_scan_lt(BinDex *bindex, BITS *result, CODE compare);
void bindex_scan_le(BinDex *bindex, BITS *result, CODE compare);
void bindex_scan_gt(BinDex *bindex, BITS *result, CODE compare);
void bindex_scan_ge(BinDex *bindex, BITS *result, CODE compare);
void bindex_scan_bt(BinDex *bindex, BITS *result, CODE compare1, CODE compare2);
void bindex_scan_eq(BinDex *bindex, BITS *result, CODE compare);
void check_worker(CODE *codes, int n, BITS *bitmap, CODE target1, CODE target2, OPERATOR OP, int t_id);
void check(BinDex *bindex, BITS *bitmap, CODE target1, CODE target2, OPERATOR OP);
void check_st(BinDex *bindex, BITS *bitmap, CODE target1, CODE target2, OPERATOR OP);


// utils
char *bin_repr(BITS x);
void display_bitmap(BITS *bitmap, int bitmap_len);
template <typename T> POSTYPE *argsort(const T *v, POSTYPE n);

BITS gen_less_bits(const CODE *val, CODE compare, int n);
void set_fv_val_less(BITS *bitmap, const CODE *val, CODE compare, POSTYPE n);
int padding_fv_val_less(BITS *bitmap, POSTYPE length_old, const CODE *val_new, CODE compare, POSTYPE n);
void append_fv_val_less(BITS *bitmap, POSTYPE length_old, const CODE *val, CODE compare, POSTYPE n);


inline int bits_num_needed(int n) {
  // calculate the number of bits for storing n 0-1 bit results
  return ((n - 1) / BITSWIDTH) + 1;
}

inline POSTYPE num_insert_to_area(POSTYPE *areaStartIdx, int k, int n) {
  if (k < K - 1) {
    return areaStartIdx[k + 1] - areaStartIdx[k];
  } else {
    return n - areaStartIdx[k];
  }
}

uint32_t str2uint32(const char *s);
std::vector<CODE> get_target_numbers(const char *s);
bool isFileExists_stat(char *path);

CODE *generate_single_block_data(CODE startValue, CODE endValue, int datalen);



// time count part

class Timer{
  public:

  double time[30];
  mutex timeMutex[30];
  double timebase;

  Timer() {
    for (int i = 0; i < 30; i++) {
      time[i] = 0.0;
    }
    struct timeval t1;                           
    gettimeofday(&t1, NULL);
    timebase = t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0;
  }

  void clear() {
    for (int i = 0; i < 30; i++) {
      time[i] = 0.0;
    }
  }
  
  void commonGetStartTime(int timeId) {
    struct timeval t1;                           
    gettimeofday(&t1, NULL);
    lock_guard<mutex> lock(timeMutex[timeId]);
    time[timeId] -= (t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0) - timebase;
  }

  void commonGetEndTime(int timeId) {
    struct timeval t1;                           
    gettimeofday(&t1, NULL);
    lock_guard<mutex> lock(timeMutex[timeId]);
    time[timeId] += (t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0) - timebase;
    if (timeId == 10)
      cout << time[timeId] << endl;
  }

  void quickGetStartTime(int *timeArray, int timeId) {
    struct timeval t1;
    gettimeofday(&t1, NULL);
    timeArray[timeId] -= (t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0) - timebase;
  }

  void quickGetEndTime(int *timeArray, int timeId) {
    struct timeval t1;
    gettimeofday(&t1, NULL);
    timeArray[timeId] += (t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0) - timebase;
  }

  double quickGetStartTime(double time) {
    struct timeval t1;
    gettimeofday(&t1, NULL);
    time -= (t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0) - timebase;
    return time;
  }

  double quickGetEndTime(double time) {
    struct timeval t1;
    gettimeofday(&t1, NULL);
    time += (t1.tv_sec * 1000.0 + t1.tv_usec / 1000.0) - timebase;
    return time;
  }
  
  void showTime() {
    cout << endl;
    cout << "###########   Time  ##########" << endl;
    cout << "[Time] build bindex: ";
    cout << time[0] << endl;
    cout << "[Time] append to bindex: ";
    cout << time[1] << endl;
    cout << "[Time] insert to block: ";
    cout << time[2] << endl;
    cout << "[Time] split block: ";
    cout << time[3] << endl;
    cout << "[Time] insert to area: ";
    cout << time[4] << endl;
    cout << "[Time] init new space: ";
    cout << time[5] << endl;
    cout << "[Time] append to filter vector: ";
    cout << time[6] << endl;
    cout << "[Time] sort: ";
    cout << time[7] << endl;
    cout << "[Time] split into area: ";
    cout << time[8] << endl;
    cout << "##############################" << endl;
    cout << endl;
  }

};

extern Timer timer;
extern int DEBUG_TIME_COUNT;

#endif