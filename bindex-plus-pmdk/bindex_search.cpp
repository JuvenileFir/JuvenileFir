#include "bindex.h"

int prefetch_stride = 6;
extern bool BITMAP_USED;

void copy_bitmap(BITS *result, BITS *ref, int n, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  memcpy(result, ref, n * sizeof(BITS));

  // for (int i = 0; i < n; i++) {
  //     result[i] = ref[i];
  // }
}

void copy_bitmap_not(BITS *result, BITS *ref, int start_n, int end_n, int t_id) {
  int jobs = ROUNDUP_DIVIDE(end_n - start_n, THREAD_NUM);
  int start = start_n + t_id * jobs;
  int end = start_n + (t_id + 1) * jobs;
  if (end > end_n) end = end_n;
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  // memcpy(result, ref, n * sizeof(BITS));

  // TODO: bitwise operation on large memory block?
  for (int i = start; i < end; i++) {
    result[i] = ~(ref[i]);
  }
}

void copy_bitmap_bt(BITS *result, BITS *ref_l, BITS *ref_r, int start_n, int end_n, int t_id) {
  int jobs = ROUNDUP_DIVIDE(end_n - start_n, THREAD_NUM);
  int start = start_n + t_id * jobs;
  int end = start_n + (t_id + 1) * jobs;
  if (end > end_n) end = end_n;

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  // memcpy(result, ref, n * sizeof(BITS));

  // TODO: bitwise operation on large memory block?
  for (int i = start; i < end; i++) {
    result[i] = ref_r[i] & (~(ref_l[i]));
  }
}

void copy_bitmap_bt_simd(BITS *to, BITS *from_l, BITS *from_r, int bitmap_len, int t_id) {
  int jobs = ((bitmap_len / SIMD_JOB_UNIT - 1) / THREAD_NUM + 1) * SIMD_JOB_UNIT;

  assert(jobs % SIMD_JOB_UNIT == 0);
  assert(bitmap_len % SIMD_JOB_UNIT == 0);
  assert(jobs * THREAD_NUM >= bitmap_len);

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }

  int cur = t_id * jobs;
  int end = (t_id + 1) * jobs;
  if (end > bitmap_len) end = bitmap_len;

  while (cur < end) {
    __m256i buf_l = _mm256_load_si256((__m256i *)(from_l + cur));
    __m256i buf_r = _mm256_load_si256((__m256i *)(from_r + cur));
    __m256i buf = _mm256_andnot_si256(buf_l, buf_r);
    _mm256_store_si256((__m256i *)(to + cur), buf);
    cur += SIMD_JOB_UNIT;
  }
}

void copy_bitmap_simd(BITS *to, BITS *from, int bitmap_len, int t_id) {
  int jobs = ((bitmap_len / SIMD_JOB_UNIT - 1) / THREAD_NUM + 1) * SIMD_JOB_UNIT;

  assert(jobs % SIMD_JOB_UNIT == 0);
  assert(bitmap_len % SIMD_JOB_UNIT == 0);
  assert(jobs * THREAD_NUM >= bitmap_len);

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }

  int cur = t_id * jobs;
  int end = (t_id + 1) * jobs;
  if (end > bitmap_len) end = bitmap_len;

  while (cur < end) {
    __m256i buf = _mm256_load_si256((__m256i *)(from + cur));
    _mm256_store_si256((__m256i *)(to + cur), buf);
    cur += SIMD_JOB_UNIT;
  }
}

void copy_bitmap_not_simd(BITS *to, BITS *from, int bitmap_len, int t_id) {
  int jobs = ((bitmap_len / SIMD_JOB_UNIT - 1) / THREAD_NUM + 1) * SIMD_JOB_UNIT;

  assert(jobs % SIMD_JOB_UNIT == 0);
  assert(bitmap_len % SIMD_JOB_UNIT == 0);
  assert(jobs * THREAD_NUM >= bitmap_len);

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }

  int cur = t_id * jobs;
  int end = (t_id + 1) * jobs;
  if (end > bitmap_len) end = bitmap_len;

  while (cur < end) {
    __m256i buf = _mm256_load_si256((__m256i *)(from + cur));
    buf = ~buf;
    _mm256_store_si256((__m256i *)(to + cur), buf);
    cur += SIMD_JOB_UNIT;
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

void copy_filter_vector(BinDex *bindex, BITS *result, int k) {
  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);
  // BITS* result = (BITS*)aligned_alloc(SIMD_ALIGEN, bitmap_len *
  // sizeof(BITS));

  if (k < 0) {
    memset_mt(result, 0, bitmap_len);
    return;
  }

  if (k >= (K - 1)) {
    memset_mt(result, 0xFF, bitmap_len);  // Slower(?) than a loop
    return;
  }

  // simd copy
  // int mt_bitmap_n = (bitmap_len / SIMD_JOB_UNIT) * SIMD_JOB_UNIT; // must
  // be SIMD_JOB_UNIT aligened for (int i = 0; i < THREAD_NUM; i++)
  //     threads[i] = std::thread(copy_bitmap_simd, result,
  //     bindex->filterVectors[k], mt_bitmap_n, i);
  // for (int i = 0; i < THREAD_NUM; i++)
  //     threads[i].join();
  // memcpy(result + mt_bitmap_n, bindex->filterVectors[k] + mt_bitmap_n,
  // bitmap_len - mt_bitmap_n);

  // naive copy
  int avg_workload = bitmap_len / THREAD_NUM;
  int i;
  for (i = 0; i < THREAD_NUM - 1; i++) {
    threads[i] = std::thread(copy_bitmap, result + (i * avg_workload), bindex->filterVectors[k].get() + (i * avg_workload),
                             avg_workload, i);
  }
  threads[i] = std::thread(copy_bitmap, result + (i * avg_workload), bindex->filterVectors[k].get() + (i * avg_workload),
                           bitmap_len - (i * avg_workload), i);

  for (i = 0; i < THREAD_NUM; i++) {
    threads[i].join();
  }
}

void copy_filter_vector_not(BinDex *bindex, BITS *result, int k) {
  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);
  // BITS* result = (BITS*)aligned_alloc(SIMD_ALIGEN, bitmap_len *
  // sizeof(BITS));

  if (k < 0) {
    memset_mt(result, 0xFF, bitmap_len);
    return;
  }

  if (k >= (K - 1)) {
    memset_mt(result, 0, bitmap_len);  // Slower(?) than a loop
    return;
  }

  // simd copy not
  int mt_bitmap_n = (bitmap_len / SIMD_JOB_UNIT) * SIMD_JOB_UNIT;  // must be SIMD_JOB_UNIT aligened
  for (int i = 0; i < THREAD_NUM; i++)
    threads[i] = std::thread(copy_bitmap_not_simd, result, bindex->filterVectors[k].get(), mt_bitmap_n, i);
  for (int i = 0; i < THREAD_NUM; i++) threads[i].join();
  for (int i = 0; i < bitmap_len - mt_bitmap_n; i++) {
    (result + mt_bitmap_n)[i] = ~((bindex->filterVectors[k].get() + mt_bitmap_n)[i]);
  }

  // naive copy
  // for (int i = 0; i < THREAD_NUM; i++) {
  //   threads[i] = std::thread(copy_bitmap_not, result,
  //   bindex->filterVectors[k],
  //                            0, bitmap_len, i);
  // }

  // for (int i = 0; i < THREAD_NUM; i++) {
  //   threads[i].join();
  // }

  return;
}

void copy_filter_vector_bt(BinDex *bindex, BITS *result, int kl, int kr) {
  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);

  // TODO: finish this
  if (kr < 0) {
    // assert(0);
    // printf("1\n");
    memset_mt(result, 0, bitmap_len);
    return;
  } else if (kr >= (K - 1)) {
    // assert(0);
    // printf("2\n");
    copy_filter_vector_not(bindex, result, kl);
    return;
  }
  if (kl < 0) {
    // assert(0);
    // printf("3\n");
    copy_filter_vector(bindex, result, kr);
    return;
  } else if (kl >= (K - 1)) {
    // assert(0);
    // printf("4\n");
    memset_mt(result, 0, bitmap_len);  // Slower(?) than a loop
    return;
  }

  // simd copy_bt
  int mt_bitmap_n = (bitmap_len / SIMD_JOB_UNIT) * SIMD_JOB_UNIT;  // must be SIMD_JOB_UNIT aligened
  for (int i = 0; i < THREAD_NUM; i++)
    threads[i] =
        std::thread(copy_bitmap_bt_simd, result, bindex->filterVectors[kl].get(), bindex->filterVectors[kr].get(), mt_bitmap_n, i);
  for (int i = 0; i < THREAD_NUM; i++) threads[i].join();
  for (int i = 0; i < bitmap_len - mt_bitmap_n; i++) {
    (result + mt_bitmap_n)[i] =
        (~((bindex->filterVectors[kl].get() + mt_bitmap_n)[i])) & ((bindex->filterVectors[kr].get() + mt_bitmap_n)[i]);
  }

  // naive copy
  // for (int i = 0; i < THREAD_NUM; i++) {
  //   threads[i] = std::thread(copy_bitmap_bt, result,
  //   bindex->filterVectors[kl],
  //                            bindex->filterVectors[kr], 0, bitmap_len, i);
  // }

  // for (int i = 0; i < THREAD_NUM; i++) {
  //   threads[i].join();
  // }
}

void copy_bitmap_xor_simd(BITS *to, BITS *bitmap1, BITS *bitmap2,
                          int bitmap_len, int t_id) {
  int jobs =
      ((bitmap_len / SIMD_JOB_UNIT - 1) / THREAD_NUM + 1) * SIMD_JOB_UNIT;

  assert(jobs % SIMD_JOB_UNIT == 0);
  assert(bitmap_len % SIMD_JOB_UNIT == 0);
  assert(jobs * THREAD_NUM >= bitmap_len);

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }

  int cur = t_id * jobs;
  int end = (t_id + 1) * jobs;
  if (end > bitmap_len) end = bitmap_len;

  while (cur < end) {
    __m256i buf1 = _mm256_load_si256((__m256i *)(bitmap1 + cur));
    __m256i buf2 = _mm256_load_si256((__m256i *)(bitmap2 + cur));
    __m256i buf = _mm256_andnot_si256(buf1, buf2);
    _mm256_store_si256((__m256i *)(to + cur), buf);
    cur += SIMD_JOB_UNIT;
  }
}

void copy_filter_vector_xor(BinDex *bindex, BITS *result, int kl, int kr) {
  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);

  // TODO: finish this
  if (kr < 0) {
    assert(0);
    // printf("1\n");
    memset_mt(result, 0, bitmap_len);
    return;
  } else if (kr >= (K - 1)) {
    assert(0);
    // printf("2\n");
    copy_filter_vector_not(bindex, result, kl);
    return;
  }
  if (kl < 0) {
    // assert(0);
    // printf("3\n");
    copy_filter_vector(bindex, result, kr);
    return;
  } else if (kl >= (K - 1)) {
    assert(0);
    // printf("4\n");
    memset_mt(result, 0, bitmap_len);  // Slower(?) than a loop
    return;
  }

  // simd copy_xor
  int mt_bitmap_n = (bitmap_len / SIMD_JOB_UNIT) *
                    SIMD_JOB_UNIT;  // must be SIMD_JOB_UNIT aligened
  for (int i = 0; i < THREAD_NUM; i++)
    threads[i] =
        std::thread(copy_bitmap_xor_simd, result, bindex->filterVectors[kl].get(),
                    bindex->filterVectors[kr].get(), mt_bitmap_n, i);
  for (int i = 0; i < THREAD_NUM; i++) threads[i].join();
  for (int i = 0; i < bitmap_len - mt_bitmap_n; i++) {
    (result + mt_bitmap_n)[i] = ((bindex->filterVectors[kl].get() + mt_bitmap_n)[i]) ^
                                ((bindex->filterVectors[kr].get() + mt_bitmap_n)[i]);
  }

  // naive copy
  // for (int i = 0; i < THREAD_NUM; i++) {
  //   threads[i] = std::thread(copy_bitmap_bt, result,
  //   hydex->filterVectors[kl],
  //                            hydex->filterVectors[kr], 0, bitmap_len, i);
  // }

  // for (int i = 0; i < THREAD_NUM; i++) {
  //   threads[i].join();
  // }
}

int BinDex::in_which_area(CODE compare) {
  // TODO: could return -1?
  // Return the last area whose startValue is less than 'compare'
  // Obviously here a naive linear search is enough
  // Return -1 if 'compare' less than the first value in the virtual space
  // TODO: outdated comments, bad code, but worked
  if (compare < areas[0]->area_start_value()) return -1;
  for (int i = 0; i < K; i++) {
    CODE area_sv = areas[i]->area_start_value();
    if (area_sv == compare) return i;
    if (area_sv > compare) return i - 1;
  }
  return K - 1;
}

int Area::in_which_block(CODE compare) {
  // Binary search here to find which block the value of compare should locate
  // in (i.e. the last block whose startValue is less than 'compare')
  // TODO: outdated comments, bad code, but worked
  assert(compare >= area_start_value());
  int res = blockNum - 1;
  for (int i = 0; i < blockNum; i++) {
    CODE area_sv = blocks[i]->block_start_value();
    if (area_sv == compare) {
      res = i;
      break;
    }
    if (area_sv > compare) {
      res = i - 1;
      break;
    }
  }
  if (res) {
    pos_block *pre_blk = blocks[res - 1].get();
    if (pre_blk->val[pre_blk->length - 1] == compare) {
      res--;
    }
  }
  return res;

  int low = 0, high = blockNum, mid = (low + high) / 2;
  while (low < high) {
    if (compare <= blocks[mid]->block_start_value()) {
      high = mid;
    } else {
      low = mid + 1;
    }
    mid = (low + high) / 2;
  }
  return mid - 1;
  /*
  // TODO: do we need this?
  int res = mid - 1;
  if (res) {
    pos_block *pre_blk = area->blocks[res - 1];
    if (pre_blk->val[pre_blk->length - 1] == compare) {
      res--;
    }
  }
  return res;
  */
}

int pos_block::on_which_pos(CODE compare) {
  // Find the first value which is no less than 'compare', return length if
  // all data in the block are less than compare
  // assert(compare >= val[0]);
  assert(compare >= start_value);
  int low = 0, high = length, mid = (low + high) / 2;
  while (low < high) {
    if (val[mid] >= compare) {
      high = mid;
    } else {
      low = mid + 1;
    }
    mid = (low + high) / 2;
  }
  return mid;
}

vector<POSTYPE> pos_block::find_refine_array(CODE compare)
{
  vector<POSTYPE> refine_array;
  for (int i = 0; i < length; i++){
    if (val[i] < compare)
      refine_array.push_back(pos[i]);
  }
  return refine_array;
}

vector<POSTYPE> pos_block::find_not_refine_array(CODE compare)
{
  vector<POSTYPE> refine_array;
  for (int i = 0; i < length; i++){
    if (val[i] >= compare)
      refine_array.push_back(pos[i]);
  }
  return refine_array;
}


inline void refine(BITS *bitmap, POSTYPE pos) { bitmap[pos >> BITSSHIFT] ^= (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)); }

void refine_positions(BITS *bitmap, POSTYPE *pos, POSTYPE n) {
  for (int i = 0; i < n; i++) {
    refine(bitmap, *(pos + i));
  }
}

void new_refine_positions(BITS *bitmap, vector<POSTYPE> refine_array) {
  for (int i = 0; i < refine_array.size(); i++) {
    refine(bitmap, refine_array[i]);
  }
}

void refine_positions_mt(BITS *bitmap, Area *area, int start_blk_idx, int end_blk_idx, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  int jobs = ROUNDUP_DIVIDE(end_blk_idx - start_blk_idx, THREAD_NUM);
  int cur = start_blk_idx + t_id * jobs;
  int end = start_blk_idx + (t_id + 1) * jobs;
  if (end > end_blk_idx) end = end_blk_idx;

  // int prefetch_stride = 6;
  while (cur < end) {
    POSTYPE *pos_list = area->blocks[cur]->pos.get();
    POSTYPE n = area->blocks[cur]->length;
    int i;
    for (i = 0; i + prefetch_stride < n; i++) {
      if(prefetch_stride) {
        __builtin_prefetch(&bitmap[*(pos_list + i + prefetch_stride) >> BITSSHIFT], 1, 1);
      }
      POSTYPE pos = *(pos_list + i);
      __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT], (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
    }
    while (i < n) {
      POSTYPE pos = *(pos_list + i);
      __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT], (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
      i++;
    }
    cur++;
  }
}

void xor_bitmap_mt(BITS *bitmap, BITS *bitmap1, BITS *bitmap2, int start_n, int end_n, int t_id) {
  int jobs = ROUNDUP_DIVIDE(end_n - start_n, THREAD_NUM);
  int start = start_n + t_id * jobs;
  int end = start_n + (t_id + 1) * jobs;
  if (end > end_n) end = end_n;

  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }

  // TODO: bitwise operation on large memory block?
  for (int i = start; i < end; i++) {
    bitmap[i] = bitmap1[i] ^ bitmap2[i];
  }
}

void set_eq_bitmap_mt(BITS *bitmap, Area *area, CODE compare, int start_blk_idx, int end_blk_idx, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }

  int jobs = ROUNDUP_DIVIDE(end_blk_idx - start_blk_idx, THREAD_NUM);
  int start = start_blk_idx + t_id * jobs;
  int end = start_blk_idx + (t_id + 1) * jobs;
  if (end > end_blk_idx) end = end_blk_idx;

  // TODO: prefetch?
  // TODO: use direct pos_idx
  for (int i = start; i < end && area->blocks[i]->block_start_value() <= compare; i++) {
    pos_block *blk = area->blocks[i].get();
    POSTYPE *pos_list = blk->pos.get();
    POSTYPE n = blk->length;
    for (int j = 0; j < n && blk->val[j] <= compare; j++) {
      if (blk->val[j] == compare) {
        POSTYPE pos = *(pos_list + j);
        __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT], (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
      }
    }
  }
}

void refine_positions_in_blks_mt(BITS *bitmap, Area *area, int start_blk_idx,
                                 int end_blk_idx, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  int jobs = ROUNDUP_DIVIDE(end_blk_idx - start_blk_idx, THREAD_NUM);
  int cur = start_blk_idx + t_id * jobs;
  int end = start_blk_idx + (t_id + 1) * jobs;
  if (end > end_blk_idx) end = end_blk_idx;

  // int prefetch_stride = 6;
  while (cur < end) {
    POSTYPE *pos_list = area->blocks[cur]->pos.get();
    POSTYPE n = area->blocks[cur]->length;
    int i;
    for (i = 0; i + prefetch_stride < n; i++) {
      __builtin_prefetch(
          &bitmap[*(pos_list + i + prefetch_stride) >> BITSSHIFT], 1, 1);
      POSTYPE pos = *(pos_list + i);
      __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT],
                           (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
    }
    while (i < n) {
      POSTYPE pos = *(pos_list + i);
      __sync_fetch_and_xor(&bitmap[pos >> BITSSHIFT],
                           (1U << (BITSWIDTH - 1 - pos % BITSWIDTH)));
      i++;
    }
    cur++;
  }
}

void bindex_scan_lt(BinDex *bindex, BITS *result, CODE compare) {
  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);
  int area_idx = bindex->in_which_area( compare);
  if (area_idx < 0) {
    // 'compare' less than all raw_data, return all zero result
    // BITS* result = (BITS*)malloc(sizeof(BITS) * bitmap_len);
    memset_mt(result, 0, bitmap_len);
    return;
  }

  Area *area = bindex->areas[area_idx].get();
  int block_idx = area->in_which_block(compare);
  // int pos_idx = area->blocks[block_idx]->on_which_pos(compare);


// all pos < compare need to be refined in this block 
// fix this step later, merge it with if (is_upper_fv) {...}
  // vector<POSTYPE> refine_array = area->blocks[block_idx]->find_refine_array(compare); 
  // Do an estimation to select the filter vector which is most
  // similar to the correct result
  int is_upper_fv;
  int start_blk_idx, end_blk_idx;
  if (block_idx < bindex->areas[area_idx]->blockNum / 2) {
    is_upper_fv = 1;
    start_blk_idx = 0;
    end_blk_idx = block_idx;
  } else {
    is_upper_fv = 0;
    start_blk_idx = block_idx + 1;
    end_blk_idx = area->blockNum;
  }

  // fprintf(stderr, "area_idx: %d\nblock_idx: %d\npos_idx: %d\nis_upper_fv:
  // %d\nstart_blk_idx: %d\nend_blk_idx: %d\n", area_idx, block_idx, pos_idx,
  // is_upper_fv, start_blk_idx, end_blk_idx);

  // clang-format off
  PRINT_EXCECUTION_TIME("copy",
                        copy_filter_vector(bindex,
                                            result,
                                            is_upper_fv ? (area_idx - 1) : (area_idx)))

  PRINT_EXCECUTION_TIME("refine",
                        for (int i = 0; i < THREAD_NUM; i++) {
                          threads[i] = std::thread(refine_positions_mt, result, area, start_blk_idx, end_blk_idx, i);
                        }

                        for (int i = 0; i < THREAD_NUM; i++) {
                          threads[i].join();
                        }

                        if (is_upper_fv) {
                          // refine_positions(result, area->blocks[block_idx]->pos.get(), pos_idx);
                          new_refine_positions(result, area->blocks[block_idx]->find_refine_array(compare));
                        } else {
                          // refine_positions(result, area->blocks[block_idx]->pos.get() + pos_idx, area->blocks[block_idx]->length - pos_idx);
                          new_refine_positions(result, area->blocks[block_idx]->find_not_refine_array(compare));
                        })
  // clang-format on
}

void bindex_scan_le(BinDex *bindex, BITS *result, CODE compare) {
  // TODO: (compare + 1) overflow
  bindex_scan_lt(bindex, result, compare + 1);
}

void bindex_scan_gt(BinDex *bindex, BITS *result, CODE compare) {
  // TODO: (compare + 1) overflow
  compare = compare + 1;

  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);
  int area_idx = bindex->in_which_area( compare);
  if (area_idx < 0) {
    // 'compare' less than all raw_data, return all 1 result
    // BITS* result = (BITS*)malloc(sizeof(BITS) * bitmap_len);
    memset_mt(result, 0xFF, bitmap_len);
    return;
  }
  Area *area = bindex->areas[area_idx].get();
  int block_idx = area->in_which_block(compare);
  // int pos_idx = area->blocks[block_idx]->on_which_pos(compare);

  // Do an estimation to select the filter vector which is most
  // similar to the correct result
  int is_upper_fv;
  int start_blk_idx, end_blk_idx;
  if (block_idx < bindex->areas[area_idx]->blockNum / 2) {
    is_upper_fv = 1;
    start_blk_idx = 0;
    end_blk_idx = block_idx;
  } else {
    is_upper_fv = 0;
    start_blk_idx = block_idx + 1;
    end_blk_idx = area->blockNum;
  }

  // clang-format off
  PRINT_EXCECUTION_TIME("copy",
                        copy_filter_vector_not(bindex,
                                                result,
                                                is_upper_fv ? (area_idx - 1) : (area_idx)))

  PRINT_EXCECUTION_TIME("refine",
                        for (int i = 0; i < THREAD_NUM; i++) {
                          threads[i] = std::thread(refine_positions_mt, result, area, start_blk_idx, end_blk_idx, i);
                        }

                        for (int i = 0; i < THREAD_NUM; i++) {
                          threads[i].join();
                        }

                        /* if (is_upper_fv) {
                          refine_positions(result, area->blocks[block_idx]->pos.get(), pos_idx);
                        } else {
                          refine_positions(result, area->blocks[block_idx]->pos.get() + pos_idx, area->blocks[block_idx]->length - pos_idx);
                        }) */

                        if (is_upper_fv) {
                          new_refine_positions(result, area->blocks[block_idx]->find_refine_array(compare));
                        } else {
                          new_refine_positions(result, area->blocks[block_idx]->find_not_refine_array(compare));
                        })
  // clang-format on
}

void bindex_scan_ge(BinDex *bindex, BITS *result, CODE compare) {
  // TODO: (compare - 1) overflow
  bindex_scan_gt(bindex, result, compare - 1);
}

void bindex_scan_bt(BinDex *bindex, BITS *result, CODE compare1, CODE compare2) {
  assert(compare2 > compare1);
  // TODO: (compare1 + 1) overflow
  compare1 = compare1 + 1;

  std::thread threads[THREAD_NUM];
  int bitmap_len = bits_num_needed(bindex->length);

  // x > compare1
  int area_idx_l = bindex->in_which_area( compare1);
  if (area_idx_l < 0) {
    // TODO: finish this
    assert(0);
    // 'compare' less than all raw_data, return all 1 result
    // BITS* result = (BITS*)malloc(sizeof(BITS) * bitmap_len);
    memset_mt(result, 0xFF, bitmap_len);
    return;
  }
  Area *area_l = bindex->areas[area_idx_l].get();
  int block_idx_l = area_l->in_which_block(compare1);
  // int pos_idx_l = area_l->blocks[block_idx_l]->on_which_pos(compare1);

  // Do an estimation to select the filter vector which is most
  // similar to the correct result
  int is_upper_fv_l;
  int start_blk_idx_l, end_blk_idx_l;
  if (block_idx_l < bindex->areas[area_idx_l]->blockNum / 2) {
    is_upper_fv_l = 1;
    start_blk_idx_l = 0;
    end_blk_idx_l = block_idx_l;
  } else {
    is_upper_fv_l = 0;
    start_blk_idx_l = block_idx_l + 1;
    end_blk_idx_l = area_l->blockNum;
  }

  // x < compare2
  int area_idx_r = bindex->in_which_area(compare2);
  if (area_idx_r < 0) {
    // TODO: finish this
    assert(0);
    // 'compare' less than all raw_data, return all zero result
    // BITS* result = (BITS*)malloc(sizeof(BITS) * bitmap_len);
    memset_mt(result, 0, bitmap_len);
    return;
  }
  Area *area_r = bindex->areas[area_idx_r].get();
  int block_idx_r = area_r->in_which_block(compare2);
  // int pos_idx_r = area_r->blocks[block_idx_r]->on_which_pos(compare2);
  // Do an estimation to select the filter vector which is most
  // similar to the correct result
  int is_upper_fv_r;
  int start_blk_idx_r, end_blk_idx_r;
  if (block_idx_r < bindex->areas[area_idx_r]->blockNum / 2) {
    is_upper_fv_r = 1;
    start_blk_idx_r = 0;
    end_blk_idx_r = block_idx_r;
  } else {
    is_upper_fv_r = 0;
    start_blk_idx_r = block_idx_r + 1;
    end_blk_idx_r = area_r->blockNum;
  }

  // clang-format off
  PRINT_EXCECUTION_TIME("copy",
                        copy_filter_vector_bt(bindex,
                                              result,
                                              is_upper_fv_l ? (area_idx_l - 1) : (area_idx_l),
                                              is_upper_fv_r ? (area_idx_r - 1) : (area_idx_r))
                        )

  PRINT_EXCECUTION_TIME("refine",
                        // refine left part
                        for (int i = 0; i < THREAD_NUM; i++)
                          threads[i] = std::thread(refine_positions_mt, result, area_l, start_blk_idx_l, end_blk_idx_l, i);
                        for (int i = 0; i < THREAD_NUM; i++) threads[i].join();

                        /* if (is_upper_fv_l)
                          refine_positions(result, area_l->blocks[block_idx_l]->pos.get(), pos_idx_l);
                        else
                          refine_positions(result, area_l->blocks[block_idx_l]->pos.get() + pos_idx_l, area_l->blocks[block_idx_l]->length - pos_idx_l);
                         */
                        
                        if (is_upper_fv_l) {
                          new_refine_positions(result, area_l->blocks[block_idx_l]->find_refine_array(compare1));
                        } else {
                          new_refine_positions(result, area_l->blocks[block_idx_l]->find_not_refine_array(compare1));
                        }


                        // refine right part
                        for (int i = 0; i < THREAD_NUM; i++)
                          threads[i] = std::thread(refine_positions_mt, result, area_r, start_blk_idx_r, end_blk_idx_r, i);
                        for (int i = 0; i < THREAD_NUM; i++) threads[i].join();

                        /* if (is_upper_fv_r)
                          refine_positions(result, area_r->blocks[block_idx_r]->pos.get(), pos_idx_r);
                        else
                          refine_positions(result, area_r->blocks[block_idx_r]->pos.get() + pos_idx_r, area_r->blocks[block_idx_r]->length - pos_idx_r);
                         */

                        if (is_upper_fv_r) {
                          new_refine_positions(result, area_r->blocks[block_idx_r]->find_refine_array(compare2));
                        } else {
                          new_refine_positions(result, area_r->blocks[block_idx_r]->find_not_refine_array(compare2));
                        }
                        )
  // clang-format on
}

void bindex_scan_eq(BinDex *bindex, BITS *result, CODE compare) {
  int bitmap_len = bits_num_needed(bindex->length);
  std::thread threads[THREAD_NUM];

  int area_idx = bindex->in_which_area(compare);
  assert(area_idx >= 0 && area_idx <= K - 1);

  if (area_idx != K - 1 &&
      bindex->areas[area_idx + 1]->area_start_value() == compare) {
    // nm > N / K
    // result1 = hydex_scan_lt(compare)
    // result2 = hydex_scan_lt(compare + 1)
    // result = result1 ^ result2
    // TODO: (compare1 + 1) overflow
    std::thread threads[THREAD_NUM];
    int bitmap_len = bits_num_needed(bindex->length);

    // compare
    int area_idx = bindex->in_which_area(compare);
    if (area_idx < 0) {
      bindex_scan_lt(bindex, result2, compare + 1);
      return;
    }
    Area *area = bindex->areas[area_idx].get();
    int block_idx = area->in_which_block(compare);
    // int pos_idx = area->blocks[block_idx]->on_which_pos(compare);
    // Do an estimation to select the filter vector which is most
    // similar to the correct result
    int is_upper_fv;
    int start_blk_idx, end_blk_idx;
    if (block_idx < bindex->areas[area_idx]->blockNum / 2) {
      is_upper_fv = 1;
      start_blk_idx = 0;
      end_blk_idx = block_idx;
    } else {
      is_upper_fv = 0;
      start_blk_idx = block_idx + 1;
      end_blk_idx = area->blockNum;
    }

    // compare + 1
    CODE compare1 = compare + 1;
    int area_idx1 = bindex->in_which_area(compare1);
    if (area_idx1 < 0) {
      assert(0);
      return;
    }
    Area *area1 = bindex->areas[area_idx1].get();
    int block_idx1 = area1->in_which_block(compare1);
    // int pos_idx1 = area1->blocks[block_idx1]->on_which_pos(compare1);
    // Do an estimation to select the filter vector which is most
    // similar to the correct result
    int is_upper_fv1;
    int start_blk_idx1, end_blk_idx1;
    if (area_idx1 == 0 || (block_idx1 < bindex->areas[area_idx1]->blockNum / 2 &&
                           bindex->areas[area_idx1 - 1]->area_start_value() !=
                               area1->area_start_value())) {
      is_upper_fv1 = 1;
      start_blk_idx1 = 0;
      end_blk_idx1 = block_idx1;
    } else {
      is_upper_fv1 = 0;
      start_blk_idx1 = block_idx1 + 1;
      end_blk_idx1 = area1->blockNum;
    }

    // clang-format off
    PRINT_EXCECUTION_TIME("copy",
                        copy_filter_vector_xor(bindex,
                                              result,
                                              is_upper_fv ? (area_idx - 1) : (area_idx),
                                              is_upper_fv1 ? (area_idx1 - 1) : (area_idx1))
                        )

    PRINT_EXCECUTION_TIME("refine",
                        // refine left part
                        for (int i = 0; i < THREAD_NUM; i++)
                          threads[i] = std::thread(refine_positions_in_blks_mt, result, area, start_blk_idx, end_blk_idx, i);
                        for (int i = 0; i < THREAD_NUM; i++) threads[i].join();

                        /* if (is_upper_fv)
                          refine_positions(result, area->blocks[block_idx]->pos.get(), pos_idx);
                        else
                          refine_positions(result, area->blocks[block_idx]->pos.get() + pos_idx, area->blocks[block_idx]->length - pos_idx);
 */
                        if (is_upper_fv) {
                          new_refine_positions(result, area->blocks[block_idx]->find_refine_array(compare));
                        } else {
                          new_refine_positions(result, area->blocks[block_idx]->find_not_refine_array(compare));
                        }
                        // refine right part
                        for (int i = 0; i < THREAD_NUM; i++)
                          threads[i] = std::thread(refine_positions_in_blks_mt, result, area1, start_blk_idx1, end_blk_idx1, i);
                        for (int i = 0; i < THREAD_NUM; i++) threads[i].join();

                        /* if (is_upper_fv1)
                          refine_positions(result, area1->blocks[block_idx1]->pos.get(), pos_idx1);
                        else
                          refine_positions(result, area1->blocks[block_idx1]->pos.get() + pos_idx1, area1->blocks[block_idx1]->length - pos_idx1);
 */
                        if (is_upper_fv1) {
                          new_refine_positions(result, area1->blocks[block_idx1]->find_refine_array(compare1));
                        } else {
                          new_refine_positions(result, area1->blocks[block_idx1]->find_not_refine_array(compare1));
                        }

                        )
    // clang-format on
  } else {
    // nm < N / K
    memset_mt(result, 0, bitmap_len);

    Area *area = bindex->areas[area_idx].get();
    int block_idx = area->in_which_block(compare);
    // int pos_idx = on_which_pos(area->blocks[block_idx], compare);

    for (int i = 0; i < THREAD_NUM; i++) {
      threads[i] = std::thread(set_eq_bitmap_mt, result, area, compare,
                               block_idx, area->blockNum, i);
    }

    for (int i = 0; i < THREAD_NUM; i++) {
      threads[i].join();
    }
  }
}

void check_worker(CODE *codes, int n, BITS *bitmap, CODE target1, CODE target2, OPERATOR OP, int t_id) {
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(t_id * 2, &mask);
  if (pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0) {
    fprintf(stderr, "set thread affinity failed\n");
  }
  int avg_workload = n / THREAD_NUM;
  int start = t_id * avg_workload;
  int end = t_id == (THREAD_NUM - 1) ? n : start + avg_workload;
  for (int i = start; i < end; i++) {
    int data = codes[i];
    int truth;
    switch (OP) {
      case EQ:
        truth = data == target1;
        break;
      case LT:
        truth = data < target1;
        break;
      case GT:
        truth = data > target1;
        break;
      case LE:
        truth = data <= target1;
        break;
      case GE:
        truth = data >= target1;
        break;
      case BT:
        truth = data > target1 && data < target2;
        break;
      default:
        assert(0);
    }
    int res = !!(bitmap[i >> BITSSHIFT] & (1U << (BITSWIDTH - 1 - i % BITSWIDTH)));
    if (truth != res) {
      std::cerr << "raw data[" << i << "]: " << codes[i] << ", truth: " << truth << ", res: " << res << std::endl;
      assert(truth == res);
    }
  }
}

void check(BinDex *bindex, BITS *bitmap, CODE target1, CODE target2, OPERATOR OP) {
  std::cout << "checking, target1: " << target1 << " target2: " << target2 << std::endl;
  std::thread threads[THREAD_NUM];
  for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
    threads[t_id] = std::thread(check_worker, bindex->raw_data.get(), bindex->length, bitmap, target1, target2, OP, t_id);
  }
  for (int t_id = 0; t_id < THREAD_NUM; t_id++) {
    threads[t_id].join();
  }
  std::cout << "CHECK PASSED!" << std::endl;
}

void check_st(BinDex *bindex, BITS *bitmap, CODE target1, CODE target2, OPERATOR OP) {
  printf("checking, target1: %u, target2: %u, OP: %d\n", target1, target2, OP);
  assert(OP != BT || target1 <= target2);
  for (int i = 0; i < bindex->length; i++) {
    int data = bindex->raw_data[i];
    int truth;
    switch (OP) {
      case EQ:
        truth = data == target1;
        break;
      case LT:
        truth = data < target1;
        break;
      case GT:
        truth = data > target1;
        break;
      case LE:
        truth = data <= target1;
        break;
      case GE:
        truth = data >= target1;
        break;
      case BT:
        truth = data > target1 && data < target2;
        break;
      default:
        assert(0);
    }
    int res = !!(bitmap[i >> BITSSHIFT] & (1U << (BITSWIDTH - 1 - i % BITSWIDTH)));
    if (truth != res) {
      fprintf(stderr, "bindex->raw_data[%d]: %d, truth: %d, res: %d\n", i, bindex->raw_data[i], truth, res);
      assert(truth == res);
      exit(-1);
    }
  }
  printf("CHECK PASS\n");
}