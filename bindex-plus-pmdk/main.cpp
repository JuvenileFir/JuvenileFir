#include "bindex.h"

extern CODE *data_sorted;
std::vector<CODE> target_numbers_l;  // left target numbers
std::vector<CODE> target_numbers_r;  // right target numbers
std::vector<CODE> test_numbers;  // test numbers
std::vector<CODE> remove_numbers;  // test numbers
pobj::pool<BinDexLayout> pop;
pobj::persistent_ptr<BinDex> bindex;
pobj::persistent_ptr<FreeBlockList> freeBlockList;
BITS *result1;
BITS *result2;
bool BITMAP_USED;
bool BATCH_SPLIT;
bool BATCH_BLOCK_INSERT;
bool SORTED;
Timer timer;
int DEBUG_TIME_COUNT;

void exp_opt(int argc, char *argv[]) {
  char opt;
  int selectivity;
  CODE target1, target2;
  char DATA_PATH[256] = "\0";
  char OPERATOR_TYPE[5];
  prefetch_stride = 6;
  BITMAP_USED = false;
  BATCH_SPLIT = false;
  BATCH_BLOCK_INSERT = true;
  SORTED = false;
  // int CODE_WIDTH = sizeof(CODE) *8;
  DEBUG_TIME_COUNT = 1;

  // get command line options
  bool STRICT_SELECTIVITY = false;
  bool TEST_INSERTING = false;
  bool REBUILD = false;
  bool CREATE = false;
  bool REMOVE = false;
  while ((opt = getopt(argc, argv, "hsi::qubcjl:r:o:f:n:p:d:")) != -1) {
    switch (opt) {
      case 'h':
        printf(
            "Usage: %s \n"
            "[-l <left target list>] [-r <right target list>]"
            "[-s use stric selectivity]"
            "[-i test inserting]"
            "[-p <prefetch-stride>]"
            "[-q rebuild bindex]"
            "[-b use bitmap]"
            "[-f <input-file>] [-o <operator>] \n",
            argv[0]);
        exit(0);
      case 'l':
        target_numbers_l = get_target_numbers(optarg);
        break;
      case 'r':
        target_numbers_r = get_target_numbers(optarg);
        break;
      case 'd':
        REMOVE = true;
        remove_numbers = get_target_numbers(optarg);
        break;
      case 'o':
        strcpy(OPERATOR_TYPE, optarg);
        break;
      case 'f':
        strcpy(DATA_PATH, optarg);
        break;
      case 'n':
        // DATA_N
        break;
      case 'b':
        BATCH_SPLIT = true;
        break;
      case 'p':
        prefetch_stride = str2uint32(optarg);
        break;
      case 's':
        STRICT_SELECTIVITY = true;
        break;
      case 'z':
        BITMAP_USED = true;
        break;
      case 'i':
        TEST_INSERTING = true;
        if (optarg != NULL)
          test_numbers = get_target_numbers(optarg);
        break;
      case 'u':
        SORTED = true;
      case 'q':
        REBUILD = true;
        break;
      case 'c':
        CREATE = true;
        break;
      case 'j':
        BATCH_BLOCK_INSERT = false;
        break;
      default:
        printf("Error: unknown option %c\n", (char)opt);
        exit(-1);
    }
  }
  assert(target_numbers_r.size() == 0 || target_numbers_l.size() == target_numbers_r.size());
  assert(blockNumMax);

  // initial data
  CODE *data = (CODE *)malloc(N * sizeof(CODE));
  if (!strlen(DATA_PATH)) {
    printf("initing data by random\n");
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<CODE> dist(MINCODE, MAXCODE);
    CODE mask = ((uint64_t)1 << (sizeof(CODE) * 8)) - 1;
    for (int i = 0; i < N; i++) {
      data[i] = dist(mt) & mask;
      assert(data[i] <= mask);
    }
  } else {
    FILE *fp;

    if (!(fp = fopen(DATA_PATH, "rb"))) {
      printf("init_data_from_file: fopen(%s) faild\n", DATA_PATH);
      exit(-1);
    }
    printf("initing data from %s\n", DATA_PATH);

    // 8/16/32 only
    if (CODEWIDTH == 8) {
      uint8_t *file_data = (uint8_t *)malloc(N * sizeof(uint8_t));
      if (fread(file_data, sizeof(uint8_t), N, fp) == 0) {
        printf("init_data_from_file: fread faild.\n");
        exit(-1);
      }
      for (int i = 0; i < N; i++) data[i] = file_data[i];
      free(file_data);
    } else if (CODEWIDTH == 16) {
      uint16_t *file_data = (uint16_t *)malloc(N * sizeof(uint16_t));
      if (fread(file_data, sizeof(uint16_t), N, fp) == 0) {
        printf("init_data_from_file: fread faild.\n");
        exit(-1);
      }
      for (int i = 0; i < N; i++) data[i] = file_data[i];
      free(file_data);
    } else if (CODEWIDTH == 32) {
      uint32_t *file_data = (uint32_t *)malloc(N * sizeof(uint32_t));
      if (fread(file_data, sizeof(uint32_t), N, fp) == 0) {
        printf("init_data_from_file: fread faild.\n");
        exit(-1);
      }
      for (int i = 0; i < N; i++) data[i] = file_data[i];
      free(file_data);
    } else {
      printf("init_data_from_file: CODE_WIDTH != 8/16/32.\n");
      exit(-1);
    }
  }

  // TODO: build bindex from persistent file 
  if (!strlen(DATA_PATH)) {
    //save data
    char DATA_PATH2[256] = "./savefile\0";
    FILE *fp2;
    if (!(fp2 = fopen(DATA_PATH2, "wb"))) {
      printf("save_to_file: fopen(%s) faild\n", DATA_PATH2);
      exit(-1);
    }
    printf("saving data to %s\n", DATA_PATH2);

    // 8/16/32 only
    if (CODEWIDTH == 8) {
      if (fwrite(data, sizeof(uint8_t), N, fp2) == 0) {
        printf("save_to_file: fwrite faild.\n");
        exit(-1);
      }
    } else if (CODEWIDTH == 16) {
      if (fwrite(data, sizeof(uint16_t), N, fp2) == 0) {
        printf("save_to_file: fwrite faild.\n");
        exit(-1);
      }
    } else if (CODEWIDTH == 32) {
      if (fwrite(data, sizeof(uint32_t), N, fp2) == 0) {
        printf("save_to_file: fwrite faild.\n");
        exit(-1);
      }
    } else {
      printf("CODE_WIDTH %d\n",CODEWIDTH);
      printf("save_to_file: CODE_WIDTH != 8/16/32.\n");
    }
    fclose(fp2);

    printf("[+] save finished!\n");
  }

  // Build the bindex structure
  printf("Build the bindex structure...\n");
  char *path = (char *)"/mnt/pmem0/bindex-plus";
  if(!isFileExists_stat(path)){
    cout << "[+] create " << path << endl;
    pop = pobj::pool<BinDexLayout>::create(path, LAYOUT,
                                     MIN_POOL, S_IRUSR | S_IWUSR);
  }
  else {
    cout << "[+] open " << path << endl;
    pop = pobj::pool<BinDexLayout>::open(path, LAYOUT);
  }


  // BinDex *bindex = (BinDex *)malloc(sizeof(BinDex));
  cout << "[+] init bindex" << endl;
  bindex = pop.root()->bindex_;
  freeBlockList = pop.root()->freeBlockList_;
  // PRINT_EXCECUTION_TIME("BinDex building", bindex->init_bindex(data, N));
  if(REBUILD){
    // PRINT_EXCECUTION_TIME("FreeBlock Init",freeBlockList->initFreeBlockList(400));
    if (DEBUG_TIME_COUNT) timer.commonGetStartTime(0);
    PRINT_EXCECUTION_TIME("BinDex rebuilding", bindex->rebuild_bindex(data, N));
    if (DEBUG_TIME_COUNT) timer.commonGetEndTime(0);
  }
  else if (CREATE) {
    pobj::make_persistent_atomic<FreeBlockList>(pop,pop.root()->freeBlockList_);
    freeBlockList = pop.root()->freeBlockList_;
    pobj::make_persistent_atomic<BinDex>(pop,pop.root()->bindex_);
    bindex = pop.root()->bindex_;
    PRINT_EXCECUTION_TIME("FreeBlock Init",freeBlockList->initFreeBlockList(100000));
    PRINT_EXCECUTION_TIME("BinDex building", bindex->init_bindex(data, N));
  } 
  else {
    PRINT_EXCECUTION_TIME("BinDex loading", bindex->load_bindex(data, N));
  }

  printf("\n");
  cout << "Done" << endl;

  if (TEST_INSERTING) {
    for (auto pd = test_numbers.begin(); pd != test_numbers.end(); pd++){
      cout << *pd << endl;
    }
    /* PRINT_EXCECUTION_TIME("APPEND 0.1", bindex->append_to_bindex( data, 0.001 * N));
    PRINT_EXCECUTION_TIME("APPEND 1", bindex->append_to_bindex( data, 0.01 * N));
    PRINT_EXCECUTION_TIME("APPEND 5", bindex->append_to_bindex( data, 0.05 * N));
    PRINT_EXCECUTION_TIME("APPEND 10", bindex->append_to_bindex( data, 0.1 * N)); */
    // PRINT_EXCECUTION_TIME("APPEND 10", bindex->append_to_bindex( data, 0.1 * N));
    // PRINT_EXCECUTION_TIME("APPEND 20", bindex->append_to_bindex( data, 0.2 * N));
    // PRINT_EXCECUTION_TIME("APPEND 30", bindex->append_to_bindex( data, 0.3 * N));
    // PRINT_EXCECUTION_TIME("APPEND ", bindex->append_to_bindex( data, (test_numbers[0] / 100.0) * N));
    // 14.252 38.649 56.519
    // 84.208  50
    // 111.217  80

    // baseline-pmdk
    // 82.234  50
    // 96.740  80
    // bindex->display_bindex();
    // exit(0);
    // PRINT_EXCECUTION_TIME("APPEND 10", bindex->append_to_bindex_unsorted(data, 0.1 * N));
    // PRINT_EXCECUTION_TIME("APPEND 20", bindex->append_to_bindex_unsorted(data, 0.2 * N));
    // PRINT_EXCECUTION_TIME("APPEND 30", bindex->append_to_bindex_unsorted(data, 0.3 * N));
    // PRINT_EXCECUTION_TIME("APPEND 80", bindex->append_to_bindex_unsorted(data, 0.8 * N));
    // 119.832  50
    // 188.722  80
    // PRINT_EXCECUTION_TIME("APPEND ", bindex->single_append_to_bindex_unsorted( data, (test_numbers[0] / 100.0) * N));
    CODE *append_data = (CODE *)malloc(N * sizeof(CODE));

    {
    FILE *fp;

    if (!(fp = fopen("data_1e6_2.dat", "rb"))) {
      printf("init_append_data_from_file: fopen(%s) faild\n", "data_1e6_2.dat");
      exit(-1);
    }
    printf("initing append_data from %s\n", "data_1e6_2.dat");

    // 8/16/32 only
    if (CODEWIDTH == 8) {
      uint8_t *file_append_data = (uint8_t *)malloc(N * sizeof(uint8_t));
      if (fread(file_append_data, sizeof(uint8_t), N, fp) == 0) {
        printf("init_append_data_from_file: fread faild.\n");
        exit(-1);
      }
      for (int i = 0; i < N; i++) append_data[i] = file_append_data[i];
      free(file_append_data);
    } else if (CODEWIDTH == 16) {
      uint16_t *file_append_data = (uint16_t *)malloc(N * sizeof(uint16_t));
      if (fread(file_append_data, sizeof(uint16_t), N, fp) == 0) {
        printf("init_append_data_from_file: fread faild.\n");
        exit(-1);
      }
      for (int i = 0; i < N; i++) append_data[i] = file_append_data[i];
      free(file_append_data);
    } else if (CODEWIDTH == 32) {
      uint32_t *file_append_data = (uint32_t *)malloc(N * sizeof(uint32_t));
      if (fread(file_append_data, sizeof(uint32_t), N, fp) == 0) {
        printf("init_append_data_from_file: fread faild.\n");
        exit(-1);
      }
      for (int i = 0; i < N; i++) append_data[i] = file_append_data[i];
      free(file_append_data);
    } else {
      printf("init_append_data_from_file: CODE_WIDTH != 8/16/32.\n");
      exit(-1);
    }
  }
    
    /* for (int i = 10; i <= 100; i += 10){
      bindex->rebuild_bindex(data, N);
      cout << i << endl;
      PRINT_EXCECUTION_TIME("APPEND ", bindex->append_to_bindex_and_rebuild( append_data, (i / 100.0) * N));
      cout << endl;
      bindex->check_bindex();
    } */


  
  // sorted VS unsorted Normal test

    for (int i = 10; i <= 100; i += 10){
      for (int j = 0; j < 10; j++) {
        bindex->rebuild_bindex(data, N);
        printf("APPEND %d ",i);
        timer.commonGetStartTime(1);
        PRINT_EXCECUTION_TIME("", bindex->append_to_bindex( data, (i / 100.0) * N));
        timer.commonGetEndTime(1);
        // PRINT_EXCECUTION_TIME("APPEND ", bindex->single_append_to_bindex_unsorted( data, (i / 100.0) * N));
        cout << endl;
        bindex->check_bindex();
      }
      timer.showTime();
      timer.clear();
    } 
    //1064590639
    free(append_data);
   
   
    // split batch VS nobatch test
    // append to one block only
    
/*     int datalen = 3756;    // 2049
    // int datalen = 3756 * 3.5;    // 2049
    // CODE *block_data0 = generate_single_block_data(1060272361,1064590639,datalen*10);
    CODE *block_data = generate_single_block_data(1060272361,1064590639,datalen*100);
    bindex->areas[63]->show_volume();

    for (int i = 110; i < 200; i += 10){
      bindex->rebuild_bindex(data, N);
      cout << i << endl;
      timer.commonGetStartTime(1);
      PRINT_EXCECUTION_TIME("APPEND Batch", bindex->append_to_bindex( block_data, datalen * (i / 10.0)));
      // PRINT_EXCECUTION_TIME("APPEND AND REBUILD", bindex->append_to_bindex_and_rebuild( block_data, datalen * (i / 10.0)));
      timer.commonGetEndTime(1);
      // bindex->areas[63]->show_volume();
      // bindex->areas[63]->display_area();
      
      cout << "Bindex length: " << bindex->length << endl;
      cout << endl;
      bindex->check_bindex();
      timer.showTime();
      timer.clear();
    }

    free(block_data);  */



    // PRINT_EXCECUTION_TIME("APPEND 10", bindex->single_append_to_bindex_unsorted(data, 0.1 * N));
    // PRINT_EXCECUTION_TIME("APPEND 20", bindex->single_append_to_bindex_unsorted(data, 0.2 * N));
    // PRINT_EXCECUTION_TIME("APPEND 30", bindex->single_append_to_bindex_unsorted(data, 0.3 * N));
    // PRINT_EXCECUTION_TIME("APPEND 80", bindex->single_append_to_bindex_unsorted(data, 0.8 * N));
    // 120.277  50
    // 191.633  80

    /* PRINT_EXCECUTION_TIME("APPEND 10", bindex->append_to_bindex( data, 0.1 * N));
    PRINT_EXCECUTION_TIME("APPEND 20", bindex->append_to_bindex( data, 0.2 * N));
    PRINT_EXCECUTION_TIME("APPEND 30", bindex->append_to_bindex( data, 0.3 * N)); */
    return;
  }

  if(REMOVE) {
    for(int j = 0; j < 20; j++){
      cout << data[j] << ",";
    }
    cout << endl;
    CODE *data_to_remove = (CODE *)malloc(remove_numbers.size() * sizeof(CODE));
    int i =0;
    for (auto pd = remove_numbers.begin(); pd != remove_numbers.end(); pd++){
      data_to_remove[i] = *pd;
      i++;
    }
    // PRINT_EXCECUTION_TIME("REMOVE", bindex->remove_from_bindex(data_to_remove,remove_numbers.size()));
    PRINT_EXCECUTION_TIME("REMOVE", bindex->remove_from_bindex(data,0.1 * N));
    return;
  }

  // display_bindex(bindex);
  printf("\n");

  // BinDex Scan
  printf("BinDex scan...\n\n");
  // int target1, target2;
  int bitmap_len = bits_num_needed(bindex->length);
  BITS *bitmap = (BITS *)aligned_alloc(SIMD_ALIGEN, bitmap_len * sizeof(BITS));
  memset_mt(bitmap, 0xFF, bitmap_len);
  // for hydex_scan_eq
  result1 = (BITS *)aligned_alloc(SIMD_ALIGEN, bitmap_len * sizeof(BITS));
  result2 = (BITS *)aligned_alloc(SIMD_ALIGEN, bitmap_len * sizeof(BITS));
  // cout << "1" << endl;
  // CAUTION! MAX_VAL * selc doesn't strictly match the selectivity, so we reset target_numbers_l with sorted data to
  // fix the selectivity
  if (STRICT_SELECTIVITY) {
    int n_selc = target_numbers_l.size();
    target_numbers_l.clear();
    for (int i = 0; i < n_selc - 1; i++) {
      // cout << "search [" << i << "]: " << data_sorted[(int)(DATA_N * i / (n_selc - 1))] << endl;
      target_numbers_l.push_back(data_sorted[(int)(DATA_N * i / (n_selc - 1))]);
    }
    target_numbers_l.push_back(data_sorted[(int)DATA_N - 1]);
  }
  // cout << "2" << endl;
  for (int pi = 0; pi < target_numbers_l.size(); pi++) {
    // printf("RUNNING %d\n", pi);
    // cout << "RUNNING" << endl;
    target1 = target_numbers_l[pi];
    if (target_numbers_r.size() != 0) {
      assert(strcmp("bt", OPERATOR_TYPE) == 0);
      target2 = target_numbers_r[pi];
    }
    if (strcmp("bt", OPERATOR_TYPE) == 0 && target1 > target2) {
      std::swap(target1, target2);
    }

    for (int i = 0; i < RUNS; i++) {
      if (strcmp("lt", OPERATOR_TYPE) == 0) {
        PRINT_EXCECUTION_TIME("lt", bindex_scan_lt(bindex.get(), bitmap, target1));
        check(bindex.get(), bitmap, target1, 0, LT);
      } else if (strcmp("le", OPERATOR_TYPE) == 0) {
        PRINT_EXCECUTION_TIME("le", bindex_scan_le(bindex.get(), bitmap, target1));
        check(bindex.get(), bitmap, target1, 0, LE);
      } else if (strcmp("gt", OPERATOR_TYPE) == 0) {
        PRINT_EXCECUTION_TIME("gt", bindex_scan_gt(bindex.get(), bitmap, target1));
        check(bindex.get(), bitmap, target1, 0, GT);
      } else if (strcmp("ge", OPERATOR_TYPE) == 0) {
        PRINT_EXCECUTION_TIME("ge", bindex_scan_ge(bindex.get(), bitmap, target1));
        check(bindex.get(), bitmap, target1, 0, GE);
      } else if (strcmp("eq", OPERATOR_TYPE) == 0) {
        PRINT_EXCECUTION_TIME("eq", bindex_scan_eq(bindex.get(), bitmap, target1));
        check(bindex.get(), bitmap, target1, 0, EQ);
      } else if (strcmp("bt", OPERATOR_TYPE) == 0) {
        PRINT_EXCECUTION_TIME("bt", bindex_scan_bt(bindex.get(), bitmap, target1, target2));
        check(bindex.get(), bitmap, target1, target2, BT);
      }

      printf("\n");
    }
  }

  // clean jobs
  free(data);
  free(bitmap);
  // TODO: free_bindex(bindex);
  free(result1);
  free(result2);

  if (data_sorted != NULL) free(data_sorted);
}

int main(int argc, char *argv[]) { exp_opt(argc, argv); }

