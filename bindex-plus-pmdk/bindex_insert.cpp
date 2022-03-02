#include "bindex.h"

extern pobj::persistent_ptr<FreeBlockList> freeBlockList;
extern bool BITMAP_USED;
extern Timer timer;
extern int DEBUG_TIME_COUNT;
extern bool BATCH_SPLIT;
extern bool BATCH_BLOCK_INSERT;
extern bool SORTED;

int pos_block::insert_to_block( CODE *val_f, POSTYPE *pos_f, int n) {
  
  if(DEBUG_TIME_COUNT) timer.commonGetStartTime(2);
  // Insert max(n, #vacancy) elements to a block, return 0 if the block is still
  // not filled up.
  int flagNum, length_new;
  if ((n + length) >= blockMaxSize) {
    // Block filled up! This block will be splitted in insert_to_area(..)
    flagNum = blockMaxSize - length;  // The number of successfully inserted
    // elements, flagNum will be return
    length_new = blockMaxSize;
  } else {
    flagNum = 0;
    length_new = length + n;
  }

  // cout << n << " " << length << " " << length_new << endl;
  if (!SORTED)
    insert_element_to_free_space(val_f, pos_f, (flagNum > 0)?flagNum:n);
  else {

    if (BATCH_BLOCK_INSERT) {
      // Batch insert to block
      // Merge two sorted array
      int k, i, j;
      for (k = length_new - 1, i = length - 1, j = length_new - length - 1; i >= 0 && j >= 0;)
        {
          if (val_f[j] >= val[i])
          {
            val[k] = val_f[j];
            pos[k--] = pos_f[j--];
          }
          else
          {
            val[k] = val[i];
            pos[k--] = pos[i--];
          }
        }
        while (j >= 0)
        {
          val[k] = val_f[j];
          pos[k--] = pos_f[j--];
        }
    }
    else {
      // Separate insert to block
      int insert_num = (flagNum > 0)?flagNum:n;
      for (int j = 0; j < insert_num; j++){
        int noflag = 0;
        for (int i = 0; i < length; i++) {
          if (val[i] > val_f[j]) {
            noflag = 1;
            for (int k = length; k > i; k--) {
              val[k] = val[k-1];
              pos[k] = pos[k-1];
            }
            val[i] = val_f[j];
            pos[i] = pos_f[j];
            length = length + 1;
            break;
          }
        }
        if (!noflag) {
          val[length] = val_f[j];
          pos[length] = pos_f[j];
          length = length + 1;
        }
      }
    }
    
  }
  // cout << "done" << endl;
  /* // Merge two sorted array
  for (int k = length; k < length_new; k++){
    val[k] = val_f[k - length];
    pos[k] = pos_f[k - length];

    // need to split reverse_bit and make it atomic
    reverse_bit(k);
  } */

  length = length_new;      
  // pop.persist(val);
  // pop.persist(pos);

  
  if(DEBUG_TIME_COUNT) timer.commonGetEndTime(2);
  return flagNum;
}

// A new function to insert elements into pos_block.
// Use bitmap to find a empty place to put element in.
// Revise sorted slot array for sequuence.
// Pos_block's pos array will be unsorted.
// Something important is that this part will take one element as input
// since unsorted elements may arrive one by one 
// This part will replace the old one after test.
int pos_block::new_insert_to_block( CODE val_f, POSTYPE pos_f) {
  // Insert max(n, #vacancy) elements to a block, return 0 if the block is still
  // not filled up.

  // cout << "[+]insert to block" << endl;

  int flagNum, length_new;
  // cout << "[+]block length:" << length << endl;
  if ((1 + length) > blockMaxSize) {
    // Block filled up! This block will be splitted in insert_to_area(..)
    // return -1 to split the block
    cout << "[+]block filled up" << endl;
    return -1;
  } else {
    flagNum = 0;
    length_new = length + 1;
  }

  // find an empty place
  int insert_position = find_first_space();
  // cout << "first space in " << insert_position << endl;
  // cout << "insert to this position" << endl;

  // insert val to this position
  val[insert_position] = val_f;
  pos[insert_position] = pos_f;

  length = length_new;
  if (BITMAP_USED)
    reverse_bit(insert_position);
  // cout << "[+]insert finished" << endl;
  return 1; // insert successfully
}

void Area::area_split_block(int block_idx) {
  assert(blockNum < blockNumMax);
  // cout << "split start " << endl;

  auto pb_old = blocks[block_idx];
  int half_length = pb_old->length / 2;

  CODE *data_to_old = (CODE *)malloc(half_length * sizeof(CODE));
  POSTYPE *pos_to_old = (POSTYPE *)malloc(half_length * sizeof(POSTYPE));
  CODE *data_to_new = (CODE *)malloc(half_length * sizeof(CODE));
  POSTYPE *pos_to_new = (POSTYPE *)malloc(half_length * sizeof(POSTYPE));

  POSTYPE *idx = argsort(pb_old->val.get(), pb_old->length);
  for (int i = 0; i < half_length; i++) {
    data_to_old[i] = pb_old->val[idx[i]];
  }

  for (int i = 0; i < half_length; i++) {
    pos_to_old[i] = pb_old->pos[idx[i]];
  }

  for (int i = 0; i < half_length; i++) {
    data_to_new[i] = pb_old->val[idx[i + half_length]];
  }

  for (int i = 0; i < half_length; i++) {
    pos_to_new[i] = pb_old->pos[idx[i + half_length]];
  }
  free(idx);
  pb_old->rebuild_pos_block(data_to_old,pos_to_old,half_length);
  pobj::persistent_ptr<pos_block> pb_new;
  // flush(cout);
  // sleep(1);
  if(DEBUG_TIME_COUNT) timer.commonGetStartTime(5);
  pb_new = freeBlockList->getFreeBlock();
  if(DEBUG_TIME_COUNT) timer.commonGetEndTime(5);
  // cout << "done" << endl;
  // pobj::make_persistent_atomic<pos_block>(pop,pb_new);
  pb_new->rebuild_pos_block(data_to_new,pos_to_new,half_length);
  for (int i = blockNum; i > (block_idx + 1); i--) {
    blocks[i] = blocks[i - 1];
  }

  // cout << "insert into block array" << endl;
  blocks[block_idx + 1] = pb_new;

  blocks[block_idx + 1]->idx = block_idx + 1;

  blockNum = blockNum + 1;
  // cout << "split finished " << endl;
  // cout << block_idx << " length: " << blocks[block_idx]->length << endl;
  // cout << block_idx + 1 << " length: " << blocks[block_idx + 1]->length << endl;
}

void Area::area_split_block_sorted(int block_idx) {
  assert(blockNum < blockNumMax);
  // cout << "split start " << endl;

  auto pb_old = blocks[block_idx];
  pb_old->length = blockInitSize;  // Split pb_old into two blocks, only keep
  // half of the original values in pb_old

  pobj::persistent_ptr<pos_block> pb_new;
  // Fill values into new block
  pobj::make_persistent_atomic<pos_block>(pop,pb_new);
  pb_new->init_pos_block(pb_old->val.get() + blockInitSize, pb_old->pos.get() + blockInitSize, blockInitSize);
  
  // Update blocks in area
  for (int i = blockNum; i > (block_idx + 1); i--) {
    blocks[i] = blocks[i - 1];
  }
  blocks[block_idx + 1] = pb_new;

  blocks[block_idx + 1]->idx = block_idx + 1;

  blockNum = blockNum + 1;
  // cout << "split finished " << endl;
}

// insert elements to cells in free list
// if free list is all used, append elements to the end of block
int pos_block::insert_element_to_free_space(CODE *val_f, POSTYPE *pos_f, int n) {
  int p = free_idx;  // pointer to the free list
  int q = 0;  // pointer to the elements array
  int temp;
  while (p != -1 && q < n) {
    temp = pos[p];
    pos[p] = pos_f[q];
    val[p] = val_f[q];
    if (BITMAP_USED)
      reverse_bit(p);
    q++;
    p = temp;
  }

  free_idx = p;

  n = n - q;
  int temp_length = length + q;
  for (int i = 0; i < n; i++) {
    pos[i + temp_length] = pos_f[i + q];
    val[i + temp_length] = val_f[i + q];
    if (BITMAP_USED)
      reverse_bit(i + temp_length);
  }

   return 0;
}

void pos_block::insert_freepos_to_free_list(int new_free_idx) {
  if (free_idx == -1) {
    free_idx = new_free_idx;
    return;
  }

  int p = free_idx;
  int q = pos[p];

  while (q != -1) {
    p = pos[p];
    q = pos[p];
  }

  pos[p] = new_free_idx;
  return;
}

void Area::vec_insert_to_area(CODE *raw_data,vector <POSTYPE>&pos, int n) {
  // TODO: Inserting too many elements into an area will explode (the blocks
  // array will be filled up), automatically rebuilding the whole BinDex
  // structure (or enlarging current area) is needed in future implementation

  // TODO: use multi-thread here, add lock in insert_to_block (the update of slot array and block) 
  // sorted or unsorted ? that's a question    maybe we should just test all of them
  // this function will use sorted data. 
  // display_area();
  int block_num_int = blockNum;

  if (DEBUG)  show_volume();
  
  vector <POSTYPE> pos_array[block_num_int];
  int flag = 0;
  int splitflag = 0;

  // cout << endl << "[+]insert Num " << n << endl;
  for (int i = 0; i < n; i++) {
    if (DEBUG)  show_volume();
    for (int j = 0; j < blockNum; j++) {
      if (raw_data[pos[i]] < blocks[j]->block_start_value()){
        if(j == 0) {
          splitflag = blocks[0]->new_insert_to_block(raw_data[pos[i]],pos[i]);
          flag = 1;
          if (splitflag == -1){
            // show_volume();
            area_split_block(0);
            // show_volume();
            splitflag = 0;
            flag = 2;
            i--;
          }
        }
        else {
          splitflag = blocks[j-1]->new_insert_to_block(raw_data[pos[i]],pos[i]);
          flag = 1;
          if (splitflag == -1){
            // show_volume();
            area_split_block(j-1);
            // show_volume();
            splitflag = 0;
            flag = 2;
            i--;
          }
        }
        break;
      }
    }
    if (flag == 0){
      splitflag = blocks[blockNum - 1]->new_insert_to_block(raw_data[pos[i]],pos[i]);
      if (splitflag == -1){
        // show_volume();
        area_split_block(blockNum - 1);
        // show_volume();
        splitflag = 0;
        i--;
      }
    }
    else {
      flag = 0;
    }
    if (DEBUG)  show_volume();
  }
  length = length + n;
  if (DEBUG)  show_volume();
}

void Area::single_insert_to_area(CODE val, POSTYPE pos) {
  // TODO: Inserting too many elements into an area will explode (the blocks
  // array will be filled up), automatically rebuilding the whole BinDex
  // structure (or enlarging current area) is needed in future implementation

  // TODO: use multi-thread here, add lock in insert_to_block (the update of slot array and block) 
  // sorted or unsorted ? that's a question    maybe we should just test all of them
  // this function will use sorted data. 
  int flagnum = 0;
  int overflow = 0;
  // this code is shxx , fix this later
  for (int i = 0; i < blockNum; i++){
    if (val < blocks[i]->block_start_value()){
      if (i == 0){
        overflow = blocks[0]->new_insert_to_block(val, pos);
        if(overflow == -1){
          area_split_block(0);
          if (val < blocks[1]->block_start_value())
            overflow = blocks[0]->new_insert_to_block(val, pos);
          else
            overflow = blocks[1]->new_insert_to_block(val, pos);
        }
      }
      else {
        overflow = blocks[i-1]->new_insert_to_block(val, pos);
        if(overflow == -1){
          area_split_block(i-1);
          if (val < blocks[i]->block_start_value())
            overflow = blocks[i-1]->new_insert_to_block(val, pos);
          else
            overflow = blocks[i]->new_insert_to_block(val, pos);
        }
      }
      flagnum = 1;
      break;
    }
  }
  if (flagnum == 0){
    overflow = blocks[blockNum-1]->new_insert_to_block(val, pos);
    if(overflow == -1){
      area_split_block(blockNum-1);
      if (val < blocks[blockNum-2]->block_start_value())
            overflow = blocks[blockNum-2]->new_insert_to_block(val, pos);
          else
            overflow = blocks[blockNum-1]->new_insert_to_block(val, pos);
    }
  }

  
  length = length + 1;
}

void Area::insert_to_area_fixed(CODE *val, POSTYPE *pos, int n) {
  // TODO: Inserting too many elements into an area will explode (the blocks
  // array will be filled up), automatically rebuilding the whole BinDex
  // structure (or enlarging current area) is needed in future implementation

  // TODO: use multi-thread here, add lock in insert_to_block (the update of slot array and block) 

  int i, j;
  for (i = 0, j = 0; i < n && j < blockNum - 1;) {
    int num_insert_to_block = 0;
    int start = i;
    while (val[i] < blocks[j + 1]->block_start_value() && i < n) {
      i++;
      num_insert_to_block++;
    }
    if (num_insert_to_block) {
      int flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
      while (flagNum) {
        area_split_block(j++);
        num_insert_to_block -= flagNum;
        start += flagNum;
        flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
      }
    }
    j++;
  }

  if (i < n) {  // Insert val[i] to the end of last block if val[i] is no less
    // than the maximum value of current area
    int num_insert_to_block = n - i;
    int start = i;
    int flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
    while (flagNum) {
      area_split_block(j++);
      num_insert_to_block -= flagNum;
      start += flagNum;
      flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
    }
  }
  length = length + n;
}

void Area::insert_to_area(CODE *val, POSTYPE *pos, int n) {
  // TODO: Inserting too many elements into an area will explode (the blocks
  // array will be filled up), automatically rebuilding the whole BinDex
  // structure (or enlarging current area) is needed in future implementation

  // TODO: use multi-thread here, add lock in insert_to_block (the update of slot array and block) 



  int i, j;
  for (i = 0, j = 0; i < n && j < blockNum;) {
    int num_insert_to_block = 0;
    int start = i;

    if (j == blockNum - 1) {
        num_insert_to_block = n - i;
        i = n;
    }
    else {
      while (val[i] < blocks[j + 1]->block_start_value() && i < n) {
        i++;
        num_insert_to_block++;
      }
    }

    if (num_insert_to_block) {
      int flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
      if (flagNum) {
        if(DEBUG_TIME_COUNT) timer.commonGetStartTime(3);
        if(!SORTED)
          area_split_block(j);
        else
          area_split_block_sorted(j);
        if(DEBUG_TIME_COUNT) timer.commonGetEndTime(3);
        i -= (num_insert_to_block - flagNum);
        continue;
      }
    }
    j++;
  }

  length = length + n;

}

void Area::insert_to_area_batch(CODE *val, POSTYPE *pos, int n) {
  // TODO: Inserting too many elements into an area will explode (the blocks
  // array will be filled up), automatically rebuilding the whole BinDex
  // structure (or enlarging current area) is needed in future implementation

  // TODO: use multi-thread here, add lock in insert_to_block (the update of slot array and block) 
  // insert_to_area_batch_exec(val, pos, n, 0, blockNum);


  int i, j;
  for (i = 0, j = 0; i < n && j < blockNum;) {
    int num_insert_to_block = 0;
    int start = i;

    if (j == blockNum - 1) {
        num_insert_to_block = n - i;
        i = n;
    }
    else {
      while (val[i] < blocks[j + 1]->block_start_value() && i < n) {
        i++;
        num_insert_to_block++;
      }
    }

    if (num_insert_to_block) {
       if (num_insert_to_block >= blockMaxSize) {
        // first init new blocks and split old block + newVal into it
        if(DEBUG_TIME_COUNT) timer.commonGetStartTime(3);
        area_split_several_blocks(j, val + start, pos + start, num_insert_to_block);
        if(DEBUG_TIME_COUNT) timer.commonGetEndTime(3);
        continue;
      }
      if(DEBUG_TIME_COUNT) timer.commonGetStartTime(2);
      int flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
      if(DEBUG_TIME_COUNT) timer.commonGetEndTime(2);
      if (flagNum) { 
        if(DEBUG_TIME_COUNT) timer.commonGetStartTime(3);
        if(!SORTED)
          area_split_block(j);
        else
          area_split_block_sorted(j);
        if(DEBUG_TIME_COUNT) timer.commonGetEndTime(3);
        i -= (num_insert_to_block - flagNum);
        continue;
      }
    }
    j++;
  }

  length = length + n;


}


void Area::insert_to_area_batch_exec(CODE *val, POSTYPE *pos, int n, int startBlockIdx, int endBlockIdx) {
  // TODO: Inserting too many elements into an area will explode (the blocks
  // array will be filled up), automatically rebuilding the whole BinDex
  // structure (or enlarging current area) is needed in future implementation

  // TODO: use multi-thread here, add lock in insert_to_block (the update of slot array and block) 

  int i, j;
  for (i = 0, j = startBlockIdx; i < n && j < endBlockIdx;) {
    int num_insert_to_block = 0;
    int start = i;

    if (j == endBlockIdx - 1) {
        num_insert_to_block = n - i;
        i = n;
    }
    else {
      while (val[i] < blocks[j + 1]->block_start_value() && i < n) {
        i++;
        num_insert_to_block++;
      }
    }

    if (num_insert_to_block) {
      if (num_insert_to_block > blockMaxSize) {
        // first init new blocks and split old block + newVal into it
        area_split_several_blocks(j, val + start, pos + start, num_insert_to_block);


        // int newBlockNum = (num_insert_to_block + blocks[j]->length) / blockMaxSize;
        /* insert_to_area_batch_exec(val + start,
                                  pos + start,
                                  num_insert_to_block,
                                  j,
                                  j + newBlockNum); */
        continue;
      }
      int flagNum = blocks[j]->insert_to_block( val + start, pos + start, num_insert_to_block);
      if (flagNum) {
        area_split_block(j);
        i -= (num_insert_to_block - flagNum);
        continue;
      }
    }
    j++;
  }

}


void Area::area_split_several_blocks(int blockIdx, CODE *val, POSTYPE *pos, int n) {
  assert(blockNum < blockNumMax);
  // show_volume();
  // cout << "split start " << endl;

  auto pb_old = blocks[blockIdx];

  int newBlockNum = (n + pb_old->length) / blockMaxSize;
  int dlength = (n + pb_old->length) / (newBlockNum + 1) ;  // number to each block
  int llength = (n + pb_old->length) % (newBlockNum + 1) ;  // number left
  CODE *data_to_old = (CODE *)malloc((dlength + llength) * sizeof(CODE));
  POSTYPE *pos_to_old = (POSTYPE *)malloc((dlength + llength) * sizeof(POSTYPE));
  CODE *data_to_new[newBlockNum]; // = (CODE *)malloc(half_length * sizeof(CODE));
  POSTYPE *pos_to_new[newBlockNum]; // = (POSTYPE *)malloc(half_length * sizeof(POSTYPE));

  CODE *newVal = (CODE *)malloc((n + pb_old->length) * sizeof(CODE));
  POSTYPE *newPos = (POSTYPE *)malloc((n + pb_old->length) * sizeof(POSTYPE));

  memcpy(newVal, pb_old->val.get(), pb_old->length * sizeof(CODE));
  memcpy(newVal + pb_old->length, val, n * sizeof(CODE));
  memcpy(newPos, pb_old->pos.get(), pb_old->length * sizeof(POSTYPE));
  memcpy(newPos + pb_old->length, pos, n * sizeof(POSTYPE));

  for (int i = 0; i < newBlockNum; i++) {
    data_to_new[i] = (CODE *)malloc(dlength * sizeof(CODE));
    pos_to_new[i] = (POSTYPE *)malloc(dlength * sizeof(POSTYPE));
  }
  POSTYPE *idx = argsort(newVal, n + pb_old->length);

  for (int i = 0; i < (dlength + llength); i++) {
    data_to_old[i] = newVal[idx[i]];
  }

  for (int i = 0; i < (dlength + llength); i++) {
    pos_to_old[i] = newPos[idx[i]];
  }

  for (int i = 0 ; i < newBlockNum; i++) {
    for (int j = 0; j < dlength; j++) {
      data_to_new[i][j] = newVal[idx[llength + dlength * (i + 1) + j]];
      pos_to_new[i][j] = newPos[idx[llength + dlength * (i + 1) + j]];
    }
  }

  free(idx);


  for (int i = blockNum + newBlockNum - 1; i > (blockIdx + newBlockNum); i--) {
    blocks[i] = blocks[i - newBlockNum];
  }

  pb_old->rebuild_pos_block(data_to_old, pos_to_old, dlength + llength);

  for (int i = 0; i < newBlockNum; i++) {
    pobj::persistent_ptr<pos_block> pb_new;
    flush(cout);
    if(DEBUG_TIME_COUNT) timer.commonGetStartTime(5);
    pb_new = freeBlockList->getFreeBlock();
    pb_new->rebuild_pos_block(data_to_new[i], pos_to_new[i], dlength);
    if(DEBUG_TIME_COUNT) timer.commonGetEndTime(5);
    // pobj::make_persistent_atomic<pos_block>(pop, pb_new);
    // pb_new->init_pos_block(data_to_new[i], pos_to_new[i], dlength);
    blocks[blockIdx + i + 1] = pb_new;
    blocks[blockIdx + i + 1]->idx = blockIdx + i + 1;
  }

  // cout << "insert into block array" << endl;
  blockNum = blockNum + newBlockNum;
}

void BinDex::append_to_bindex_unsorted(CODE *new_data, POSTYPE n) {
  // copy new data to bindex->raw_data
  cout << "raw data length: " << length << endl;
  cout << "insert " << n << " data" << endl;
  cout << "[+]copy new data to raw data" << endl;
  memcpy(&raw_data[length],new_data,n * sizeof(CODE));

  std::thread threads[THREAD_NUM];
  int i, j, k;
  // Update the areas
  int accum_add_count = 0;

  CODE start_value[K];
  for (int z = 0; z < K; z++){
    start_value[z] = areas[z]->area_start_value();
  }

  cout << "[+]scan new data and split into different areas" << endl;
  int flag = 0;
  vector <POSTYPE>pos_array[K];
  for (i = 0; i < n; i++) {
    for (j = 0; j < K; j++){
      // TODO: Here a naive linear search is used for calculating the
      // areaStartIdx, optimizations may(?may not) be done in future implementation
      if (new_data[i] < start_value[j]) {
        if(j == 0) {
          pos_array[0].push_back(i + length);
          flag = 1;
        }
        else {
          pos_array[j-1].push_back(i + length);
          flag = 1;
        }
        break;
      }
    }
    if (flag == 0){
      pos_array[K - 1].push_back(i + length);
    }
    else {
      flag = 0;
    }
  }


  /* int insert_sum = 0;
  for (int p = 0; p < K; p++){
    // cout << "Area " << p << ": " << pos_array[p].size() << endl;
    cout << "Area " << p << ": " << pos_array[p].size() << " startValue: " << areas[p]->area_start_value() << " " << start_value[p] << endl;
    insert_sum += pos_array[p].size();
  }
  cout << "Sum: " << insert_sum << endl; */

  // show_volume();

  cout << "[+]insert to area" << endl;
  for (k = 0; k * THREAD_NUM < K; k++) {
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      i = k * THREAD_NUM + j;
      // cout << "[+]insert to area "<< i << endl;
      areas[i]->vec_insert_to_area(this->raw_data.get(),pos_array[i],pos_array[i].size());
    }
  }

  // show_volume();
  
  /* for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
    threads[j].join();
  } */

  cout << "[+]insert to filter vectors" << endl;
  // Append to the filter vectors
  for (k = 0; k * THREAD_NUM < (K - 1); k++) {
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      i = k * THREAD_NUM + j;
      threads[j] = std::thread(append_fv_val_less, filterVectors[i].get(), length, new_data,
                               areas[i + 1]->area_start_value(), n);
    }
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      threads[j].join();
    }
  }

  length += n;
  // display_bindex();
}


void BinDex::single_append_to_bindex_unsorted(CODE *new_data, POSTYPE n) {
  // copy new data to bindex->raw_data
  cout << "raw data length: " << length << endl;
  cout << "insert " << n << " data" << endl;
  cout << "[+]copy new data to raw data" << endl;
  memcpy(&raw_data[length],new_data,n * sizeof(CODE));

  std::thread threads[THREAD_NUM];
  int i, j, k;
  // Update the areas
  int accum_add_count = 0;

  CODE start_value[K];
  for (int z = 0; z < K; z++){
    start_value[z] = areas[z]->area_start_value();
  }

  cout << "[+]scan new data and split into different areas" << endl;
  int flag = 0;
  for (i = 0; i < n; i++) {
    for (j = 0; j < K; j++){
      // TODO: Here a naive linear search is used for calculating the
      // areaStartIdx, optimizations may(?may not) be done in future implementation
      if (new_data[i] < start_value[j]) {
        if(j == 0) {
          // pos_array[0].push_back(i + length);
          areas[0]->single_insert_to_area(raw_data.get()[i + length],i + length);
          flag = 1;
        }
        else {
          // pos_array[j-1].push_back(i + length);
          areas[j-1]->single_insert_to_area(raw_data.get()[i + length],i + length);
          flag = 1;
        }
        break;
      }
    }
    if (flag == 0){
      // pos_array[K - 1].push_back(i + length);
      areas[K-1]->single_insert_to_area(raw_data.get()[i + length],i + length);
    }
    else {
      flag = 0;
    }
  }

  cout << "[+]insert to filter vectors" << endl;
  // Append to the filter vectors
  for (k = 0; k * THREAD_NUM < (K - 1); k++) {
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      i = k * THREAD_NUM + j;
      threads[j] = std::thread(append_fv_val_less, filterVectors[i].get(), length, new_data,
                               areas[i + 1]->area_start_value(), n);
    }
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      threads[j].join();
    }
  }

  length += n;
}

void BinDex::append_to_bindex_and_rebuild(CODE *new_data, POSTYPE n) {
  CODE *merged_data = (CODE *)malloc((length + n)* sizeof(CODE));
  memcpy(merged_data,raw_data.get(),length * sizeof(CODE));
  memcpy(merged_data + length, new_data, n * sizeof(CODE));  // TODO:!!!!
  rebuild_bindex(merged_data, length + n);
}

void BinDex::append_to_bindex(CODE *new_data, POSTYPE n) {

  if(DEBUG_TIME_COUNT) timer.commonGetStartTime(7);
  POSTYPE *idx = argsort(new_data, n);
  if(DEBUG_TIME_COUNT) timer.commonGetEndTime(7);


  CODE *data_sorted = (CODE *)malloc(n * sizeof(CODE));
  POSTYPE areaStartIdx[K];
  POSTYPE *new_pos = (POSTYPE *)malloc(n * sizeof(POSTYPE));
  bool batch = false;
  if(DEBUG_TIME_COUNT) timer.commonGetStartTime(8);
  areaStartIdx[0] = 0;
  int k = 1;
  for (POSTYPE i = 0; i < n; i++) {
    // TODO: multithread appending should(?) be done in future implementation
    data_sorted[i] = new_data[idx[i]];
    new_pos[i] = idx[i] + length;
    raw_data[length + i] = new_data[i];
    while (k < K && data_sorted[i] >= areas[k]->area_start_value()) {
      // TODO: Here a naive linear search is used for calculating the
      // areaStartIdx, optimizations may(?may not) be done in future implementation
      areaStartIdx[k++] = i;
    }
  }
  while (k < K) {
    areaStartIdx[k++] = n;
  }
  if(DEBUG_TIME_COUNT) timer.commonGetEndTime(8);
  std::thread threads[THREAD_NUM];
  int i, j;
  // Update the areas
  int accum_add_count = 0;
  for (k = 0; k * THREAD_NUM < K; k++) {
    if(DEBUG_TIME_COUNT) timer.commonGetStartTime(4);
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      i = k * THREAD_NUM + j;
      int num_added = num_insert_to_area(areaStartIdx, i, n);
      accum_add_count += num_added;
      /* threads[j] = std::thread(&Area::insert_to_area,areas[i], data_sorted + areaStartIdx[i],
                               new_pos + areaStartIdx[i], num_added);  */
      
      if (BATCH_SPLIT)
      {
        /* areas[i]->insert_to_area_batch(data_sorted + areaStartIdx[i],
                                       new_pos + areaStartIdx[i], num_added); */
        threads[j] = std::thread(&Area::insert_to_area_batch, areas[i], data_sorted + areaStartIdx[i],
                                 new_pos + areaStartIdx[i], num_added);
      }
      else
      {
        /* areas[i]->insert_to_area(data_sorted + areaStartIdx[i],
                                 new_pos + areaStartIdx[i], num_added); */
        threads[j] = std::thread(&Area::insert_to_area, areas[i], data_sorted + areaStartIdx[i],
                                 new_pos + areaStartIdx[i], num_added);
      }
      area_counts[i] += accum_add_count;
    }
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++) {
      threads[j].join();
    }
    if(DEBUG_TIME_COUNT) timer.commonGetEndTime(4);
  }

  // Append to the filter vectors
  if(DEBUG_TIME_COUNT) timer.commonGetStartTime(6);
  for (k = 0; k * THREAD_NUM < (K - 1); k++) {
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      i = k * THREAD_NUM + j;
      threads[j] = std::thread(append_fv_val_less, filterVectors[i].get(), length, new_data,
                               areas[i + 1]->area_start_value(), n);
    }
    for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++) {
      threads[j].join();
    }
  }
  if(DEBUG_TIME_COUNT) timer.commonGetEndTime(6);

  length += n;
  // display_bindex();
  // free(idx);
}


