#include <assert.h>
#include <inttypes.h>

#include <algorithm>
//#include <emmintrin.h>
#include <asm/unistd.h>
#include <getopt.h>
#include <immintrin.h>
#include <linux/perf_event.h>
#include <pthread.h>
#include <sched.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <bitset>
#include <iostream>
#include <iomanip>
#include <random>
#include <sstream>
#include <string>
#include <thread>

__attribute__((unused)) static long perf_event_open(
    struct perf_event_attr* hw_event, pid_t pid, int cpu, int group_fd,
    unsigned long flags) {
  return syscall(__NR_perf_event_open, hw_event, pid, cpu, group_fd, flags);
}

struct read_format {
  uint64_t nr;
  struct {
    uint64_t value;
    uint64_t id;
  } values[];
};

#define ON_HEAP 0

#define ROUNDUP_DIVIDE(x, n) ((x + n - 1) / n)
#define ROUNDUP(x, n) (ROUNDUP_DIVIDE(x, n) * n)
#define SKE_CODE_2_POS(code) (code + 128)
#define POS_2_SKE_CODE(code) (code - 128)
#define SKE_N (1 << 8)  // typedef int8_t ske_t/     1<<8
#define FREQ_Z SKE_N
#define INVALID_CODE 0
#define UNIQUE_CODE 1
#define NON_UNIQUE_CODE 2
#define BITMAP_TMP_N 8192
#define OPERATOR_LE 0
#define OPERATOR_EQ 1
#define OPERATOR_GT 2
#define OPERATOR_BET 3//between

typedef uint32_t base_t;
typedef int8_t ske_t;

//int pthread_setaffinity_np(pthread_t thread,size_t cpusetsize,const cpu_set_t *cpuset);
//int pthread_getaffinity_np(pthread_t thread,size_t cpusetsize, cpu_set_t *cpuset);

//定义函数指针新类型“inner_scan_work_func”
typedef void (*inner_scan_work_func)(int t_id, int bitmap_res_tmp[][2]);
int DATA_N = 1e9;//即1000000000
char DATA_PATH[256];
int THREAD_N = 36;
base_t TARGET_NUMBER;
int CODE_WIDTH = 32;
int STRIDE = 4096;
int DEBUG = 0;
int OPERATOR_TYPE;
base_t TARGET_NUMBER_2;

std::vector<base_t> target_numbers_l;  // 左端点
std::vector<base_t> target_numbers_r;  // 右端点
std::vector<base_t> insert_numbers;
char OP[5];
const int BITSWIDTH = sizeof(base_t) * 8;//即32
const int BITSSHIFT = 5;  // x / BITSWIDTH == x >> BITSSHIFT

int SAMPLES_N;
int JOBS_PER_THREAD;
base_t MAX_CODE_VAL;
int MALLOC_DATA_N;
int BITMAP_N;
int size;


base_t* data;
base_t* samples;
int* res;
ske_t* ske_col;
int* bitmap_res;
base_t map_array[SKE_N]
                [2];  // map_array[ske_code][0] = number; map_array[ske_code][1]
                      // = INVALID_CODE | UNIQUE_CODE | NON_UNIUQE_CODE
__m256i repeat_sx_256_1;
__m256i repeat_sx_256_2;
int exitVal;
pthread_mutex_t lock;
int gened_samples_n;
bool is_unique_1;
bool is_unique_2;
inner_scan_work_func inner_scan_work;

typedef void (*scan_func)(base_t target);
typedef struct {
  scan_func func;
  bool in_res;
} scan_test_t;
typedef struct {
  base_t target;
  ske_t ske_code;
  int start;
  int jobs;
} naive_scan_args_t;
typedef struct {
  int start;
  int jobs;
} create_ske_col_args_t;

ske_t map_it(base_t num) {
  // linear search...
  int high = 0;

  while (high < SKE_N) {
    base_t data = map_array[high][0];
    int option = map_array[high][1];
    bool break_loop = false;

    switch (option) {
      case NON_UNIQUE_CODE:
        break_loop = (num <= data);
        break;
      case UNIQUE_CODE:
        break_loop = (num == data);
        break;
    }

    if (break_loop) break;

    high++;
  }

  // assertions
  assert(high >= 0);
  assert(high < SKE_N);
  assert(map_array[high][1] != INVALID_CODE);
  int low = high - 1;
  switch (map_array[high][1]) {
    case NON_UNIQUE_CODE:
      while (low >= 0 && map_array[low][1] != NON_UNIQUE_CODE) low--;
      assert(low >= 0 ? num > map_array[low][0] : 1);//右侧为何是1？
      assert(num <= map_array[high][0]);
      break;
    case UNIQUE_CODE:
      assert(num == map_array[high][0]);
      break;
    case INVALID_CODE:
      assert(0);
      break;
  }

  return POS_2_SKE_CODE(high);//即减128
}

void usage() {
  printf("usage: \n");
  printf("-n: DATA_N \tDefault: %d\n", DATA_N);
  printf("[ -f: DATA_PATH ]\n");
  printf("-l: TARGET_NUMBER \tDefault: %d\n", TARGET_NUMBER);
  printf("-r: TARGET_NUMBER_2 \tDefault: %d\n", TARGET_NUMBER_2);
  printf("-T: THREAD_N \tDefault: %d\n", THREAD_N);
  printf("-w: CODE_WIDTH \tDefault: %d\n", CODE_WIDTH);
  printf("-s: STRIDE \tDefault: %d\n", STRIDE);
  printf("-o: OPERATOR_TYPE \tDefault: %d\n", OPERATOR_TYPE);
  printf("-d: DEBUG\n");
  printf("-h: HELP\n");
}

uint32_t strtouint32(const char* s) {
  uint32_t sum = 0;

  while (*s) sum = sum * 10 + (*s++ - '0');

  return sum;
}

void le_scan_work(int t_id, int bitmap_res_tmp[][2]) {
  int cur = t_id * JOBS_PER_THREAD;
  int curend = (t_id + 1) * JOBS_PER_THREAD;

  while (cur < DATA_N && cur < curend) {
    int bitmap_res_tmp_idx = 0;
    
    for (int j = 0; j < STRIDE; j++) {
      int i = cur + j * 32;//每次处理一个数据块的代码
      //对每个代码执行谓词求值所需的逻辑比较
      __m256i code = _mm256_load_si256((__m256i*)&ske_col[i]);//从i处开始，从数组连续载入32个值

      __m256i definite = _mm256_cmpgt_epi8(repeat_sx_256_1, code);
      int bitvector_def = _mm256_movemask_epi8(definite);
      //将32个字节的最高有效位（1/0）提取出来组合为一个32位int数，即变成位向量
      bitmap_res[i / 32] = bitvector_def;//前面数组中的下标实际为1、2、3……
      //bitmap_res即位向量
      if (!is_unique_1) {//若是非唯一码，还要判断是否相等。因为唯一码就一对一代表了target
        __m256i possible = _mm256_cmpeq_epi8(code, repeat_sx_256_1);

        int bitvector_possible = _mm256_movemask_epi8(possible);

//检查边界值是否有任何匹配的元组并存储符合条件的位置
        if (bitvector_possible) {//如果有相等的位置，就存入bitmap_res_tmp
          bitmap_res_tmp[bitmap_res_tmp_idx][0] = i >> 5;//i除以32，记录是第几次计算的
          bitmap_res_tmp[bitmap_res_tmp_idx][1] = bitvector_possible;
          bitmap_res_tmp_idx++;//代表待进一步判断的非唯一编码个数
        }
      }
    }

    cur += STRIDE * 32;
//检查所有不确定的元组（tuples）
    if (!is_unique_1) {
      for (int i = 0; i < bitmap_res_tmp_idx; i++) {
        int bitmap_pos = bitmap_res_tmp[i][0];//非唯一编码位置
        int bitmap = bitmap_res_tmp[i][1];//判断相等的位向量
        while (bitmap) {
          int mask = bitmap & (~bitmap + 1);//~即按位取反，再+1即补码。效果为只保留到最低位1
          int j = __builtin_ctz(bitmap);
          //↑__builtin_ctz（）函数用于计算输入数二进制形态下从最低位（右）起连续的0个数。此函数版本针对uint
          int data_pos = (bitmap_pos << 5) + j;//换算出第一个相等的位向量编码位置
          if (data_pos < DATA_N && (mask & bitmap_res_tmp[i][1]) &&
              data[data_pos] < TARGET_NUMBER)
            bitmap_res[bitmap_pos] |= mask;
            //↑注意此处相|的对象是bitmap_res数组，正常情况下其相等位置应全为0。故此操作为逐一单点置位
          bitmap = bitmap & (bitmap - 1);//与减了1的自己相与，效果为去掉一个最低位1
        }
      }
    }
  }
}

void gt_scan_work(int t_id, int bitmap_res_tmp[][2]) {
  int cur = t_id * JOBS_PER_THREAD;
  int curend = (t_id + 1) * JOBS_PER_THREAD;

  while (cur < DATA_N && cur < curend) {
    int bitmap_res_tmp_idx = 0;
    for (int j = 0; j < STRIDE; j++) {
      int i = cur + j * 32;
      __m256i code = _mm256_load_si256((__m256i*)&ske_col[i]);

      __m256i definite = _mm256_cmpgt_epi8(code, repeat_sx_256_1);
      int bitvector_def = _mm256_movemask_epi8(definite);

      bitmap_res[i / 32] = bitvector_def;

      if (!is_unique_1) {
        __m256i possible = _mm256_cmpeq_epi8(code, repeat_sx_256_1);

        int bitvector_possible = _mm256_movemask_epi8(possible);

        if (bitvector_possible) {
          bitmap_res_tmp[bitmap_res_tmp_idx][0] = i >> 5;
          bitmap_res_tmp[bitmap_res_tmp_idx][1] = bitvector_possible;
          bitmap_res_tmp_idx++;
        }
      }
    }

    cur += STRIDE * 32;

    if (!is_unique_1) {
      for (int i = 0; i < bitmap_res_tmp_idx; i++) {
        int bitmap_pos = bitmap_res_tmp[i][0];
        int bitmap = bitmap_res_tmp[i][1];
        while (bitmap) {
          int mask = bitmap & (~bitmap + 1);
          int j = __builtin_ctz(bitmap);

          int data_pos = (bitmap_pos << 5) + j;
          if (data_pos < DATA_N && (mask & bitmap_res_tmp[i][1]) &&
              data[data_pos] > TARGET_NUMBER)
            bitmap_res[bitmap_pos] |= mask;

          bitmap = bitmap & (bitmap - 1);
        }
      }
    }
  }
}

void eq_scan_work(int t_id, int bitmap_res_tmp[][2]) {
  int cur = t_id * JOBS_PER_THREAD;
  int curend = (t_id + 1) * JOBS_PER_THREAD;

  while (cur < DATA_N && cur < curend) {
    int bitmap_res_tmp_idx = 0;
    for (int j = 0; j < STRIDE; j++) {
      int i = cur + j * 32;
      __m256i code = _mm256_load_si256((__m256i*)&ske_col[i]);

      __m256i possible = _mm256_cmpeq_epi8(code, repeat_sx_256_1);
      int bitvector_possible = _mm256_movemask_epi8(possible);

      if (is_unique_1) {
        bitmap_res[i / 32] = bitvector_possible;
      } else if (bitvector_possible) {
        bitmap_res_tmp[bitmap_res_tmp_idx][0] = i >> 5;
        bitmap_res_tmp[bitmap_res_tmp_idx][1] = bitvector_possible;
        bitmap_res_tmp_idx++;
      }
    }

    cur += STRIDE * 32;

    if (!is_unique_1) {
      for (int i = 0; i < bitmap_res_tmp_idx; i++) {
        int bitmap_pos = bitmap_res_tmp[i][0];
        int bitmap = bitmap_res_tmp[i][1];
        while (bitmap) {
          int mask = bitmap & (~bitmap + 1);
          int j = __builtin_ctz(bitmap);

          int data_pos = (bitmap_pos << 5) + j;
          if (data_pos < DATA_N && (mask & bitmap_res_tmp[i][1]) &&
              data[data_pos] == TARGET_NUMBER)
            bitmap_res[bitmap_pos] |= mask;

          bitmap = bitmap & (bitmap - 1);
        }
      }
    }
  }
}

void bet_scan_work(int t_id, int bitmap_res_tmp[][2]) {
  int cur = t_id * JOBS_PER_THREAD;
  int curend = (t_id + 1) * JOBS_PER_THREAD;

  while (cur < DATA_N && cur < curend) {
    int bitmap_res_tmp_idx = 0;
    for (int j = 0; j < STRIDE; j++) {
      int i = cur + j * 32;
      __m256i code = _mm256_load_si256((__m256i*)&ske_col[i]);

      __m256i definite_gt = _mm256_cmpgt_epi8(code, repeat_sx_256_1);
      __m256i definite_le = _mm256_cmpgt_epi8(repeat_sx_256_1, code);

      int bitvector_def = _mm256_movemask_epi8(definite_gt);
      bitvector_def &= _mm256_movemask_epi8(definite_le);

      bitmap_res[i / 32] = bitvector_def;

      if (!is_unique_1) {
        __m256i possible = _mm256_cmpeq_epi8(code, repeat_sx_256_1);

        int bitvector_possible = _mm256_movemask_epi8(possible);

        if (bitvector_possible) {
          bitmap_res_tmp[bitmap_res_tmp_idx][0] = i >> 5;
          bitmap_res_tmp[bitmap_res_tmp_idx][1] = bitvector_possible;
          bitmap_res_tmp_idx++;
        }
      }

      if (!is_unique_2) {
        __m256i possible = _mm256_cmpeq_epi8(code, repeat_sx_256_2);

        int bitvector_possible = _mm256_movemask_epi8(possible);

        if (bitvector_possible) {
          bitmap_res_tmp[bitmap_res_tmp_idx][0] = i >> 5;
          bitmap_res_tmp[bitmap_res_tmp_idx][1] = bitvector_possible;
          bitmap_res_tmp_idx++;
        }
      }
    }

    cur += STRIDE * 32;

    if (!is_unique_1 || !is_unique_2) {
      for (int i = 0; i < bitmap_res_tmp_idx; i++) {
        int bitmap_pos = bitmap_res_tmp[i][0];
        int bitmap = bitmap_res_tmp[i][1];
        while (bitmap) {
          int mask = bitmap & (~bitmap + 1);
          int j = __builtin_ctz(bitmap);

          int data_pos = (bitmap_pos << 5) + j;
          if (data_pos < DATA_N && (mask & bitmap_res_tmp[i][1]) &&
              data[data_pos] > TARGET_NUMBER && 
              data[data_pos] < TARGET_NUMBER_2)//后续添加，源程序似乎遗漏此条件
            bitmap_res[bitmap_pos] |= mask;

          bitmap = bitmap & (bitmap - 1);
        }
      }
    }
  }
}

std::vector<base_t> get_target_numbers(const char* s) {
  std::string input(s);//构造函数：以s指针开头的字符串初始化字符串input
  std::stringstream ss(input);//将字符串input复制给ss
  std::string value;
  std::vector<base_t> result;
  while (std::getline(ss, value, ',')) {//从流ss中获取数据/数据转换为字符串value/指定结束符为“,”
    result.push_back((base_t)strtod(value.c_str(), nullptr));
    //string的c_str()方法返回代表该字符串的指针；strtod（）函数的第二个参数用来存储转换后的后一部分字符串的开始指针
  }
  return result;
}

void init(int argc, char* argv[]) {
  char optstr[] = "n:f:l:r:i:T:w:s:o:d";
  extern char* optarg;
  char opt;

  // global vars
  while ((opt = getopt(argc, argv, optstr)) != -1) {
    switch (opt) {
      case 'n':
        DATA_N = (int)atof(optarg);
        break;
      case 'f':
        strcpy(DATA_PATH, optarg);
        break;
      case 'l':
        target_numbers_l = get_target_numbers(optarg);
        break;
      case 'r':
        target_numbers_r = get_target_numbers(optarg);
        break;
      case 'i':
        insert_numbers = get_target_numbers(optarg);
        break;
      case 'T':
        THREAD_N = atoi(optarg);
        break;
      case 'w':
        CODE_WIDTH = atoi(optarg);
        break;
      case 's':
        STRIDE = atoi(optarg);
        break;
      case 'o':
        strcpy(OP, optarg);
        break;
      case 'd':
        DEBUG = 1;
        break;
      case 'h':
        usage();
        exit(-1);
        break;
      case '?':
        usage();
        exit(-1);
    }
  }
//下述四个参数但凡有一个未设置（即=0），则打印usage信息，并退出程序
  if (!DATA_N || !THREAD_N || !CODE_WIDTH || !STRIDE) {
    usage();
    exit(-1);
  }
//判断比较谓词
  if (strcmp("lt", OP) == 0 || strcmp("le", OP) == 0) {
    OPERATOR_TYPE = OPERATOR_LE;
  } else if (strcmp("gt", OP) == 0) {
    OPERATOR_TYPE = OPERATOR_GT;
  } else if (strcmp("eq", OP) == 0) {
    OPERATOR_TYPE = OPERATOR_EQ;
  } else if (strcmp("bt", OP) == 0) {
    OPERATOR_TYPE = OPERATOR_BET;
  } else {
    OPERATOR_TYPE = -1;
  }
  //无效谓词
  if (OPERATOR_TYPE < 0 || OPERATOR_TYPE > 3) {
    usage();
    exit(-1);
  }
//确保右端点数组为空，或左右端点数组大小相等
  assert(target_numbers_r.size() == 0 ||
         target_numbers_l.size() == target_numbers_r.size());

  SAMPLES_N = DATA_N / 2000;//取样总数50万
  JOBS_PER_THREAD = ROUNDUP(ROUNDUP_DIVIDE(DATA_N, THREAD_N),
                            32);  // 数据总数除以线程数，再以为单位32向上取整
  MAX_CODE_VAL = ((int64_t)1 << CODE_WIDTH) - 1;
  MALLOC_DATA_N = ROUNDUP(DATA_N, (STRIDE * 32));
  BITMAP_N = ROUNDUP_DIVIDE(MALLOC_DATA_N, 32);  // 结果为data数/stride，向上取整；int bitmap_res[]

  // malloc动态生成各数据结构
  data = (base_t*)malloc(MALLOC_DATA_N * sizeof(base_t));
  samples = (base_t*)malloc(SAMPLES_N * sizeof(base_t));
  res = (int*)malloc(DATA_N * sizeof(int));
  ske_col = (ske_t*)aligned_alloc(32, MALLOC_DATA_N * sizeof(ske_t));
  bitmap_res = (int*)malloc(BITMAP_N * sizeof(int));
  memset(ske_col, 0, MALLOC_DATA_N * sizeof(ske_t));
//下述五个结构参数但凡有一个未设置（即=0），则打印失败信息，并退出程序
  if (!data || !samples || !res || !ske_col || !bitmap_res) {
    printf("init: failed to alloc.\n");
    exit(-1);
  }

  // inner_scan_work_func 为函数指针，选择内置扫描工作函数
  switch (OPERATOR_TYPE) {
    case OPERATOR_LE:
      inner_scan_work = le_scan_work;
      break;
    case OPERATOR_GT:
      inner_scan_work = gt_scan_work;
      break;
    case OPERATOR_EQ:
      inner_scan_work = eq_scan_work;
      break;
    case OPERATOR_BET:
      inner_scan_work = bet_scan_work;
      break;
    default:
      assert(0);
  }

  // summary打印各种信息
  printf("DATA_N: %d\n", DATA_N);
  printf("DATA_PATH: %s\n", DATA_PATH);
  // printf("TARGET_NUMBER: %u\n", TARGET_NUMBER);
  // printf("TARGET_NUMBER_2: %u\n", TARGET_NUMBER_2);
  std::cout << "TARGET_L: " << std::endl;
  for (auto& n : target_numbers_l) std::cout << n << " ";
  std::cout << std::endl;
  std::cout << "TARGET_R: " << std::endl;
  for (auto& n : target_numbers_r) std::cout << n << " ";
  std::cout << std::endl;
  std::cout << "INSERT: " << std::endl;
  for (auto& n : insert_numbers) std::cout << n << " ";
  std::cout << std::endl;
  printf("THREAD_N: %d\n", THREAD_N);
  printf("CODE_WIDTH: %d\n", CODE_WIDTH);
  printf("STRIDE: %d\n", STRIDE);
  printf("OPERATOR_TYPE: %d\n", OPERATOR_TYPE);
  printf("DEBUG: %d\n", DEBUG);
  printf("\n");
  printf("SAMPLES_N: %d\n", SAMPLES_N);
  printf("JOBS_PER_THREAD: %d\n", JOBS_PER_THREAD);
  printf("MAX_CODE_VAL: %u\n", MAX_CODE_VAL);
  printf("MALLOC_DATA_N: %d\n", MALLOC_DATA_N);
  printf("BITMAP_N: %d\n", BITMAP_N);
  printf("----------\n");
}

template <typename T>
void ReservoirSample(T data[], int n, T samples[], int k) {
  std::random_device rd;
  std::mt19937 mt(rd());

  for (int i = 0; i < k; i++) samples[i] = data[i];

  for (int i = k; i < n; i++) {
    std::uniform_int_distribution<T> dist(0, k - 1);
    int j = dist(mt);
    if (j < k) samples[j] = data[i];
  }
}

void* MakeSamplesWork(void* argument) {
  int t_id = *(int*)argument;
  double ratio = (double)SAMPLES_N / DATA_N;
 
  int Samples_n = (int)(size * ratio);
  
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_real_distribution<double> dist(0, 1);

  int cur = JOBS_PER_THREAD * t_id;
  bool break_loop = false;
  while (1) {
    double tmp = dist(mt);//用以随机取样
    if (tmp < ratio) {
      pthread_mutex_lock(&lock);
      if (gened_samples_n < Samples_n)
        samples[gened_samples_n++] = data[cur];
      else
        break_loop = true;
      pthread_mutex_unlock(&lock);
    }
    cur = (cur + 1) % JOBS_PER_THREAD;
    if (break_loop) break;
  }

  return NULL;
}

void MakeSamples() {
  pthread_t t[THREAD_N];//左侧类型用于声明线程ID，
                        //也即代表当前系统位数的无符号（长）整形变量，64位系统即为unsigned long long
                        //数组t[]用来存储新创建线程所在地址单元，THREAD_N为线程数，默认为36
  gened_samples_n = 0;
  int args[THREAD_N];//用于MakeSamplesWork函数中，在下面的循环中由1到36，描述线程数
  //pthread_mutex_init()函数以动态方式创建互斥锁，使线程按顺序执行
  if (pthread_mutex_init(&lock, NULL) != 0) {
    printf("MakeSamples: failed to pthread_mutex_init\n");
    exit(-1);
  }
  //依次创建线程，每个线程运行一次MakeSamplesWork，开始位置为args[i]
  for (int i = 0; i < THREAD_N; i++) {
    args[i] = i;
    if (pthread_create(&t[i], NULL, MakeSamplesWork, &args[i]) < 0) {
      printf("MakeSamples: failed to pthread_create\n");
      exit(-1);
    }
  }
  //pthread_join使一个线程等待另一个结束
  for (int i = 0; i < THREAD_N; i++) {
    if (pthread_join(t[i], NULL) < 0) {
      printf("MakeSamples: failed to pthread_join\n");
      exit(-1);
    }
  }
  //销毁互斥锁
  pthread_mutex_destroy(&lock);
}

void init_data_by_random() {
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<base_t> dist(0, MAX_CODE_VAL);//指定随机数生成范围

  printf("initing data by random\n");
  size = DATA_N;
  // data and samples
  for (int i = 0; i < DATA_N; i++) data[i] = dist(mt);

  // ReservoirSample(data, DATA_N, samples, SAMPLES_N);
  MakeSamples();

  printf("init_data_by_random: done\n");
}

void init_data_from_file() {
  FILE* fp;

  if (!(fp = fopen(DATA_PATH, "rb"))) {
    printf("init_data_from_file: fopen(%s) faild\n", DATA_PATH);
    exit(-1);
  }

  printf("initing data from %s\n", DATA_PATH);

  // 8/16/32 only
  if (CODE_WIDTH == 8) {
    uint8_t* file_data = (uint8_t*)malloc(DATA_N * sizeof(uint8_t));
    size = fread(file_data, sizeof(uint8_t), DATA_N, fp);
    if ( size == 0) {
      printf("init_data_from_file: fread faild.\n");
      exit(-1);
    }
    for (int i = 0; i < size; i++) data[i] = file_data[i];
    free(file_data);
  } else if (CODE_WIDTH == 16) {
    uint16_t* file_data = (uint16_t*)malloc(DATA_N * sizeof(uint16_t));
    size = fread(file_data, sizeof(uint16_t), DATA_N, fp); 
    if (size == 0) {
      printf("init_data_from_file: fread faild.\n");
      exit(-1);
    }
    for (int i = 0; i < size; i++) data[i] = file_data[i];
    free(file_data);
  } else if (CODE_WIDTH == 32) {
    uint32_t* file_data = (uint32_t*)malloc(DATA_N * sizeof(uint32_t));
    size = fread(file_data, sizeof(uint32_t), DATA_N, fp);
    if (size == 0) {
      printf("init_data_from_file: fread faild.\n");
      exit(-1);
    }
    for (int i = 0; i < size; i++) data[i] = file_data[i];
    free(file_data);
  } else {
    printf("init_data_from_file: CODE_WIDTH != 8/16/32.\n");
    exit(-1);
  }

  JOBS_PER_THREAD = ROUNDUP(ROUNDUP_DIVIDE(size, THREAD_N), 32);
  // ReservoirSample(data, DATA_N, samples, SAMPLES_N);
  printf("数据装载完成，开始采样\ndata数据量%d\n每线程任务数%d\n",size,JOBS_PER_THREAD);
  MakeSamples();

  printf("init_data_from_file: done\n");
}

int binary_search(base_t num, base_t arr[], int n) {
  int low = 0;
  int high = n - 1;

  while (low <= high) {
    int mid = low + (high - low) / 2;
    if (arr[mid] < num) {
      low = mid + 1;
    } else if (arr[mid] > num) {
      high = mid - 1;
    } else {
      return mid;
    }
  }

  return -1;
}

void create_map() {//map_array即函数S（）的线性排列
  std::sort(samples, samples + SAMPLES_N);//样本排序

  base_t freqs_samples[FREQ_Z - 1];//二次取样样本值数组
  base_t freq_data[FREQ_Z - 1];//高频值数组
  int freq_code[FREQ_Z - 1];  //高频值的编码数组，这里使用int，因为编码范围只有0~255其实是（1~254）
  int freq_n = 0;//高频值数量

  printf("creating map...\n");

  // 所有编码一开始均为0（无效）
  memset(map_array, INVALID_CODE, sizeof(map_array));

  // 填 freqs_samples[]
  for (int i = 1; i < FREQ_Z; i++)
    freqs_samples[i - 1] = samples[i * SAMPLES_N / FREQ_Z];//已排序样本中均匀取256个

  // 给map_array填高频值
  for (int i = 0, j = 0; i < FREQ_Z - 1 && j < FREQ_Z - 1;) {
    while (j < FREQ_Z - 1 && freqs_samples[j] == freqs_samples[i]) j++;//i不动，j++直到样本值不一样
    //如果j与i相差大于1，说明中间有连续相等的样本值，要在之间赋唯一编码
    if (j - i > 1) {
      int unique_code = (i + j - 1) / 2;//确定唯一编码为所有相等样本下标的中点
      map_array[unique_code][0] = freqs_samples[unique_code];
      map_array[unique_code][1] = UNIQUE_CODE;
      freq_data[freq_n] = freqs_samples[unique_code];
      freq_code[freq_n] = unique_code;

      if (DEBUG)
        printf("freq_data[%d]: %u, coded as %d\n", freq_n,
               freqs_samples[unique_code], unique_code);

      freq_n++;
    }
    //将i增加到j处，开始下一循环
    i = j;
  }

  // 去掉高频值，重填样本数组
  int left_samples_n = 0;
  for (int i = 0; i < SAMPLES_N; i++) {
    if (binary_search(samples[i], freq_data, freq_n) >=
        0)  // 高频数组中有该样本值，则跳过不要
      continue;
    samples[left_samples_n++] = samples[i];
  }

  // map_array for the ordinarys
  // 为高频数组右端加一个最大值
  freq_data[freq_n] = MAX_CODE_VAL;
  freq_code[freq_n] = SKE_N - 1;
  freq_n++;
  //同理给映射数组填右端点，注意首末人为规定非唯一
  map_array[SKE_N - 1][0] = MAX_CODE_VAL;
  map_array[SKE_N - 1][1] = NON_UNIQUE_CODE;

  // 分配 [left_code, right_code) 根据 samples[start, end]
  int start = 0, end = 0, freq_idx = 0;
  int left_code = 0, right_code = 0;
//给map_array填非唯一编码
  while (start < left_samples_n && end < left_samples_n && freq_idx < freq_n) {
    while (end < left_samples_n && samples[end] < freq_data[freq_idx]) end++;
    end--;//end代表最后一个小于高频值的样本值下标，故++到超过再--

    right_code = freq_code[freq_idx];

    if (right_code && end > start) {  // 前面的right_code避免唯一码为 0
      //？避免连续唯一码问题？；避免样本没有编码多
      int samples_per_code = (end - start) / (right_code - left_code);//样本数按编码平均分
      if (samples_per_code) {
        for (int i = 0; i < right_code - left_code; i++) {
          map_array[left_code + i][0] =
              samples[start + samples_per_code * (i + 1)];  // 重点. (i+1)
          map_array[left_code + i][1] = NON_UNIQUE_CODE;
        }
      } else {
        // 这里else专门应对“样本少，编码多”的问题，
        for (int i = 0; i < end - start; i++) {
          map_array[left_code + i][0] = samples[start + i];
          map_array[left_code + i][1] = NON_UNIQUE_CODE;
        }
      }

      // 重点. 保持映射连续
      map_array[right_code - 1][0] = map_array[right_code][0] - 1;
      map_array[right_code - 1][1] = NON_UNIQUE_CODE;
    }

    start = end + 1;
    left_code = right_code + 1;
    freq_idx++;
  }

  if (DEBUG) {//逐行显示映射内容
    for (int i = 0; i < SKE_N; i++)
      printf("map_array[%03d]: %u, %u\n", i, map_array[i][0], map_array[i][1]);
  }

  printf("create_map: done\n");
}

// if we follow usual create_map,
// when we scan x < 0, since map_array[0] = 1, we need probe 0 and 1
// and for others, the map_array is consecutive, we need only probe once.
// that's why scan x < 0 is slower than others
void create_map_8bit() {
  for (int i = 0; i < 256; i++) {
    map_array[i][0] = i;
    map_array[i][1] = UNIQUE_CODE;
  }
}

void* create_ske_col_work(void* argument) {
  create_ske_col_args_t* arg = (create_ske_col_args_t*)argument;
  int start = arg->start;
  int jobs = arg->jobs;

  for (int i = start; i < start + jobs && i < DATA_N; i++)
    ske_col[i] = map_it(data[i]);

  free(arg);
  return NULL;
}

void create_ske_col() {
  pthread_t t[THREAD_N];

  printf("creating ske_col...\n");

  for (int i = 0; i < THREAD_N; i++) {
    create_ske_col_args_t* arg =
        (create_ske_col_args_t*)malloc(sizeof(create_ske_col_args_t));
    if (!arg) {
      printf("create_ske_col: failed to alloc for arg\n");
      exit(-1);
    }
    arg->start = i * JOBS_PER_THREAD;
    arg->jobs = JOBS_PER_THREAD;

    if (pthread_create(&t[i], NULL, create_ske_col_work, arg) < 0) {
      printf("create_ske_col: failed to pthread_create\n");
      exit(-1);
    }
  }

  for (int i = 0; i < THREAD_N; i++) {
    if (pthread_join(t[i], NULL) < 0) {
      printf("create_ske_col: failed to pthread_join\n");
      exit(-1);
    }
  }

  printf("create_ske_col: done\n");
}

void check_worker(base_t* codes, int n, int* bitmap, base_t target1, base_t target2, int t_id) {
  cpu_set_t mask;//定义CPU集
  CPU_ZERO(&mask);//初始化操作
  CPU_SET(t_id * 2, &mask);//将某个CPU加进CPU集里 
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    //pthread_setaffinity_np用来设置某一线程和CPU的亲和性，控制某线程跑在哪个CPU以及某CPU上有几个线程
    fprintf(stderr, "set thread affinity failed\n");
  }
  int avg_workload = n / THREAD_N;//每个线程的（检查）任务量
  int start = t_id * avg_workload;//开始位置为对应线程任务量的初始位置
  int end = (t_id == (THREAD_N - 1)) ? n : start + avg_workload;
  //↑如果是最后一个线程，结束位置为n；其余情况的结束位置都是开始位置往后延长一个平均任务量的位置
  for (int i = start; i < end; i++) {
    base_t data = codes[i];
    int truth;
    switch (OPERATOR_TYPE) {
      case OPERATOR_EQ:
        truth = data == target1;
        break;
      case OPERATOR_LE:
        truth = data < target1;
        break;
      case OPERATOR_GT:
        truth = data > target1;
        break;
      case OPERATOR_BET:
        truth = data > target1 && data < target2;
        break;
      default:
        assert(0);
    }
    int res =//双！即把数值转换为逻辑值；将某个bitmap（int型，标准32位）从高位逐位向低位扫描，看是否符合结果。
        !!(bitmap[i >> BITSSHIFT/*即i除以32*/] & (1U << (BITSWIDTH - 1 - i % BITSWIDTH)));//1后U指无符号整型
    if (truth != res) {//cerr对应标准错误流，类似于cout，用于显示错误信息，不需要缓冲，直接输出至显示器。
      std::cerr << "raw data[" << i << "]: " << codes[i] << ", truth: " << truth
                << ", res: " << res << std::endl;
      assert(truth == res);
    }
  }
}

void check(int* bitmap, base_t target1, base_t target2) {
  std::cout << "checking, target1: " << target1 << " target2: " << target2
            << std::endl;
  std::thread threads[THREAD_N];
  for (int t_id = 0; t_id < THREAD_N; t_id++) {
    threads[t_id] =
        std::thread(check_worker, data, DATA_N, bitmap, target1, target2, t_id);
  }
  for (int t_id = 0; t_id < THREAD_N; t_id++) {
    threads[t_id].join();
  }
  std::cout << "CHECK PASSED!" << std::endl;
}

int restore_and_check(base_t target, bool in_res) {
  if (!in_res) {
    //把bitmap_res[]结果按位（由低到高）存入res[]
    for (int i = 0; i < (int)BITMAP_N; i++) {
      for (int j = 0; j < 32; j++) {
        int mask = 1 << j;
        if ((i * 32 + j < DATA_N) && (mask & bitmap_res[i]))
          res[i * 32 + j] = 1;
      }
    }
  }

  // check res[]
  for (int i = 0; i < DATA_N; i++)
    if ((//简而言之就是res[i]出现了错误
          (OPERATOR_TYPE == OPERATOR_LE) &&
          ((res[i] && data[i] >= target) || (!res[i] && data[i] < target))
        ) ||
        (
          (OPERATOR_TYPE == OPERATOR_EQ) &&
          ((res[i] && data[i] != target) || (!res[i] && data[i] == target))
        ) ||
        (
          (OPERATOR_TYPE == OPERATOR_GT) &&
          ((res[i] && data[i] <= target) || (!res[i] && data[i] > target))
        )) {
      printf("data[%d]: %u failed. res: %d, ground truth: %d\n", i, data[i],
             res[i], data[i] < target);
      printf("map_it(target): %d, map_it(failed): %d\n",
             SKE_CODE_2_POS(map_it(target)), SKE_CODE_2_POS(map_it(data[i])));
      return -1;
    }

  return 0;
}

void* naive_scan_work(void* argument) {//实际工作
  naive_scan_args_t* arg = (naive_scan_args_t*)argument;//一参带四参
  base_t target = arg->target;
  ske_t ske_code = arg->ske_code;//ske_code = map_it(target);该变量为目标数的S映射
  int start = arg->start;
  int jobs = arg->jobs;

  for (int i = start; i < start + jobs && i < DATA_N; i++) {
    ske_t candidiate = ske_col[i];
    if (candidiate < ske_code)
      res[i] = 1;
    else if (candidiate == ske_code)//编码相等
      res[i] = data[i] < target;//则再于上层比较大小
  }

  free(arg);
  return NULL;
}

void naive_scan(base_t target) {//单线程版扫描，实际上并不使用
  ske_t ske_code = map_it(target);//设置目标数的S映射

  naive_scan_args_t* arg =
      (naive_scan_args_t*)malloc(sizeof(naive_scan_args_t));//生成结构体指针用来传参
  if (!arg) {
    printf("thread_naive_scan: failed to alloc for arg\n");
    exit(-1);
  }
  arg->target = target;
  arg->ske_code = ske_code;
  arg->start = 0;
  arg->jobs = DATA_N;
  //↑装载四个参数于一个结构体内
  naive_scan_work(arg);
}

void thread_naive_scan(base_t target) {//多线程版扫描
  ske_t ske_code = map_it(target);//设置目标数的S映射
  pthread_t t[THREAD_N];

  for (int i = 0; i < THREAD_N; i++) {
    naive_scan_args_t* arg =
        (naive_scan_args_t*)malloc(sizeof(naive_scan_args_t));//生成结构体指针×线程数次，用来传参
    if (!arg) {
      printf("thread_naive_scan: failed to alloc for arg\n");
      exit(-1);
    }
    arg->target = target;
    arg->ske_code = ske_code;
    arg->start = i * JOBS_PER_THREAD;
    arg->jobs = JOBS_PER_THREAD;
    //↑装载四个参数于一个结构体内×线程数次
    if (pthread_create(&t[i], NULL, naive_scan_work, arg) < 0) {
      printf("thread_naive_scan: failed to pthread_create\n");
      exit(-1);
    }
  }

  for (int i = 0; i < THREAD_N; i++) {
    if (pthread_join(t[i], NULL) < 0) {//以拥塞主线程的方式等待所创建的线程逐一完成
      printf("thread_naive_scan: failed to pthread_join\n");
      exit(-1);
    }
  }
}

void* simd_scan_work(void* argument) {
  int t_id = *(int*)argument;//

  int bitmap_res_tmp[BITMAP_TMP_N][2];  // bitmap_res_tmp[i][0] = bitmap;
                                        // bitmap_res_tmp[i][1] = pos

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    printf("failed to pthread_setaffinity_np\n");
    exit(-1);
  }

  inner_scan_work(t_id, bitmap_res_tmp);//核心执行语句

  return NULL;
}

void simd_scan(base_t target) {

    int cur = 0;
    int bitmap_res_tmp[BITMAP_TMP_N][2];
    while (cur < DATA_N) {
    int bitmap_res_tmp_idx = 0;
    
    for (int j = 0; j < STRIDE; j++) {
      int i = cur + j * 32;//每次处理一个数据块的代码
      //对每个代码执行谓词求值所需的逻辑比较
      __m256i code = _mm256_load_si256((__m256i*)&ske_col[i]);//从i处开始，从数组连续载入32个值
      __m256i definite = _mm256_cmpgt_epi8(repeat_sx_256_1, code);
      int bitvector_def = _mm256_movemask_epi8(definite);
      //将32个字节的最高有效位（1/0）提取出来组合为一个32位int数，即变成位向量
      bitmap_res[i / 32] = bitvector_def;//前面数组中的下标实际为1、2、3……
      //bitmap_res即位向量
      if (!is_unique_1) {//若是非唯一码，还要判断是否相等。因为唯一码就一对一代表了target
        __m256i possible = _mm256_cmpeq_epi8(code, repeat_sx_256_1);

        int bitvector_possible = _mm256_movemask_epi8(possible);

//检查边界值是否有任何匹配的元组并存储符合条件的位置
        if (bitvector_possible) {//如果有相等的位置，就存入bitmap_res_tmp
          bitmap_res_tmp[bitmap_res_tmp_idx][0] = i >> 5;//i除以32，记录是第几次计算的
          bitmap_res_tmp[bitmap_res_tmp_idx][1] = bitvector_possible;
          bitmap_res_tmp_idx++;//代表待进一步判断的非唯一编码个数
        }
      }
    }

    cur += STRIDE * 32;
//检查所有不确定的元组（tuples）
    if (!is_unique_1) {
      for (int i = 0; i < bitmap_res_tmp_idx; i++) {
        int bitmap_pos = bitmap_res_tmp[i][0];//非唯一编码位置
        int bitmap = bitmap_res_tmp[i][1];//判断相等的位向量
        while (bitmap) {
          int mask = bitmap & (~bitmap + 1);//~即按位取反，再+1即补码。效果为只保留到最低位1
          int j = __builtin_ctz(bitmap);
          //↑__builtin_ctz（）函数用于计算输入数二进制形态下从最低位（右）起连续的0个数。此函数版本针对uint
          int data_pos = (bitmap_pos << 5) + j;//换算出第一个相等的位向量编码位置
          if (data_pos < DATA_N && (mask & bitmap_res_tmp[i][1]) &&
              data[data_pos] < TARGET_NUMBER)
            bitmap_res[bitmap_pos] |= mask;
            //↑注意此处相|的对象是bitmap_res数组，正常情况下其相等位置应全为0。故此操作为逐一单点置位
          bitmap = bitmap & (bitmap - 1);//与减了1的自己相与，效果为去掉一个最低位1
        }
      }
    }
  }

    assert(target == TARGET_NUMBER);
}

void thread_simd_scan(base_t target) {
#ifdef PERF
  struct perf_event_attr pea;
  int fd1, fd2;
  uint64_t id1, id2;
  uint64_t val1, val2;
  char buf[4096];
  struct read_format* rf = (struct read_format*)buf;

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type = PERF_TYPE_HARDWARE;//硬件事件类型:底层动作计数器
  pea.size = sizeof(struct perf_event_attr);//属性结构的大小
  pea.config = PERF_COUNT_HW_CACHE_MISSES;//类型特定的配置:记录cache miss次数,通常指最后一级cache未命中
  pea.disabled = 1;
  pea.exclude_kernel = 1;//不计算内核
  pea.exclude_hv = 1;//不计算虚拟机管理程序
  pea.inherit = 1;//如果该perf_event绑定的task创建子进程，event自动对子进程也进行追踪
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  fd1 = syscall(__NR_perf_event_open, &pea, 0, -1, -1, 0);
  //↑cpu=0,event绑定指定CPU；pid=-1,CPU绑定当前CPU的所有进程。group_id=-1，创建新的group_leader。
  if (fd1 == -1) {
    printf("wtf\n");
    exit(-1);
  }
  ioctl(fd1, PERF_EVENT_IOC_ID, &id1);

  memset(&pea, 0, sizeof(struct perf_event_attr));
  pea.type = PERF_TYPE_HARDWARE;
  pea.size = sizeof(struct perf_event_attr);
  pea.config = PERF_COUNT_HW_CACHE_REFERENCES;//类型特定的配置:cache access次数。通常这表示最后一级cache访问，↓
  //但这可能因CPU而异。这可能包括预取和一致性消息；同样，这取决于CPU的设计。
  pea.disabled = 1;
  pea.exclude_kernel = 1;
  pea.exclude_hv = 1;
  pea.inherit = 1;
  pea.read_format = PERF_FORMAT_GROUP | PERF_FORMAT_ID;
  fd2 = syscall(__NR_perf_event_open, &pea, 0, -1, fd1 /*!!!*/, 0);
  if (fd2 == -1) {
    printf("wtf\n");
    exit(-1);
  }
  ioctl(fd2, PERF_EVENT_IOC_ID, &id2);

  ioctl(fd1, PERF_EVENT_IOC_RESET, PERF_IOC_FLAG_GROUP);
  ioctl(fd1, PERF_EVENT_IOC_ENABLE, PERF_IOC_FLAG_GROUP);
  //作用是把处于PERF_EVENT_STATE_OFF状态的event设置成PERF_EVENT_STATE_INACTIVE，以便该event能参与到perf sched当中去
#endif
  pthread_t t[THREAD_N];
  int t_id;

  for (int i = 0; i < THREAD_N; i++) {
    t_id = i;

    if (pthread_create(&t[i], NULL, simd_scan_work, &t_id) < 0) {
      printf("thread_simd_scan: failed to pthread_create\n");
      exit(-1);
    }
  }

  for (int i = 0; i < THREAD_N; i++) {
    if (pthread_join(t[i], NULL) < 0) {
      printf("thread_simd_scan: failed to pthread_join\n");
      exit(-1);
    }
  }

  assert(target == TARGET_NUMBER);

#ifdef PERF
  ioctl(fd1, PERF_EVENT_IOC_DISABLE, PERF_IOC_FLAG_GROUP);

  read(fd1, buf, sizeof(buf));
  for (uint64_t i = 0; i < rf->nr; i++) {
    if (rf->values[i].id == id1) {
      val1 = rf->values[i].value;
    } else if (rf->values[i].id == id2) {
      val2 = rf->values[i].value;
    }
  }

  printf("cache-miss: %" PRIu64 "\n", val1);
  printf("cache-ref: %" PRIu64 "\n", val2);
  close(fd1);
  close(fd2);
#endif
}

// scan for B < target
void test_scan(base_t target) {
  struct timeval t1, t2;
  double elapsed;

  ske_t ske_code_1 = map_it(TARGET_NUMBER);
  ske_t ske_code_2 = map_it(TARGET_NUMBER_2);
  is_unique_1 = map_array[SKE_CODE_2_POS(ske_code_1)][1] == UNIQUE_CODE;
  is_unique_2 = map_array[SKE_CODE_2_POS(ske_code_2)][1] == UNIQUE_CODE;

  if (is_unique_1)
    printf("is_unique_1: UNIQUE\n");
  else
    printf("is_unique_1: NON-UNIQUE\n");

  if (is_unique_2)
    printf("is_unique_2: UNIQUE\n");
  else
    printf("is_unique_2: NON-UNIQUE\n");

  repeat_sx_256_1 = _mm256_set1_epi8(ske_code_1);//将32个有符号8位整数设置为ske_code
  repeat_sx_256_2 = _mm256_set1_epi8(ske_code_2);//↑

  scan_test_t tests[] = {
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      {thread_simd_scan, false}, {thread_simd_scan, false},
      //{simd_scan, false},{naive_scan, true},{thread_naive_scan, true},
  };

  if (OPERATOR_TYPE == OPERATOR_LE) printf("finding x < %u\n", target);
  if (OPERATOR_TYPE == OPERATOR_EQ) printf("finding x = %u\n", target);
  if (OPERATOR_TYPE == OPERATOR_GT) printf("finding x > %u\n", target);
  if (OPERATOR_TYPE == OPERATOR_BET) printf("finding %u < x < %u\n", TARGET_NUMBER, TARGET_NUMBER_2);
   
 printf("-----------------------------------\n");

  for (int i = 0; i < (int)(sizeof(tests) / sizeof(tests[0])); i++) {
    memset(bitmap_res, 0, BITMAP_N * sizeof(int));
    memset(res, 0, DATA_N * sizeof(int));

    scan_func cur_func = tests[i].func;
    // bool in_res = tests[i].in_res;

    gettimeofday(&t1, NULL);
    cur_func(target);//即thread_simd_scan(target);此行为核心程序
    gettimeofday(&t2, NULL);

    elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0;//秒
    elapsed += (t2.tv_usec - t1.tv_usec) / 1000.0;//微秒
    printf("scan_func[%d]: %f ms\n", i, elapsed);//显示程序运行时间

    printf("-----------------------------------\n");
    // check(bitmap_res, TARGET_NUMBER, TARGET_NUMBER_2);

    /*
    if (restore_and_check(target, in_res) < 0) {
      printf("scan_func[%d]: failed\n", i);
      exitVal = -1;
    } else {
      printf("scan_func[%d]: passed\n", i);
    }
    */
  }
}

void insert(){
  int cur = -1;
  size = DATA_N;
  if (strlen(DATA_PATH)){
    FILE*fp = fopen(DATA_PATH, "rb");

      uint32_t* file_data = (uint32_t*)malloc(DATA_N * sizeof(uint32_t));
      size=fread(file_data, sizeof(uint32_t), DATA_N, fp);
      free(file_data);


    if(size == DATA_N){
    printf("Overflow: failed to insert.\n");
    exit(-1);
  }
    for (size_t pi = 0; pi < insert_numbers.size(); pi++){
      data[size+pi] = insert_numbers[pi];
      if(map_array[SKE_CODE_2_POS(map_it(data[size+pi]))][1] == NON_UNIQUE_CODE)  cur++;
    }

   /* if (cur > 255){
          MakeSamples();//此处Makesamples()可重写
          create_map();
        }*/
    
    for (size_t pi = 0; pi < insert_numbers.size(); pi++) {
    //int temp_size = size-insert_numbers.size();
    base_t INSERT_NUMBER = insert_numbers[pi];
    ske_t ske_code = map_it(INSERT_NUMBER);
    std::bitset<8> d(ske_code);
    //printf(%lu%u\n\n", , );
    std::cout<<"插入第 "<<pi+1<<" 个数："
             <<std::setw(12)<<std::left<<INSERT_NUMBER<<"编码："<<d;
    printf("  十进制形式：%d\n-----------------------------------------------------------\n",
           ske_code);
    ske_col[size+pi]=ske_code;
          
    }
    size+=insert_numbers.size();
  }

  else{
      printf("Overflow: failed to insert.\n");
      exit(-1);
    }
}

void clean() {
  // free mallocs
  free(data);
  free(samples);
  free(res);
  free(ske_col);
  free(bitmap_res);
}

int main(int argc, char* argv[]) {
  
  init(argc, argv);//先初始化
//根据情况生成数据
  if (strlen(DATA_PATH))
    init_data_from_file();
  else
    init_data_by_random();
//创建映射S
  if (CODE_WIDTH == 8)
    create_map_8bit();
  else
    create_map();
//创建Sketched Column
  create_ske_col();
  if(insert_numbers.size()) insert();
  for(int i=0;i<10;i++){
    std::bitset<8> s(ske_col[size-1-i]);
    std::cout<<"倒数第 "<<std::setw(2)<<std::left<<i+1<<"个数："
             <<std::setw(12)<<std::left<<data[size-1-i]<<"编码："<<s;
    printf("  十进制形式：%d\n-----------------------------------------------------------\n",
           ske_col[size-1-i]);
  }
  for (size_t pi = 0; pi < target_numbers_l.size(); pi++) {
    printf("第%lu组比较\n", pi+1);//%lu即无符号长整型long unsigned
    TARGET_NUMBER = target_numbers_l[pi];
    if (target_numbers_r.size() != 0) {
      assert(strcmp("bt", OP) == 0);
      TARGET_NUMBER_2 = target_numbers_r[pi];
    }//↑分别给两个目标数赋值
    if (strcmp("bt", OP) == 0 && TARGET_NUMBER > TARGET_NUMBER_2) {
      std::swap(TARGET_NUMBER, TARGET_NUMBER_2);
    }//↑若两目标数不满足左小右大，则交换以确保左小右大

    test_scan(TARGET_NUMBER);//此函数为实际操作
  }
//释放内存
  clean();

  exit(exitVal);
}
