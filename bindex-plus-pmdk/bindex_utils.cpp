#include "bindex.h"

extern Timer timer;
extern int DEBUG_TIME_COUNT;

CODE pos_block::block_start_value() { return start_value; }
// CODE pos_block::block_start_value() { return val[0]; };

CODE Area::area_start_value() { return blocks[0]->start_value; }

void pos_block::display_block()
{
    cout << "Block info:" << endl;
    printf("Virtual space values:\t");
    for (int i = 0; i < length; i++)
    {
        printf("%d,", val[i]);
    }
    cout << endl;

    cout << "bitmap:" << endl;
    display_bitmap(bitmap_pos.get(), bits_num_needed(blockMaxSize));
    cout << endl;
    /* printf("\nPositions:\t\t");
  for (int i = 0; i < length; i++) {
    printf("%d,", pos[i]);
  } */
}

void Area::display_area()
{
    cout << "[+]Area " << idx << " info:" << endl;
    for (int i = 0; i < blockNum; i++)
    {
        cout << "Block " << i << ": " << endl;
        cout << "Size: " << blocks[i]->length << endl;
        cout << "Start Value: " << blocks[i]->block_start_value() << endl;
    }
    cout << endl;
    
    /* printf("Virtual space values:\t\n");
    for (int i = 0; i < blockNum; i++)
    {
        printf("block%d--", i);
        for (int j = 0; j < blocks[i]->length; j++)
        {
            printf("%d,", blocks[i]->val[j]);
        }
        printf("\t");
        printf("\n");
    } */
    /* printf("\nPositions:\t\t\n");
  for (int i = 0; i < blockNum; i++) {
    printf("block%d--", i);
    for (int j = 0; j < blocks[i]->length; j++) {
      printf("%d,", blocks[i]->pos[j]);
    }
    printf("\t");
    printf("\n");
  } */
}

void Area::show_volume()
{
    cout << "[+] Area " << idx << " volume:" << endl;
    for (int i = 0; i < blockNum; i++)
    {
        cout << "Block " << i << ": " << "start value: " << blocks[i]->block_start_value() << " Size: " << blocks[i]->length << endl;
    }
}

void BinDex::show_volume()
{
    cout << "[+]BinDex Volume" << endl;
    for (int i = 0; i < K; i++)
    {
        areas[i]->show_volume();
    }
}

void BinDex::display_bindex()
{
    for (int i = 0; i < K; i++)
    {
        printf("Area%d:\n", i);
        areas[i]->display_area();
        printf("\t\n");
    }
    /* printf("Length:%d\t raw_data:", length);
  for (int i = 0; i < length; i++) {
    printf("%d,", raw_data[i]);
    if ((i + 1) % 4 == 0) printf("|");
    if ((i + 1) % 32 == 0) printf("--------");
  }
  printf("\t\n==\n");
  for (int i = 0; i < K - 1; i++) {
    printf("filterVector%d:\n", i);
    display_bitmap(filterVectors[i].get(), bits_num_needed(length));
    printf("\t\n");
  } */
}

char *bin_repr(BITS x)
{
    // Generate binary representation of a BITS variable
    int len = BITSWIDTH + BITSWIDTH / 4 + 1;
    char *result = (char *)malloc(sizeof(char) * len);
    BITS ref;
    int j = 0;
    for (int i = 0; i < BITSWIDTH; i++)
    {
        ref = 1 << (BITSWIDTH - i - 1);
        result[i + j] = (ref & x) ? '1' : '0';
        if ((i + 1) % 4 == 0)
        {
            j++;
            result[i + j] = '-';
        }
    }
    result[len - 1] = '\0';
    return result;
}

void display_bitmap(BITS *bitmap, int bitmap_len)
{
    for (int i = 0; i < bitmap_len; i++)
    {
        printf("%s;", bin_repr(*(bitmap + i)));
    }
}

template <typename T>
POSTYPE *argsort(const T *v, POSTYPE n)
{
    POSTYPE *idx = (POSTYPE *)malloc(n * sizeof(POSTYPE));
    for (POSTYPE i = 0; i < n; i++)
    {
        idx[i] = i;
    }
    __gnu_parallel::sort(idx, idx + n, [&v](size_t i1, size_t i2)
                         { return v[i1] < v[i2]; });
    return idx;
}

template POSTYPE *argsort<int>(const int*, POSTYPE);
template POSTYPE *argsort<unsigned int>(const unsigned int*, POSTYPE);

BITS gen_less_bits(const CODE *val, CODE compare, int n) {
  // n must <= BITSWIDTH (32)
  BITS result = 0;
  for (int i = 0; i < n; i++) {
    if (val[i] < compare) {
      result += (1 << (BITSWIDTH - 1 - i));
    }
  }
  return result;
}

void set_fv_val_less(BITS *bitmap, const CODE *val, CODE compare, POSTYPE n)
{
    // Set values for filter vectors
    int i;
    for (i = 0; i + BITSWIDTH < (int)n; i += BITSWIDTH)
    {
        bitmap[i / BITSWIDTH] = gen_less_bits(val + i, compare, BITSWIDTH);
    }
    bitmap[i / BITSWIDTH] = gen_less_bits(val + i, compare, n - i);
}

int padding_fv_val_less(BITS *bitmap, POSTYPE length_old, const CODE *val_new, CODE compare, POSTYPE n)
{
    // Padding new bit results to old bitmap to fill up a BITS variable, return
    // the number of bits needed for fill up a BITS variable

    if (length_old % BITSWIDTH == 0)
        return 0; // No padding is needed for fill up a BITS

    int result_bits_count;
    int bitmap_insert_pos = length_old / BITSWIDTH;
    int padding = (bitmap_insert_pos + 1) * BITSWIDTH - length_old;
    result_bits_count = padding;
    if (padding > (int)n)
    {
        result_bits_count = n; // Too little new data, still not filled up,
                               // return the number of actually appended bits
    }
    for (int i = 0; i < padding; i++)
    {
        if (val_new[i] < compare)
        {
            bitmap[bitmap_insert_pos] += (1 << (padding - i - 1));
        }
    }
    return result_bits_count;
}

void append_fv_val_less(BITS *bitmap, POSTYPE length_old, const CODE *val, CODE compare, POSTYPE n)
{
    // double timeCount = 0.0;
    // timeCount = timer.quickGetStartTime(timeCount);
    int padding = padding_fv_val_less(bitmap, length_old, val, compare, n);
    int bitmap_insert_pos = (length_old - 1) / BITSWIDTH + 1;
    set_fv_val_less(bitmap + bitmap_insert_pos, val + padding, compare, n - padding);
    // timeCount = timer.quickGetEndTime(timeCount);
    // printf("append fv time: %f\n",timeCount);
}


uint32_t str2uint32(const char *s) {
  uint32_t sum = 0;

  while (*s) {
    sum = sum * 10 + (*s++ - '0');
  }

  return sum;
}

std::vector<CODE> get_target_numbers(const char *s) {
  std::string input(s);
  std::stringstream ss(input);
  std::string value;
  std::vector<CODE> result;
  while (std::getline(ss, value, ',')) {
    result.push_back((CODE)stod(value));
    // result.push_back(str2uint32(value.c_str()));
  }
  return result;
}

bool isFileExists_stat(char *path) {
  struct stat buffer;   
  return (stat(path, &buffer) == 0); 
}

CODE *generate_single_block_data(CODE startValue, CODE endValue, int datalen)
{
  CODE *block_data = (CODE *)malloc(datalen * sizeof(CODE));
  std::random_device rd;
  std::mt19937 mt(rd());
  std::uniform_int_distribution<CODE> dist(startValue, endValue);
  CODE mask = ((uint64_t)1 << (sizeof(CODE) * 8)) - 1;
  for (int i = 0; i < datalen; i++)
  {
    block_data[i] = dist(mt) & mask;
    assert(block_data[i] <= mask);
  }
  return block_data;
}

// audit functions

void pos_block::check_pos_block()
{
  CODE startval = val[0];
  for (int i = 0; i < length; i++){
    if (val[i] < startval){
      cout << "[ERROR] Block" << idx << " Element " << val[i] << endl;
      display_block();
      printf("display");
      exit(0);
    }
  }
}

void Area::check_area() 
{
  // show_volume();
  for(int i = 0; i < blockNum; i++){
    blocks[i]->check_pos_block();
    if (i + 1 < blockNum) {
      for (int j = 0; j < blocks[i]->length; j++) {
        if (blocks[i]->val[j] > blocks[i + 1]->val[0]) {
          cout << "[ERROR] Area" << idx << " Block" << blocks[i]->idx << " Element " << blocks[i]->val[i] << endl;
          display_area();
          exit(0);
        }
      }
    }
  }
}

void BinDex::check_bindex() 
{
  cout << endl;
  cout << "[+] Check Bindex" << endl;

  // check if all elements are in the right area and block
  // and print basic information
  for(int i = 0; i < VAREA_N; i++){
    areas[i]->check_area();
  }

  cout << "[+] Check finished." << endl;
}