#include "bindex.h"

void pos_block::refine_start_value()
{

    CODE min_value = 1 << BITSWIDTH - 1;
    int position = 0;

    for (int i = 0; i < blockMaxSize; i++)
    {
        if (val[i] <= 0)
            continue;
        if (val[i] < min_value)
        {
            min_value = val[i];
            position = i;
        }
    }

    start_value = min_value;
}

// modify block pos and val
// new_pos_f = -1 if we want to remove this element
// in this case, we move deleted space to out free list
// split this function with a find_element part
// TODO: this is a temp function since we need to reconsider the position
// of the element in the whole bindex
POSTYPE pos_block::modify_block(CODE val_f, POSTYPE new_pos_f, CODE new_val_f)
{
    int temp_val;
    int temp_pos;
    for (int i = 0; i < blockMaxSize; i++)
    {
        if (val[i] == val_f)
        {
            if (new_pos_f < 0)
            {
                temp_val = val[i];
                temp_pos = pos[i];
                pos[i] = -1;
                val[i] = -1;   // TODO: error here, uint32_t can't be -1, fix it later
                insert_freepos_to_free_list(i);
            }
            else
            {
                temp_val = val[i];
                temp_pos = pos[i];
                pos[i] = new_pos_f;
                val[i] = new_val_f;
            }
            if (start_value == temp_val)
                refine_start_value();
            return temp_pos;
        }
    }
    return -1;
}

POSTYPE *Area::remove_from_area(CODE *data_to_remove, POSTYPE n)
{
    POSTYPE *recycle_positions = (POSTYPE *)malloc(sizeof(POSTYPE) * n);
    for (int i = 0; i < n; i++){
        recycle_positions[i] = -1;
    }
    int i, j;
    for (i = 0, j = 0; i < n && j < blockNum - 1;)
    {
        int num_insert_to_block = 0;
        int start = i;
        while (data_to_remove[i] < blocks[j + 1]->block_start_value() && i < n)
        {
            i++;
            num_insert_to_block++;
        }
        if (num_insert_to_block)
        {
            for (int k = 0; k < num_insert_to_block; k++)
            {
                recycle_positions[k + start] = blocks[j]->modify_block(data_to_remove[k + start], -1, -1);
            }
        }
        j++;
    }

    if (i < n)
    { // Insert val[i] to the end of last block if val[i] is no less
        // than the maximum value of current area
        int num_insert_to_block = n - i;
        int start = i;
        for (int k = 0; k < num_insert_to_block; k++)
        {
            recycle_positions[k + start] = blocks[j]->modify_block(data_to_remove[k + start], -1, -1);
        }
    }
    length = length - n;

    return recycle_positions;
}

void BinDex::reverse_filter_vector(int vector_idx, POSTYPE *positions, POSTYPE n)
{
    for (int i = 0; i < n; i++)
    {
        if(positions[i] != -1)
            reverse_bit(vector_idx, positions[i]);
    }
}

void BinDex::reverse_bit(int vector_idx, POSTYPE position)
{
    CODE tool = ((1UL << BITSWIDTH) - 1) - (1 << (BITSWIDTH - position - 1)) ;
    filterVectors[vector_idx][position / BITSWIDTH] =
        filterVectors[vector_idx][position / BITSWIDTH] & tool;
}

void BinDex::remove_from_bindex(CODE *data_to_remove, POSTYPE n)
{
    POSTYPE *idx = argsort(data_to_remove, n);
    CODE *data_sorted = (CODE *)malloc(n * sizeof(CODE));
    POSTYPE *recycle_positions = (POSTYPE *)malloc(sizeof(POSTYPE) * n);

    for (int i = 0; i < n; i++){
        recycle_positions[i] = -1;
    }

    POSTYPE areaStartIdx[K];
    areaStartIdx[0] = 0;
    int k = 1;
    for (POSTYPE i = 0; i < n; i++)
    {
        data_sorted[i] = data_to_remove[idx[i]];
        while (k < K && data_sorted[i] >= areas[k]->area_start_value())
        {
            areaStartIdx[k++] = i;
        }
    }
    while (k < K)
    {
        areaStartIdx[k++] = n;
    }
    std::thread threads[THREAD_NUM];
    int i, j;
    // Update the areas
    int accum_add_count = 0;
    for (k = 0; k * THREAD_NUM < K; k++)
    {
        for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < K; j++)
        {
            i = k * THREAD_NUM + j;
            int num_added = num_insert_to_area(areaStartIdx, i, n);
            POSTYPE *area_recycle_positions =
                areas[i]->remove_from_area(data_sorted + areaStartIdx[i], num_added);
           
            memcpy(recycle_positions + accum_add_count, area_recycle_positions, num_added * sizeof(CODE));
            free(area_recycle_positions);
            accum_add_count += num_added;
            area_counts[i] += accum_add_count;
        }
    }


    // for (k = 0; k < n; k++)
    // {
    //     if(recycle_positions[k] != -1)
    //         cout << raw_data[recycle_positions[k]] << " ";
    //     else
    //         cout << "NF ";
    // }
    // cout << endl;


    // delete from raw_data
    for (k = 0; k < n; k++)
    {
        if(recycle_positions[k] != -1)
            raw_data[recycle_positions[k]] = 0;
    }

    // delete from  the filter vectors
    for (k = 0; k * THREAD_NUM < (K - 1); k++)
    {
        for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++)
        {
            i = k * THREAD_NUM + j;
            threads[j] = std::thread(&BinDex::reverse_filter_vector, this, i, recycle_positions, n);
        }
        for (j = 0; j < THREAD_NUM && (k * THREAD_NUM + j) < (K - 1); j++)
        {
            threads[j].join();
        }
    }

    length -= n;
    // display_bindex();

    cout << "[+]Remove done." << endl;

    free(idx);
    free(recycle_positions);
}