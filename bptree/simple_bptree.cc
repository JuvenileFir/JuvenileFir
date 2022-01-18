#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <thread>

#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <omp.h>
#include <pthread.h>

#define THREAD_NUM 32 //原线程36

#define OPERATOR_LE 0
#define OPERATOR_EQ 1
#define OPERATOR_GT 2
#define OPERATOR_BET 3

using namespace std;

#define PRINT_EXCECUTION_TIME(msg, code)                                       \
  do {                                                                         \
    struct timeval t1, t2;                                                     \
    double elapsed;                                                            \
    gettimeofday(&t1, NULL);                                                   \
    do {                                                                       \
      code;                                                                    \
    } while (0);                                                               \
    gettimeofday(&t2, NULL);                                                   \
    elapsed = (t2.tv_sec - t1.tv_sec) * 1000.0;                                \
    elapsed += (t2.tv_usec - t1.tv_usec) / 1000.0;                             \
    printf(msg " time: %f ms\n", elapsed);                                     \
  } while (0)

#define COMPARE(start1, end1 , start2, end2)                                        \
  do {                                                                              \
    memset_mt(bitmap, 0, BITS_NUM);                                                 \
    vector<LeafNode*> target_nodes;                                                 \
    Node *target = (Node *)malloc(sizeof(Node *));                                  \
    LeafNode *p;                                                                    \
    int i = search(key, &target);                                                   \
    for (int j = start1; j < end1; j++)                                             \
      refine(bitmap, ((LeafNode *)target)->values[j]);                              \
    for (p = start2; p != end2; p = p->next)                                        \
      target_nodes.push_back(p);                                                    \
      std::thread threads[THREAD_NUM];                                              \
    for (int t_id = 0; t_id < THREAD_NUM; t_id++)                                   \
      threads[t_id] = std::thread(nodes_refine_numa0, target_nodes, bitmap, t_id);  \
    for (int t_id = 0; t_id < THREAD_NUM; t_id++)                                   \
      threads[t_id].join();                                                         \
  } while (0)

  #define PRE                                                                       \
  do {                                                                              \
    memset_mt(bitmap, 0, BITS_NUM);                                                 \
  } while (0)

    #define SINGLE(_start, _end, TARGET)                                            \
  do {                                                                              \
    for (int j = _start; j < _end; j++)                                             \
      refine(bitmap, ((LeafNode *)TARGET)->values[j]);                              \
  } while (0)

  #define NODE(_start, _end)                                                        \
  do {                                                                              \
    LeafNode *p;                                                                    \
    vector<LeafNode*> target_nodes;                                                 \
    for (p = _start; p != _end; p = p->next)                                        \
      target_nodes.push_back(p);                                                    \
      std::thread threads[THREAD_NUM];                                              \
    for (int t_id = 0; t_id < THREAD_NUM; t_id++)                                   \
      threads[t_id] = std::thread(nodes_refine_numa0, target_nodes, bitmap, t_id);  \
    for (int t_id = 0; t_id < THREAD_NUM; t_id++)                                   \
      threads[t_id].join();                                                         \
  } while (0)


typedef unsigned int CODE; //代表实际的数据
typedef unsigned int POSTYPE; //代表数据的位置（下标）
typedef unsigned int BITS; // 32 0-1 bit results are stored in a BITS
char OP[5];
const int N = (int)1e9;
const int BITSWIDTH = sizeof(BITS) * 8;//32
const int BITSSHIFT = 5; // x / BITSWIDTH == x >> BITSSHIFT   2^5=32
const unsigned int CODE_MAX = (1 << 32) - 1;
const int SIMD_ALIGEN = 32;
int OPERATOR_TYPE;

inline int bits_num_needed(int n) {
  // calculate the number of bits for storing n 0-1 bit results
  return ((n - 1) / BITSWIDTH) + 1;
}
const int BITS_NUM = bits_num_needed(N);
int runs = 30;
//作用是把32位变量x变为XXXX-XXXX的形式（例如把10101010101111100001010101011010写作1010-1010-1011-1110-0001-0101-0101-1010）
char *bin_repr(BITS x) {
  // Generate binary representation of a BITS variable
  int len = BITSWIDTH + BITSWIDTH / 4 + 1;//41
  char *result = (char *)malloc(sizeof(char) * len);
  BITS ref;
  int j = 0;
  for (int i = 0; i < BITSWIDTH; i++) {
    ref = 1 << (BITSWIDTH - i - 1);
    result[i + j] = (ref & x) ? '1' : '0';
    if ((i + 1) % 4 == 0) {
      j++;
      result[i + j] = '-';
    }
  }
  result[len - 1] = '\0';
  return result;
}

//将传入序列的key按该序列的值升序进行排序（因为是B+树）
template <typename T> POSTYPE *argsort(const T *v, POSTYPE n) {
  POSTYPE *idx = (POSTYPE *)malloc(n * sizeof(POSTYPE));
  for (POSTYPE i = 0; i < n; i++) {
    idx[i] = i;
  }
  std::sort(idx, idx + n, [&v](size_t i1, size_t i2) { return v[i1] < v[i2]; });//[……](……){……}为Lambda函数
  return idx;
}

//在位向量中“点”出结果
inline void refine(BITS *bitmap, POSTYPE pos) {
  bitmap[pos >> BITSSHIFT] ^= (1U << (BITSWIDTH - 1 - pos % BITSWIDTH));
  //      >>5 即除以32    ^= 按位异或    将最高位的1个1向右移动pos除以32的余数位
}

class Node {
public:
  bool isLeaf;
  vector<CODE> keys;
  int filled;
  virtual void append(CODE key, POSTYPE value) {}
  virtual void link(Node *next){};
  virtual Node *getNext(){};
  CODE max_key() { return keys[filled - 1]; }
  int binary_search(CODE key) {
    if (!filled)
      return 0; // only one child
    switch (OPERATOR_TYPE){
      case OPERATOR_BET:
      case OPERATOR_LE://得到第一个“大于等于”的位置
        return lower_bound(keys.begin(), keys.end(), key) - keys.begin();//返回的是目标位置下标的相对开头差值（也即filled）
        break;      
      case OPERATOR_EQ:
      case OPERATOR_GT://得到第一个“大于”的位置
        return upper_bound(keys.begin(), keys.end(), key) - keys.begin();//返回的是目标位置下标的相对开头差值（也即filled）
        break;
      default:
        assert(0);
        break;
    }
  }
};

class InternalNode : public Node {
public:
  vector<Node *> children;
  InternalNode *next;
  InternalNode(int order) {
    isLeaf = false;
    filled = 0;
    keys = vector<CODE>(order, -1);
    children = vector<Node *>(order, NULL);
  }
  void link(InternalNode *next) { this->next = next; }
  void append(Node *child) {
    children[filled] = child;
    keys[filled] = child->max_key();
    filled++;
  }
  Node *getNext() { return next; }
};

class LeafNode : public Node {
public:
  // LeafNode *prev;
  LeafNode *next;
  vector<POSTYPE> values;
  LeafNode(int order) {
    isLeaf = true;
    filled = 0;
    keys = vector<CODE>(order, -1);
    values = vector<POSTYPE>(order, 0);
    next = NULL;
  }
  void append(CODE key, POSTYPE value) {
    keys[filled] = key;
    values[filled] = value;
    filled++;
  }
  void link(LeafNode *next) { this->next = next; }
  Node *getNext() { return next; }
};

void display(LeafNode *node) {
  while (node) {
    for (int i = 0; i < node->filled; i++) {
      cout << "(" << node->keys[i] << "," << node->values[i] << "), ";
    }
    cout << "===" << endl;
    node = node->next;
  }
}

void display_internal(Node *node) {
  while (node) {
    cout << node->max_key() << ",";
    node = node->getNext();
  }
}

void memset_numa0(BITS *p, int val, int n, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  int avg_workload = (n / (THREAD_NUM * SIMD_ALIGEN)) * SIMD_ALIGEN;
  int start = t_id * avg_workload;
  // int end = start + avg_workload;
  int end = t_id == (THREAD_NUM - 1) ? n : start + avg_workload;
  memset(p + start, val, (end - start) * sizeof(BITS));
  // if (t_id == THREAD_NUM) {
  //   BITS val_bits = 0U;
  //   for (int i = 0; i < sizeof(BITS); i++) {
  //     val_bits |= ((unsigned int)val) << (8 * i);
  //   }
  //   for (; end < n; end++) {
  //     p[end] = val_bits;
  //   }
  // }
}

void memset_mt(BITS *p, int val, int n) {
  std::thread threads[THREAD_NUM];
  for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
    threads[t_id] = std::thread(memset_numa0, p, val, n, t_id);
  }
  for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
    threads[t_id].join();
  }
}

//分线程在位向量中“点”出结果
void nodes_refine_numa0(vector<LeafNode *> target_nodes, BITS *bitmap,
                        int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  int avg_workload = target_nodes.size() / THREAD_NUM;
  int start = t_id * avg_workload;
  int end =
      t_id == (THREAD_NUM - 1) ? target_nodes.size() : start + avg_workload;
  LeafNode *p;
  POSTYPE pos;
  POSTYPE to_be_prefetched;
  int prefetch_stride = 6;
  for (int i = start; i < end; i++) {
    p = target_nodes[i];
    int j;
    for (j = 0; j + prefetch_stride < p->filled; j++) {
      pos = p->values[j];
      to_be_prefetched = p->values[j+prefetch_stride];
      __builtin_prefetch(&bitmap[to_be_prefetched >> BITSSHIFT], 1, 1);
      __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT],
                           (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
    }
    while(j < p->filled) {
      pos = p->values[j];
      __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT],
                           (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
      j++;
    }
  }
}

class BPTree {
public:
  int order;
  Node *root;
  LeafNode *start; // optimization for lessthan
  BPTree(int order) : order(order), root(NULL) {}
  void Initialize(CODE *codes, POSTYPE n) {
    CODE *sorted_code = (CODE *)malloc(n * sizeof(CODE));
    POSTYPE *pos = argsort(codes, n);
    for (int i = 0; i < n; i++) {
      sorted_code[i] = codes[pos[i]];
    }

    // fill leafnodes
    vector<LeafNode *> leafnodes;
    leafnodes.push_back(new LeafNode(order));
    int node_id = 0;
    int m = 0; // number filled in a node
    for (POSTYPE i = 0; i < n; i++) {
      // TODO: this implementation is just a simple version, too many duplicate
      // keys are not supported
      if (m >= order / 2 && i < n - 1 && sorted_code[i] < sorted_code[i + 1]) {
        node_id++;
        leafnodes.push_back(new LeafNode(order));
        leafnodes[node_id - 1]->link(leafnodes[node_id]);
        m = 0;
      }
      leafnodes[node_id]->append(sorted_code[i], pos[i]);
      m++;
    }
    start = leafnodes[0];

    // fill internal nodes
    vector<vector<InternalNode *>> inter_nodes;
    inter_nodes.push_back(vector<InternalNode *>());

    node_id = 0;
    m = 0;
    int level = 0;
    inter_nodes[level].push_back(new InternalNode(order));
    for (LeafNode *p = leafnodes[0]; p; p = p->next) {
      if (m >= order / 2) {
        node_id++;
        inter_nodes[level].push_back(new InternalNode(order));
        inter_nodes[level][node_id - 1]->link(inter_nodes[level][node_id]);
        m = 0;
      }
      inter_nodes[level][node_id]->append(p);
      m++;
    }

    while (inter_nodes[level].size() > order) {
      inter_nodes.push_back(vector<InternalNode *>());
      level++;
      node_id = 0;
      m = 0;

      inter_nodes[level].push_back(new InternalNode(order));
      for (Node *p = inter_nodes[level - 1][0]; p; p = p->getNext()) {
        if (m >= order / 2) {
          node_id++;
          inter_nodes[level].push_back(new InternalNode(order));
          inter_nodes[level][node_id - 1]->link(inter_nodes[level][node_id]);
          m = 0;
        }
        inter_nodes[level][node_id]->append(p);
        m++;
      }
    }

    if (inter_nodes[level].size() == 1) {
      root = inter_nodes[level][0];
    } else {
      root = new InternalNode(order);
      for (Node *p = inter_nodes[level][0]; p; p = p->getNext()) {
        ((InternalNode *)root)->append(p);
      }
    }

  }
  POSTYPE search(CODE key, Node **p = (Node **)malloc(sizeof(Node **))) {
    *p = root;
    int i;
    while (!((*p)->isLeaf)) {
      i = (*p)->binary_search(key);
      *p = ((InternalNode *)(*p))->children[i];
    }
    i = (*p)->binary_search(key);
    return i;
  }
  POSTYPE search_pos(CODE key) {
    Node *p = (Node *)malloc(sizeof(Node *));
    int i = search(key, &p);
    if (p->keys[i] == key)
      return ((LeafNode *)p)->values[i];
    else
      return -1;
  }

//unused
/*   void display_lessthan(CODE key) {
    Node *target = (Node *)malloc(sizeof(Node *));
    int i = search(key, &target);
    for (LeafNode *p = start; p != target; p = (LeafNode *)p->getNext()) {
      for (int j = 0; j < p->filled; j++) {
        cout << p->values[j] << ",";
      }
    }
    for (int j = 0; j < i; j++) {
      cout << ((LeafNode *)target)->values[j] << ",";
    }
  } */
  
/*   BITS *lessthan(CODE key,
                 BITS *bitmap = (BITS *)malloc(BITS_NUM * sizeof(BITS))) {
    // memset(bitmap, 0, sizeof(BITS) * BITS_NUM);
    memset_mt(bitmap, 0, BITS_NUM);
    Node *target = (Node *)malloc(sizeof(Node *));
    int i = search(key, &target);
    // for (LeafNode *p = start; p != target; p = (LeafNode *)p->getNext()) {
    for (LeafNode *p = start; p != target; p = p->next ) {
      for (int j = 0; j < p->filled; j++) {
        refine(bitmap, p->values[j]);
      }
    }
    for (int j = 0; j < i; j++) {
      refine(bitmap, ((LeafNode *)target)->values[j]);
    }
    return bitmap;
  } */

  BITS *lessthan_mt(CODE key, BITS *bitmap) {
    PRE;
    Node *target= (Node *)malloc(sizeof(Node *));

    int i = search(key, &target);

    SINGLE(0,i,target);
    NODE(start,target);

  }
  BITS *greaterthan_mt(CODE key, BITS *bitmap) {
    PRE;
    Node *target= (Node *)malloc(sizeof(Node *));

    int i = search(key, &target);

    SINGLE(i,((LeafNode *)target)->filled,target);
    NODE(((LeafNode *)target)->next,NULL);
  }
  BITS *equal_mt(CODE key, BITS *bitmap) {
    PRE;
    Node *target= (Node *)malloc(sizeof(Node *));

    int u = search(key, &target);
    OPERATOR_TYPE = OPERATOR_LE;
    int i = search(key, &target);
    OPERATOR_TYPE = OPERATOR_EQ;

    SINGLE(i,u && j < ((LeafNode *)target)->filled,target);

  }
  BITS *between_mt(CODE key, CODE key2, BITS *bitmap) {
    PRE;
    Node *target_start = (Node *)malloc(sizeof(Node *));
    Node *target_end = (Node *)malloc(sizeof(Node *));

    int u = search(key2, &target_end);
    OPERATOR_TYPE = OPERATOR_GT;
    int i = search(key, &target_start);
    OPERATOR_TYPE = OPERATOR_BET;

    SINGLE(0,u,target_end);
    SINGLE(i,((LeafNode *)target_start)->filled,target_start);
    NODE(((LeafNode *)target_start)->next,target_end);

}
};

void check(CODE *codes, int n, BITS *bitmap, CODE target) {
  cout << "checking, target:" << target << endl;
  for (int i = 0; i < n; i++) {
    int truth = codes[i] < target;
    int res =
        !!(bitmap[i >> BITSSHIFT] & (1U << (BITSWIDTH - 1 - i % BITSWIDTH)));
    if (truth != res) {
      cout << "raw data[" << i << "]: " << codes[i] << ", truth: " << truth
           << ", res: " << res << endl;
      assert(truth == res);
    }
  }
  cout << "CHECK PASSED!" << endl;
}

void check_numa0(CODE *codes, int n, BITS *bitmap, CODE target, CODE target2, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  int avg_workload = n / THREAD_NUM;
  int start = t_id * avg_workload;
  int end = t_id == (THREAD_NUM - 1) ? n : start + avg_workload;
  int truth;
  for (int i = start; i < end; i++) {
    switch (OPERATOR_TYPE){
    case OPERATOR_LE:
      truth = codes[i] < target;
      break;
    case OPERATOR_GT:
      truth = codes[i] > target;
      break;
    case OPERATOR_EQ:
      truth = codes[i] == target;      
      break;
    case OPERATOR_BET:
      truth = codes[i] > target && codes[i] < target2;
      break;
    default:
      break;
    }

    int res =
        !!(bitmap[i >> BITSSHIFT] & (1U << (BITSWIDTH - 1 - i % BITSWIDTH)));
    if (truth != res) {
      cout << "raw data[" << i << "]: " << codes[i] << ", truth: " << truth
           << ", res: " << res << endl;
      assert(truth == res);
  }
    // if (truth == 1){
    //     cout<<codes[i]<<endl;    
    // }
 }
}

void check_mt(CODE *codes, int n, BITS *bitmap, CODE target, CODE target2) {
  // cout << "checking, target:" << target << endl;
  switch (OPERATOR_TYPE){
    case OPERATOR_LE:
      cout << "checking:result < " << target << endl;
      break;
    case OPERATOR_GT:
      cout << "checking:result > " << target << endl;
      break;
    case OPERATOR_EQ:
      cout << "checking:result = " << target << endl;
      break;
    case OPERATOR_BET:
      cout << "checking:"<< target << " < result < " << target2 << endl;
      break;
    default:
      break;
    }
  std::thread threads[THREAD_NUM];
  for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
    threads[t_id] = std::thread(check_numa0, codes, n, bitmap, target, target2, t_id);
  }
  for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
    threads[t_id].join();
  }
  cout << "CHECK PASSED!" << endl;
}

vector<double> get_selectivities(char *s) {
  string input(s);
  stringstream ss(input);
  string value;
  vector<double> result;
  while (getline(ss, value, ',')) {
    result.push_back(stod(value));
  }
  return result;
}

int main(int argc, char *argv[]) {
  //
  char opt;
  vector<double> selectivities;
  while ((opt = getopt(argc, argv, "hs:r:o:")) != -1) {
    switch (opt) {
    case 'h':
      cout << "Usage: [-s <SELECTIVITY_ARRAY> -r <RUNS_NUM>]" << endl;
      exit(0);
    case 's':
      selectivities = get_selectivities(optarg);
      break;
    case 'r':
      runs = atoi(optarg);
      break;
    case 'o':
      strcpy(OP, optarg);
      break;
    default:
      cout << "Error: unknown option" << opt << endl;
    }
  }

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

  CODE *codes = (CODE *)malloc(N * sizeof(CODE));
  default_random_engine generator(0);
  uniform_int_distribution<unsigned int> distribution(0, CODE_MAX);
  for (POSTYPE i = 0; i < N; i++) {
    // codes[i] = rand();
    codes[i] = distribution(generator);
  }
  cout << endl;
  BPTree a(400);
  PRINT_EXCECUTION_TIME("B+ Tree BUILDING", a.Initialize(codes, N));
  

  double selectivity = 1e-5;
  CODE target,target2;
  cout << "===================" << endl;
  for (int s = 0; s < selectivities.size(); s++) {
    selectivity = selectivities[s];
    cout << "RUNNING SELECTIVITY " << selectivity << " :" << endl;
    // BITS *bitmap = (BITS *)malloc(BITS_NUM * sizeof(BITS));
    BITS *bitmap = (BITS *)aligned_alloc(SIMD_ALIGEN, BITS_NUM * sizeof(BITS));
    for (int i = 0; i < runs; i++) {
      target = (CODE)(CODE_MAX * selectivity);
      target2 = (CODE)(CODE_MAX * ( 1 - selectivity ));

      // memset(bitmap, 0, sizeof(BITS) * BITS_NUM);
      switch (OPERATOR_TYPE){
    case OPERATOR_LE:
      // PRINT_EXCECUTION_TIME("B+ tree lessthan", bitmap = a.lessthan(target));
        PRINT_EXCECUTION_TIME("B+ tree lessthan", a.lessthan_mt(target, bitmap));
      break;
    case OPERATOR_GT:
      // PRINT_EXCECUTION_TIME("B+ tree greaterthan", bitmap = a.greaterthan(target));
        PRINT_EXCECUTION_TIME("B+ tree greaterthan", a.greaterthan_mt(target, bitmap));
      break;
    case OPERATOR_EQ:
        PRINT_EXCECUTION_TIME("B+ tree equal", a.equal_mt(target, bitmap));//2147487089
      break;
    case OPERATOR_BET:
        PRINT_EXCECUTION_TIME("B+ tree between", a.between_mt(target, target2, bitmap));
      break;
    default:
      assert(0);
      cout<<"未选择操作符"<<endl;
      break;
    }

      check_mt(codes, N, bitmap, target, target2);
      // free(bitmap);
    }
  }

}
