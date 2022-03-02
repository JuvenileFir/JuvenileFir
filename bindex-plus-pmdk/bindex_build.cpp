#include "bindex.h"


extern bool BITMAP_USED;
CODE *data_sorted;
extern pobj::persistent_ptr<FreeBlockList> freeBlockList;
void BinDex::init_bindex( CODE *raw_data_in, POSTYPE n) {
  length = n;
  POSTYPE avgAreaSize = n / K;

  CODE areaStartValues[K];
  POSTYPE areaStartIdx[K];

  // CODE *data_sorted = (CODE *)malloc(n * sizeof(CODE));  // Sorted codes
  data_sorted = (CODE *)malloc(n * sizeof(CODE));  // Sorted codes
  // Store raw data in bindex
  // Allocate 2 times of needed space, preparing for future appending
  // CODE *bindex_raw_data = (CODE *)malloc(2 * n * sizeof(CODE));
  // raw_data = bindex_raw_data;
  cout << "[+] allocate raw data space" << endl;
  pobj::make_persistent_atomic<CODE[]>(pop, raw_data, n * sizeof(CODE));
  /* pobj::transaction::run(pop,[&]{
    raw_data = pobj::make_persistent<CODE>(2 * n);
    // cout << "space size: " <<malloc_usable_size(raw_data.get()) << endl;
  }); */
  CODE *bindex_raw_data = raw_data.get();
  cout << "[+] copy raw data" << endl;
  memcpy(bindex_raw_data,raw_data_in,n * sizeof(CODE));

  cout << "[+] split raw data" << endl;
  POSTYPE *pos = argsort(raw_data_in, n);
  for (int i = 0; i < n; i++) {
    data_sorted[i] = raw_data_in[pos[i]];
  }

  areaStartValues[0] = data_sorted[0];
  areaStartIdx[0] = 0;
  for (int i = 1; i < K; i++) {
    areaStartValues[i] = data_sorted[i * avgAreaSize];
    int j = i * avgAreaSize;
    if (areaStartValues[i] == areaStartValues[i - 1]) {
      areaStartIdx[i] = j;
    } else {
      // To find the first element which is less than startValue
      // TODO: in current implementation, an area must contain at least two
      // different values, area containing unique code should be considered in
      // future implementation
      while (data_sorted[j] == areaStartValues[i]) {
        j--;
      }
      areaStartIdx[i] = j + 1;
    }
  }

  cout << "[+] Build areas" << endl;
  // Build the areas
  std::thread threads[THREAD_NUM];
  for (int k = 0; k * THREAD_NUM < K; k++) {
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      int area_idx = k * THREAD_NUM + j;
      // pobj::make_persistent_atomic<Area>(pop, areas[area_idx]);
      pobj::transaction::run(pop, [&] {
        areas[area_idx] = pobj::make_persistent<Area>();
      });
      POSTYPE area_size = num_insert_to_area(areaStartIdx, area_idx, n);
      area_counts[area_idx] = area_size;
      // areas[area_idx]->init_area(data_sorted + areaStartIdx[area_idx],
      //                           pos + areaStartIdx[area_idx], area_size);
      areas[area_idx]->idx = area_idx;
      threads[j] = std::thread(&Area::init_area, areas[area_idx], data_sorted + areaStartIdx[area_idx],
      pos + areaStartIdx[area_idx], area_size); 

    }
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      threads[j].join();
    } 
  }

  
  // display_bindex();

  // Accumulative adding
  for (int i = 1; i < K; i++) {
    area_counts[i] += area_counts[i - 1];
  }
  assert(area_counts[K] == length);

  cout << "[+] Build filterVectors" << endl;
  // Build the filterVectors
  for (int k = 0; k * THREAD_NUM < K - 1; k++) {
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      // Malloc 2 times of space, prepared for future appending
      /* filterVectors[k * THREAD_NUM + j] =
          (BITS *)aligned_alloc(SIMD_ALIGEN, 2 * bits_num_needed(n) * sizeof(BITS)); */
      // TODO: how to use aligned)alloc in pmdk?
      pobj::make_persistent_atomic<BITS[]>(pop,
        filterVectors[k * THREAD_NUM + j],
        2*bits_num_needed(n)
      );
      threads[j] = std::thread(set_fv_val_less, filterVectors[k * THREAD_NUM + j].get(), raw_data_in,
                               areas[k * THREAD_NUM + j + 1]->area_start_value(), n); 
      /* set_fv_val_less(filterVectors[k * THREAD_NUM + j].get(), raw_data,
                               areas[k * THREAD_NUM + j + 1]->area_start_value(), n); */
    }
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      threads[j].join();
    }
  }
  free(pos);
  // free(data_sorted);
}



void BinDex::rebuild_bindex(CODE *raw_data_in, POSTYPE n)
{
  // report datalen and compare with dge
  cout << endl;
  cout << "#############" << endl;
  cout << "Bindex status report:" << endl;
  cout << "Datalen: " << bindex->length << endl;
  cout << "Data percent: " << double(bindex->length) / double(blockMaxSize * blockNumMax * K) << endl;
  cout << "#############" << endl;
  cout << endl;

  // rebuild bindex with rawdata
  length = n;
  POSTYPE avgAreaSize = n / K;

  CODE areaStartValues[K];
  POSTYPE areaStartIdx[K];

  data_sorted = (CODE *)malloc(n * sizeof(CODE));  // WARNING: memory may be leaked here, see line 462
  
  CODE *bindex_raw_data = raw_data.get();
  cout << "[+] copy raw data" << endl;
  memcpy(bindex_raw_data,raw_data_in,n * sizeof(CODE));


  cout << "[+] split raw data" << endl;
  POSTYPE *pos = argsort(raw_data_in, n);
  for (int i = 0; i < n; i++) {
    data_sorted[i] = raw_data_in[pos[i]];
  }

  areaStartValues[0] = data_sorted[0];
  areaStartIdx[0] = 0;
  for (int i = 1; i < K; i++) {
    areaStartValues[i] = data_sorted[i * avgAreaSize];
    int j = i * avgAreaSize;
    if (areaStartValues[i] == areaStartValues[i - 1]) {
      areaStartIdx[i] = j;
    } else {
      while (data_sorted[j] == areaStartValues[i]) {
        j--;
      }
      areaStartIdx[i] = j + 1;
    }
  }

  cout << "[+] Rebuild areas" << endl;
  // Build the areas
  std::thread threads[THREAD_NUM];
  for (int k = 0; k * THREAD_NUM < K; k++) {
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      int area_idx = k * THREAD_NUM + j;
      
      POSTYPE area_size = num_insert_to_area(areaStartIdx, area_idx, n);
      area_counts[area_idx] = area_size;

      areas[area_idx]->idx = area_idx;
      threads[j] = std::thread(&Area::rebuild_area, areas[area_idx], data_sorted + areaStartIdx[area_idx],
      pos + areaStartIdx[area_idx], area_size); 
    }
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      threads[j].join();
    } 
  }
  // display_bindex();
  // Accumulative adding
  for (int i = 1; i < K; i++) {
    area_counts[i] += area_counts[i - 1];
  }
  assert(area_counts[K] == length);

  cout << "[+] Reuild filterVectors" << endl;
  // Build the filterVectors
  for (int k = 0; k * THREAD_NUM < K - 1; k++) {
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {

      threads[j] = std::thread(set_fv_val_less, filterVectors[k * THREAD_NUM + j].get(), raw_data_in,
                               areas[k * THREAD_NUM + j + 1]->area_start_value(), n); 
      }
    for (int j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      threads[j].join();
    }
  }
  free(pos);
  // free(data_sorted);
}

void Area::rebuild_area(CODE *val, POSTYPE *pos, int n) {
  // TODO: An area may explode for extremely skewed data.
  // Area containing only unique code should be considered in
  // future implementation

  // TODO: recycle block to free block list
  int old_blockNum = blockNum;

    // check if rebuild BlockNum is smaller than before, if larger, add some new blocks to area
  int new_blockNum = n / blockInitSize + 1 - old_blockNum;
  for (int i = 0; i < new_blockNum; i++) {
    pobj::make_persistent_atomic<pos_block>(pop, blocks[blockNum + i]);
    // blocks[blockNum + i] = freeBlockList->getFreeBlock();
    pobj::make_persistent_atomic<POSTYPE[]>(pop, blocks[blockNum + i]->pos, blockMaxSize);
    pobj::make_persistent_atomic<CODE[]>(pop,blocks[blockNum + i]->val,blockMaxSize);
    // init bitmap
    if (BITMAP_USED)
      pobj::make_persistent_atomic<BITS[]>(pop,blocks[blockNum + i]->bitmap_pos,blockMaxSize / CODEWIDTH );

  }

  int i = 0;
  blockNum = 0;
  length = n;



  while (i + blockInitSize < n) {
    blocks[blockNum]->rebuild_pos_block(val + i, pos + i, blockInitSize);
    blocks[blockNum]->idx = blockNum;
    (blockNum) = blockNum + 1;
    // assert(blockNum <= old_blockNum); // maybe < ?
    i += blockInitSize;
  }
  blocks[blockNum]->rebuild_pos_block(val + i, pos + i, n - i);
  blocks[blockNum]->idx = blockNum;
  blockNum = blockNum + 1;
  // assert(blockNum <= old_blockNum);
  assert(blockNum <= blockNumMax);
}

void pos_block::rebuild_pos_block(CODE *val_f, POSTYPE *pos_f, int n) {
  // assert(n <= blockInitSize);  TODO: split will be fine but rebuild-init may have some problems
  length = 0;
  free_idx = -1;
  int insert_position;
  if (BITMAP_USED)
    rebuild_bitmap_pos(0);

  for (int i = 0; i < n; i++) {
      // find an empty place
    // int insert_position = find_first_space();
    insert_position = length;

    // insert val to this position
    val[insert_position] = val_f[i];
    pos[insert_position] = pos_f[i];

    length = length + 1;
    if (BITMAP_USED)
      reverse_bit(insert_position);
  }

  start_value = val[0];
}


// what load differs from rebuild is that load uses data
// from persistent memory (and try to fix some inconsistent
// errors) while rebuild use new data from input (or random
// in experiment), these two name may be a little confusing
void BinDex::load_bindex(CODE *raw_data_in, POSTYPE n)
{
  // load bindex from persistent memory
  // nothing will be done by now
  // we will add some code to fix some inconsistent errors in the future
  cout << "[+] Bindex loaded" << endl;
  return;
}


void Area::init_area(CODE *val, POSTYPE *pos, int n) {
  // TODO: An area may explode for extremely skewed data.
  // Area containing only unique code should be considered in
  // future implementation
  // cout << "init area ... ..." << endl;
  int i = 0;
  blockNum = 0;
  length = n;
  while (i + blockInitSize < n) {
    // blocks[blockNum] = (pos_block *)malloc(sizeof(pos_block));
    pobj::make_persistent_atomic<pos_block>(pop, blocks[blockNum]);
    // blocks[blockNum] = freeBlockList->getFreeBlock();
    blocks[blockNum]->init_pos_block(val + i, pos + i, blockInitSize);
    // COUNT_BLOCK_EXCECUTION_TIME("Block time:",blocks[blockNum]->init_pos_block(val + i, pos + i, blockInitSize));
    blocks[blockNum]->idx = blockNum;
    (blockNum) = blockNum + 1;
    i += blockInitSize;
    /* pobj::transaction::run(pop,[&]{
      blocks[blockNum] = pobj::make_persistent<pos_block>();
      blocks[blockNum]->init_pos_block(val + i, pos + i, blockInitSize);
      blocks[blockNum]->idx = blockNum;
      (blockNum) = blockNum + 1;
      i += blockInitSize;
    }); */
  }
  pobj::make_persistent_atomic<pos_block>(pop, blocks[blockNum]);
  // blocks[blockNum] = freeBlockList->getFreeBlock();
  blocks[blockNum]->init_pos_block(val + i, pos + i, n - i);
  // COUNT_BLOCK_EXCECUTION_TIME("Block time:",blocks[blockNum]->init_pos_block(val + i, pos + i, n - i));
  blocks[blockNum]->idx = blockNum;
  blockNum = blockNum + 1;
  /* pobj::transaction::run(pop,[&]{
    blocks[blockNum] = pobj::make_persistent<pos_block>();
    blocks[blockNum]->init_pos_block(val + i, pos + i, n - i);
    blocks[blockNum]->idx = blockNum;
    blockNum = blockNum + 1;
  }); */
  assert(blockNum <= blockNumMax);
}

void pos_block::reverse_bit(int position)
{
  bitmap_pos[position / BITSWIDTH] = 
    bitmap_pos[position / BITSWIDTH] ^ (1 << (BITSWIDTH - position - 1));
}

int pos_block::find_first_space()
{
  // 在block的head中保存一个free_idx，指向第一个空闲的位置，该位置则保存下一个空闲的位置，以此类推，形成空闲链表。
  // 插入时首先在后面append，当插满时，在空闲链表中寻找空位，如果free_idx为-1(代表没有空闲位置)，则开始split
  // find_first_space 负责首先检查空闲链表中是否有空位，如果有空位则返回空位的idx
  // 如果没有空位（即free_idx的值为-1），则返回 length
  // 主函数会在插入前首先检查是否已经满了，如果满了，则不会进入find_first_space，而是直接开始split

  if (free_idx == -1)
    return length;
  
  int first_space_idx = free_idx;
  free_idx = pos[free_idx];
  return first_space_idx;

}

int pos_block::fill_first_space(int insert_position, POSTYPE pos_f, CODE val_f)
{
  pos[insert_position] = pos_f;
  val[insert_position] = val_f;
  return 0;
}

void pos_block::rebuild_bitmap_pos(int n)
{
  int bitmap_num = blockMaxSize / BITSWIDTH;
  for(int i = 0; i < bitmap_num; i++){
    bitmap_pos[i] = 0;
  }
}

void pos_block::split_bitmap_pos()
{
  // TODO: USE multi-reverse later, this function may be slow now
  for(int i = length; i < blockMaxSize; i++){
    reverse_bit(i);
  }
}


void pos_block::init_pos_block(CODE *val_f, POSTYPE *pos_f, int n) 
{
   // assert(n <= blockInitSize);   TODO:FIX THIS!
  length = 0;
  free_idx = -1;

  int insert_position;

  pobj::make_persistent_atomic<POSTYPE[]>(pop, pos, blockMaxSize);
  pobj::make_persistent_atomic<CODE[]>(pop,val,blockMaxSize);

  // init bitmap
  if (BITMAP_USED)
    pobj::make_persistent_atomic<BITS[]>(pop,bitmap_pos,blockMaxSize / CODEWIDTH );


  for (int i = 0; i < n; i++) {
      // find an empty place
    // int insert_position = find_first_space();
    insert_position = length;
    // insert val to this position
    val[insert_position] = val_f[i];
    pos[insert_position] = pos_f[i];

    length = length + 1;
    if (BITMAP_USED)
      reverse_bit(insert_position);
  }

  start_value = val[0];

}


void pos_block::init_free_pos_block() 
{
  pobj::make_persistent_atomic<POSTYPE[]>(pop, pos, blockMaxSize);
  pobj::make_persistent_atomic<CODE[]>(pop,val,blockMaxSize);

  // init bitmap
  if (BITMAP_USED)
    pobj::make_persistent_atomic<BITS[]>(pop,bitmap_pos,blockMaxSize / CODEWIDTH );

}