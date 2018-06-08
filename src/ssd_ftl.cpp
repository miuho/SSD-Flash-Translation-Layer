/* Copyright 2009, 2010 Brendan Tauras */

/* ssd_ftl.cpp is part of FlashSim. */

/* FlashSim is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version. */

/* FlashSim is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details. */

/* You should have received a copy of the GNU General Public License
 * along with FlashSim.  If not, see <http://www.gnu.org/licenses/>. */

/****************************************************************************/

/* Ftl class
 * Brendan Tauras 2009-11-04
 *
 * This class is a stub class for the user to use as a template for implementing
 * his/her FTL scheme.  A few functions to gather information from lower-level
 * hardware are added to assist writing a FTL scheme.  The Ftl class should
 * rely on the Garbage_collector and Wear_leveler classes for modularity and
 * simplicity. */

/*
 * @ssd_ftl.cpp
 * 
 * HingOn Miu (hmiu)
 * Carnegie Mellon University
 * 2015-10-15
 */


#include <new>
#include <assert.h>
#include <stdio.h>
#include "ssd.h"
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

using namespace ssd;

// total number of pages in raw capacity
#define RAW_SIZE (SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE)
// total number of physical blocks
#define NUM_OF_PHY_B (RAW_SIZE / BLOCK_SIZE)
// total number of pages in overprovisioning
#define OP_SIZE ((RAW_SIZE *  OVERPROVISIONING) / 100)
// total number of blocks in overprovisioning
#define NUM_OF_OP_B (OP_SIZE / BLOCK_SIZE)
// total number of pages in usable capacity
#define USABLE_SIZE (RAW_SIZE - OP_SIZE)
// total number of logical blocks
#define NUM_OF_LGC_B ((unsigned int)(USABLE_SIZE / BLOCK_SIZE))
// total number of physical data blocks
#define NUM_OF_DATA_B (NUM_OF_LGC_B)

// track the emptiness of each logical page
unsigned int *logical_to_emptiness;
// track the number of erases for each log block
unsigned int *erase_count;
// track the mapping of logical blocks to physical blocks
int *logical_to_physical;
// track the mapping of physical data blocks to physical log blocks
int *data_to_log;
// track the pages in each physical log block
std::unordered_map <unsigned long, std::string> log_to_pages;
// store the start time of input event
double start_time;
// store the over-provisioning blocks
std::vector<unsigned long> op_blocks;
// record the current cleaning block
unsigned long current_cln_address;

/**
 * @brief Checks if the input logial address has been written
 */
bool check_page_empty(unsigned long lba) {
  // fetch the corresponding unsigned int
  unsigned int flag = logical_to_emptiness[lba / sizeof (unsigned int)];
  // check the corresponding bit
  return (((flag >> (lba % sizeof(unsigned int))) & 1) == 0);
}

/**
 * @brief Flag the input logical address as written
 */
void set_page_written(unsigned long lba) {
  // fetch the corresponding unsigned int and set corresponding bit
  logical_to_emptiness[lba / sizeof (unsigned int)] |= (1 << (lba % sizeof(unsigned int)));
}

unsigned long check_physical_address(unsigned long logical_address) {
  unsigned page = logical_address % BLOCK_SIZE;
  int nth_logical_block = (int)(logical_address / BLOCK_SIZE);
  int offset = logical_to_physical[nth_logical_block];
  int nth_physical_block = nth_logical_block + offset;
  return page + ((unsigned long)nth_physical_block) * BLOCK_SIZE;
}

void set_physical_address(unsigned long logical_address, unsigned long physical_address) {
  int nth_logical_block = (int)(logical_address / BLOCK_SIZE);
  int nth_physical_block = (int)(physical_address / BLOCK_SIZE);
  logical_to_physical[nth_logical_block] = (nth_physical_block - nth_logical_block);
}

bool check_log_block(unsigned long data_address, unsigned long *log_address) {
  unsigned page = data_address % BLOCK_SIZE;
  int nth_data_block = (int)(data_address / BLOCK_SIZE);
  int offset = data_to_log[nth_data_block];
  if (offset == 0) // data block to log block mapping cannot be 0
    return false;
  int nth_log_block = nth_data_block + offset;
  *log_address = page + ((unsigned long)nth_log_block) * BLOCK_SIZE;
  return true;
}

void set_log_block(unsigned long data_address, unsigned long log_address) {
  int nth_data_block = (int)(data_address / BLOCK_SIZE);
  int nth_log_block = (int)(log_address / BLOCK_SIZE);
  data_to_log[nth_data_block] = (nth_log_block - nth_data_block);
}

void cancel_log_block(unsigned long data_address) {
  unsigned long log_address;
  if (check_log_block(data_address, &log_address)) {
    if (log_to_pages.find(log_address) != log_to_pages.end()) {
      log_to_pages.erase(log_address);
    }
  }
  // this will clear the offset to 0
  set_log_block(data_address, data_address);
}

/**
 * @brief Fetch the most recent copy of the page in log block
 *        corresponding to the page in data block
 */
bool fetch_log_page(std::string offsets, 
                    unsigned int data_page, 
                    unsigned int *log_page) {
  if (offsets == "") {
    return false;
  }
  // find the position of the last occurance of data_page
  // check locations beside the first page
  size_t loc = offsets.rfind("," + std::to_string((long long)data_page) + ",");
  // found nothing in locations beside the first page
  if (loc == std::string::npos) {
    // check the first page of overprovision block
    loc = offsets.find(std::to_string((long long)data_page) + ",");
    // found in the first page
    if (loc == 0) {
      *log_page = 0;
      return true;
    }
    // found nothing in first page and rest of pages
    return false;
  }
  // found the page (not the first page)
  // count the index of this page
  unsigned int count = 0;
  size_t index = offsets.find(",");
  while (index != loc) {
    count++;
    index++;
    index = offsets.find(",", index);
  }

  *log_page = count + 1;
  return true;
}

/**
 * @brief Find the next free page in log block
 */
bool next_free_log_page(std::string offsets, unsigned int *page) {
  unsigned int count = 0; 
  size_t loc = offsets.find(",");
  while (loc != std::string::npos) {
    count++;
    loc++;
    loc = offsets.find(",", loc);
  }

  if (count >= BLOCK_SIZE) {
    // no more empty pages in log block
    return false;
  }
  
  *page = count;
  //fprintf(log_file, 
  return true;
}

void Ftl::print_info(void) {
  int count = 0;
  for (unsigned int i = 0; i < NUM_OF_LGC_B; i++) {
    //unsigned long data_address = check_physical_address(i * BLOCK_SIZE);
    bool non_empty = false;
    for (unsigned int j = 0; j < BLOCK_SIZE; j++) {
      if (!check_page_empty(i * BLOCK_SIZE + j)) {
        non_empty = true;
        break;
      }
    }
    if (non_empty == false) {
      count++;
    }
  }
  fprintf(log_file, "%d empty data blocks\n", count);
  for (unsigned int i = 0; i <= BLOCK_ERASES; i++) {
    int sum = 0;
    for (unsigned int logical_b = 0; logical_b < NUM_OF_LGC_B; logical_b++) {
      unsigned long data_address = check_physical_address(logical_b * BLOCK_SIZE);
      if (erase_count[data_address / BLOCK_SIZE] == i) {
        sum++;
      }
    }
    fprintf(log_file, "%d data blocks have %u erases\n", sum, i);
  }
  fprintf(log_file, "total # of op blocks %d\n", (int)NUM_OF_OP_B);
  fprintf(log_file, "free op blocks left %d\n", op_blocks.size());
  int big_sum = 0;
  for (unsigned int i = 0; i <= BLOCK_ERASES; i++) {
    int sum = 0;
    for (unsigned int logical_b = 0; logical_b < NUM_OF_LGC_B; logical_b++) {
      unsigned long data_address = check_physical_address(logical_b * BLOCK_SIZE);
      unsigned long log_address;
      if (check_log_block(data_address, &log_address)) {
        if (erase_count[log_address / BLOCK_SIZE] == i) {
          sum++;
        }
      }
    }
    fprintf(log_file, "%d log blocks have %u erases\n", sum, i);
    big_sum += sum;
  }
  if ((unsigned int)big_sum != log_to_pages.size())
    fprintf(log_file, "wrong\n");
  fprintf(log_file, "log blocks used %d\n", big_sum);
}

/**
 * @brief Check if the block exceeds erase limit
 */
bool over_erase_limit(unsigned long physical_address) {
  return (erase_count[physical_address / BLOCK_SIZE] >= BLOCK_ERASES);
}

/**
 * @brief Update the erase count for a block
 */
void update_erase_count(unsigned long physical_address) {
  (erase_count[physical_address / BLOCK_SIZE]) += 1;
}

bool find_empty_data_block_for_cleaning(unsigned long *empty_data_address) {
  unsigned int min_count = BLOCK_ERASES + 1;
  for (unsigned int i = 0; i < NUM_OF_LGC_B; i++) {
    unsigned long data_address = check_physical_address(i * BLOCK_SIZE);
    bool non_empty = false;
    for (unsigned int j = 0; j < BLOCK_SIZE; j++) {
      if (!check_page_empty(i * BLOCK_SIZE + j)) {
        non_empty = true;
        break;
      }
    }
    unsigned int count = erase_count[data_address / BLOCK_SIZE];
    if (non_empty == false && count < min_count && count < BLOCK_ERASES) {
      min_count = count;
      *empty_data_address = data_address;
    }
  }
  return (min_count != BLOCK_ERASES + 1);
}

bool find_empty_data_block_for_remapping(unsigned long *empty_data_address,
                                         unsigned long *empty_logical_block) {
  unsigned int min_count = BLOCK_ERASES + 1;
  for (unsigned int i = 0; i < NUM_OF_LGC_B; i++) {
    unsigned long data_address = check_physical_address(i * BLOCK_SIZE);
    bool non_empty = false;
    for (unsigned int j = 0; j < BLOCK_SIZE; j++) {
      if (!check_page_empty(i * BLOCK_SIZE + j)) {
        non_empty = true;
        break;
      }
    }
    unsigned int count = erase_count[data_address / BLOCK_SIZE];
    if (non_empty == false && count < min_count && count < BLOCK_ERASES) {
      min_count = count;
      *empty_logical_block = i * BLOCK_SIZE;
      *empty_data_address = data_address;
    }
  }
  return (min_count != BLOCK_ERASES + 1);
}

/**
 * @brief Returns the numerical mapping from physical address to SSD address
 */
void map_physical_to_SSD(unsigned long phy,
                         unsigned int *package, unsigned int *die, 
                         unsigned int *plane, unsigned int *block, unsigned int *page) {
  *package = ((((phy / BLOCK_SIZE) / PLANE_SIZE ) / DIE_SIZE) / PACKAGE_SIZE) % SSD_SIZE;
  *die = (((phy / BLOCK_SIZE) / PLANE_SIZE ) / DIE_SIZE) % PACKAGE_SIZE;
  *plane = ((phy / BLOCK_SIZE) / PLANE_SIZE ) % DIE_SIZE;
  *block = (phy / BLOCK_SIZE) % PLANE_SIZE;
  *page = phy % BLOCK_SIZE;
}

bool Garbage_collector::shuffle_data_log(void) {
  // find a log/data block pair with at most BLOCK_ERASES - 1 erases
  unsigned int max_count = 0;
  unsigned long max_erase_log = RAW_SIZE;
  unsigned long max_erase_data = RAW_SIZE;
  for (unsigned int i = 0; i < NUM_OF_PHY_B; i++) {
    unsigned long log_address;
    unsigned long data_address = i * BLOCK_SIZE;
    if (check_log_block(data_address, &log_address)) {
      unsigned int log_count = erase_count[log_address / BLOCK_SIZE];
      unsigned int data_count = erase_count[data_address / BLOCK_SIZE];
      if (log_count != BLOCK_ERASES &&
          data_count != BLOCK_ERASES &&
          (log_count + data_count) >= max_count) {
        max_erase_log = log_address;
        max_erase_data = data_address;
        max_count = (log_count + data_count);
      }
    }
  }
  if (max_erase_data == RAW_SIZE || max_erase_log == RAW_SIZE) return false;
  // find the corresponding logical block
  unsigned long logical_block = RAW_SIZE;
  for (unsigned int i = 0; i < NUM_OF_LGC_B; i++) {
    unsigned long logical_address = i * BLOCK_SIZE;
    unsigned long to_match = check_physical_address(logical_address);
    if (to_match == max_erase_data) {
      logical_block = logical_address;
      break;
    }
  }
  if (logical_block == RAW_SIZE) return false;
  
  // find a data block (unmapped to log block) with the fewest erases
  unsigned int min_count = BLOCK_ERASES + 1;
  unsigned long min_erase_data = RAW_SIZE;
  for (unsigned int i = 0; i < NUM_OF_LGC_B; i++) {
    unsigned long logical_address = i * BLOCK_SIZE;
    unsigned long data_address = check_physical_address(logical_address);
    unsigned long dummy;
    // make sure the data block has no log block mapped
    if (!check_log_block(data_address, &dummy)) {
      unsigned int count = erase_count[data_address / BLOCK_SIZE];
      if (count < min_count) {
        min_erase_data = data_address;
        min_count = count;
      }
    }
  }
  if (min_erase_data == RAW_SIZE) return false;
  if (min_count >= BLOCK_ERASES - 1) return false;
  
  // free up the log block
  if (clean(logical_block, max_erase_data, max_erase_log) == false) return false;
  cancel_log_block(max_erase_data);
  
  // find the corresponding logical block
  logical_block = RAW_SIZE;
  for (unsigned int i = 0; i < NUM_OF_LGC_B; i++) {
    unsigned long logical_address = i * BLOCK_SIZE;
    unsigned long to_match = check_physical_address(logical_address);
    if (to_match == min_erase_data) {
      logical_block = logical_address;
      break;
    }
  }
  if (logical_block == RAW_SIZE) return false;
  
  unsigned int package;
  unsigned int die;
  unsigned int plane;
  unsigned int block;
  unsigned int dummy;
  // move pages from data block to log block
  for (unsigned int i = 0; i < BLOCK_SIZE; i++) {
    // move the page if only it is a written page
    if (!check_page_empty(logical_block + i)) {
      map_physical_to_SSD(min_erase_data, &package, &die, &plane, &block, &dummy);
      Address src_addr = Address(package, die, plane, block, i, PAGE);
      Event read_event = Event(READ, logical_block + i, 1, start_time);
      read_event.set_address(src_addr);
      ftl.controller.issue(read_event);
      map_physical_to_SSD(max_erase_log, &package, &die, &plane, &block, &dummy);
      Event write_event = Event(WRITE, logical_block + i, 1, start_time);
      Address des_addr = Address(package, die, plane, block, i, PAGE);
      write_event.set_address(des_addr);
      ftl.controller.issue(write_event);
    }
  }
  
  // erase data block
  map_physical_to_SSD(min_erase_data, &package, &die, &plane, &block, &dummy);
  Address data_addr = Address(package, die, plane, block, 0, BLOCK);
  Event erase_data_event = Event(ERASE, logical_block, 1, start_time);
  erase_data_event.set_address(data_addr);
  ftl.controller.issue(erase_data_event);
  
  // now the data block becomes new unmapped log block, and the log
  // block becomes the data block
  set_physical_address(logical_block, max_erase_log);
  op_blocks.push_back(min_erase_data);
  
  fprintf(log_file,
    "[shuffle_data_log] log block %lu <-> data block %lu\n", max_erase_log, min_erase_data);
  
  return true;
}

/**
 * @brief Find next unmapped log block
 */
bool Garbage_collector::next_unmapped_log_block(unsigned long *log_address,
                                  unsigned int *package, unsigned int *die, 
                                  unsigned int *plane, unsigned int *block) {
  if (op_blocks.empty()) {
    if (shuffle_data_log() == false)
      return false;
  }
  
  unsigned int i = 0;
  while (i < op_blocks.size()) {
    *log_address = op_blocks[op_blocks.size() - 1 - i];
    unsigned int dummy;
    map_physical_to_SSD(*log_address, package, die, plane, block, &dummy);
    op_blocks.pop_back();
    if (!over_erase_limit(*log_address))
      return true;
    i++;
  }
  return false;
}

/**
 * @brief Find the cleaning block to use
 */
bool next_unmapped_cln_block(unsigned long *cln_address) {
  return false;
  /*
  if (over_erase_limit(current_cln_address)) {
    if (op_blocks.empty()) {
      if (find_empty_data_block_for_cleaning(cln_address) == true)
        return true;
      return false;
    }
    *cln_address = op_blocks[op_blocks.size() - 1];
    current_cln_address = *cln_address;
    op_blocks.pop_back();
    return true;
  }
  else {
    *cln_address = current_cln_address;
    return true;
  }*/
}

unsigned long Garbage_collector::remap_data_block(unsigned long logical_block,
                                                  unsigned long old_data_pba,
                                                  unsigned long log_pba) {
  unsigned int package;
  unsigned int die;
  unsigned int plane;
  unsigned int block;
  unsigned int dummy;
  unsigned long new_data_pba;
  unsigned long new_logical_block = RAW_SIZE;
  
  // check if a block available
  if (find_empty_data_block_for_remapping(&new_data_pba, &new_logical_block) == false) {
    fprintf(log_file, "[remap_data_block] no empty data block left\n");
    if (next_unmapped_log_block(&new_data_pba, &package, &die, &plane, &block) == false) {
      fprintf(log_file, "[remap_data_block] no log block left\n");
      return log_pba;
    }
  }

  // copy pages from old data block to new data block
  std::unordered_map<unsigned long, std::string>::const_iterator log_pages =
    log_to_pages.find(log_pba);
  for (unsigned int i = 0; i < BLOCK_SIZE; i++) {
    // move the page if only it is a written page
    if (!check_page_empty(logical_block + i)) {
      // find if the latest copy
      unsigned int log_page;
      if (!fetch_log_page(log_pages->second, i, &log_page)) {
        // read from latest copy of page in data block
        map_physical_to_SSD(old_data_pba, &package, &die, &plane, &block, &dummy);
        Address src_addr = Address(package, die, plane, block, i, PAGE);
        Event read_event = Event(READ, logical_block + i, 1, start_time);
        read_event.set_address(src_addr);
        ftl.controller.issue(read_event);
        map_physical_to_SSD(new_data_pba, &package, &die, &plane, &block, &dummy);
        Event write_event = Event(WRITE, logical_block + i, 1, start_time);
        Address des_addr = Address(package, die, plane, block, i, PAGE);
        write_event.set_address(des_addr);
        ftl.controller.issue(write_event);
      }
    }
  }
  
  fprintf(log_file, "[remap_data_block] moved pages to new data block\n");
  
  if (new_logical_block != RAW_SIZE)
    set_physical_address(new_logical_block, old_data_pba);
  set_physical_address(logical_block, new_data_pba);
  set_log_block(old_data_pba, old_data_pba);
  set_log_block(new_data_pba, log_pba);
  
  return new_data_pba;
}

unsigned long Garbage_collector::remap_log_block(unsigned long logical_block,
                                                 unsigned long data_pba,
                                                 unsigned long old_log_pba) {
  unsigned int package;
  unsigned int die;
  unsigned int plane;
  unsigned int block;
  unsigned int dummy;
  unsigned long new_log_pba;
  
  // check if a unmapped log block available
  if (next_unmapped_log_block(&new_log_pba, &package, &die, &plane, &block) == false) {
    fprintf(log_file, "[remap_log_block] no log block left\n");
    return data_pba;
  }

  // copy pages from old log block to new log block
  std::unordered_map<unsigned long, std::string>::const_iterator log_pages =
    log_to_pages.find(old_log_pba);
  int j = 0;
  std::string offsets = "";
  for (unsigned int i = 0; i < BLOCK_SIZE; i++) {
    // move the page if only it is a written page
    if (!check_page_empty(logical_block + i)) {
      // find if the latest copy
      unsigned int log_page;
      if (fetch_log_page(log_pages->second, i, &log_page)) {
        // read from latest copy of page in log block
        map_physical_to_SSD(old_log_pba, &package, &die, &plane, &block, &dummy);
        Address src_addr = Address(package, die, plane, block, log_page, PAGE);
        Event read_event = Event(READ, logical_block + i, 1, start_time);
        read_event.set_address(src_addr);
        ftl.controller.issue(read_event);
        map_physical_to_SSD(new_log_pba, &package, &die, &plane, &block, &dummy);
        Event write_event = Event(WRITE, logical_block + i, 1, start_time);
        Address des_addr = Address(package, die, plane, block, j, PAGE);
        write_event.set_address(des_addr);
        ftl.controller.issue(write_event);
        j++;
        offsets += (std::to_string((long long)i) + ",");
      }
    }
  }
  
  fprintf(log_file, "[remap_log_block] moved pages to new log block\n");
  
  cancel_log_block(data_pba);
  set_log_block(data_pba, new_log_pba);
  log_to_pages[new_log_pba] = offsets;
  
  return new_log_pba;
}

/**
 * @brief Move data to cleaning black and move back
 */
bool Garbage_collector::clean(unsigned long logical_block,
                              unsigned long data_pba, unsigned long log_pba) {
  unsigned int package;
  unsigned int die;
  unsigned int plane;
  unsigned int block;
  unsigned int dummy;
  unsigned long cln_pba;
  
  // calculate log block 
  map_physical_to_SSD(log_pba, &package, &die, &plane, &block, &dummy);
  Address log_addr = Address(package, die, plane, block, 0, BLOCK);
 
  // calculate data block
  map_physical_to_SSD(data_pba, &package, &die, &plane, &block, &dummy);
  Address data_addr = Address(package, die, plane, block, 0, BLOCK);
  
  // check if a unmapped cleaning block available
  if (find_empty_data_block_for_cleaning(&cln_pba) == false) {
    fprintf(log_file, "[clean] no empty data block left\n");
    //if (next_unmapped_cln_block(&cln_pba) == false) {
      // no free cleaning block
    //  fprintf(log_file, "[clean] no cleaning block left\n");
      return false;
    //}
  }
  
  fprintf(log_file, "[clean] data block %lu, log block %lu\n", data_pba, log_pba);
  
  // calculate cleaning block 
  map_physical_to_SSD(cln_pba, &package, &die, &plane, &block, &dummy);
  Address cln_addr = Address(package, die, plane, block, 0, BLOCK);

  // copy live pages from data block and log block to cleaning block
  std::unordered_map<unsigned long, std::string>::const_iterator log_pages = log_to_pages.find(log_pba);
  for (unsigned int i = 0; i < BLOCK_SIZE; i++) {
    // move the page if only it is a written page
    if (!check_page_empty(logical_block + i)) {
      // find if the latest copy
      unsigned int log_page;
      Address src_addr;
      if (fetch_log_page(log_pages->second, i, &log_page)) {
        // read from latest copy of page in log block
        map_physical_to_SSD(log_pba, &package, &die, &plane, &block, &dummy);
        src_addr = Address(package, die, plane, block, log_page, PAGE);
      }
      else {
        // read from latest copy in data block
        map_physical_to_SSD(data_pba, &package, &die, &plane, &block, &dummy);
        src_addr = Address(package, die, plane, block, i, PAGE);
      }
      Event read_event = Event(READ, logical_block + i, 1, start_time);
      read_event.set_address(src_addr);
      ftl.controller.issue(read_event);
      Event write_event = Event(WRITE, logical_block + i, 1, start_time);
      map_physical_to_SSD(cln_pba, &package, &die, &plane, &block, &dummy);
      Address des_addr = Address(package, die, plane, block, i, PAGE);
      write_event.set_address(des_addr);
      ftl.controller.issue(write_event);
    }
  }
  
  // erase data block
  Event erase_data_event = Event(ERASE, logical_block, 1, start_time);
  erase_data_event.set_address(data_addr);
  ftl.controller.issue(erase_data_event);
  // erase log block
  Event erase_log_event = Event(ERASE, logical_block, 1, start_time);
  erase_log_event.set_address(log_addr);
  ftl.controller.issue(erase_log_event);
  // copy live pages from cleaning block to data block
  for (unsigned int i = 0; i < BLOCK_SIZE; i++) {
    // move the page if only it is a written page
    if (!check_page_empty(logical_block + i)) {
      Event read_event = Event(READ, logical_block + i, 1, start_time);
      map_physical_to_SSD(cln_pba, &package, &die, &plane, &block, &dummy);
      Address src_addr = Address(package, die, plane, block, i, PAGE);
      read_event.set_address(src_addr);
      ftl.controller.issue(read_event);
      Event write_event = Event(WRITE, logical_block + i, 1, start_time);
      map_physical_to_SSD(data_pba, &package, &die, &plane, &block, &dummy);
      Address des_addr = Address(package, die, plane, block, i, PAGE);
      write_event.set_address(des_addr);
      ftl.controller.issue(write_event);
    }
  }
  // erase cleaning block
  Event erase_cln_event = Event(ERASE, logical_block, 1, start_time);
  erase_cln_event.set_address(cln_addr);
  ftl.controller.issue(erase_cln_event);
  
  // update erase counts
  update_erase_count(data_pba);
  update_erase_count(log_pba);
  update_erase_count(cln_pba);
  
  return true;
}

void Ftl::init_ftl_user()
{
  // initialize the bit checking emptiness array
  unsigned int emp_len = (USABLE_SIZE + sizeof(unsigned int) - 1) / sizeof(unsigned int);
  // 0 bit for empty, 1 bit for written
  logical_to_emptiness = new unsigned int [emp_len]();
  
  // initialize erases count for all physical blocks
  erase_count = new unsigned int [NUM_OF_PHY_B]();
  
  // initialize offset mapping table from logical block to physical block
  logical_to_physical = new int [NUM_OF_LGC_B]();
  
  // initialize offset mapping table from physical data block to physical log block
  data_to_log = new int [NUM_OF_PHY_B]();

  // initialize a cleaning block
  //current_cln_address = USABLE_SIZE;
  
  // initialize the list of overprovisioning blocks
  for (unsigned int i = USABLE_SIZE; i < RAW_SIZE; i += BLOCK_SIZE) {
    op_blocks.push_back(i);
  }
}

enum status Ftl::translate( Event &event ){
  unsigned int package;
  unsigned int die;
  unsigned int plane;
  unsigned int block;
  unsigned int page;
  unsigned long data_address;
  unsigned long log_address;
  unsigned long logical_address;
  unsigned long physical_address;
  logical_address = event.get_logical_address();
  fprintf(log_file, "[translate] input LBA: %lu *******************************\n", logical_address);

  //print_info();
  
  // legal logical address is only from 0 to USABLE_SIZE - 1 
  if (logical_address >= USABLE_SIZE) {
    fprintf(log_file, "[translate] LBA not assessible\n");
    return FAILURE;
  }

  // set start time
  start_time = event.get_start_time();
  
  // find the physical address
  physical_address = check_physical_address(logical_address);
  map_physical_to_SSD(physical_address, &package, &die, &plane, &block, &page);
  fprintf(log_file, "[translate] original mapping is (%u,%u,%u,%u,%u)\n", 
    package, die, plane, block, page);
  // find the physical data block address
  data_address = physical_address - page;
  fprintf(log_file, "[translate] data block address is %lu\n", data_address);
  
  enum event_type operation = event.get_event_type();

  if (operation == WRITE) {   
    // check if page is empty
    if (check_page_empty(logical_address)) {
      set_page_written(logical_address);
      // original translation
      Address pba = Address(package, die, plane, block, page, PAGE);
      event.set_address(pba);
      fprintf(log_file, "[translate] wrote to an empty page\n");
      return SUCCESS;
    } 
    
    // check if log block mapped to data block
    if (check_log_block(data_address, &log_address)) {
      fprintf(log_file, "[translate] data block %lu maps to log block %lu\n",
        data_address, log_address);
      // check if there is a empty page in log block
      std::unordered_map<unsigned long, std::string>::const_iterator log_pages =
        log_to_pages.find(log_address);
      unsigned int log_page;
      if (next_free_log_page(log_pages->second, &log_page)) {
        log_to_pages[log_address] = 
          log_pages->second + std::to_string((long long)page) + ",";
        fprintf(log_file, "[translate] log block pba %lu contains %s\n",
          log_address, log_to_pages[log_address].c_str());
        map_physical_to_SSD(log_address, &package, &die, &plane, &block, &page);
        // logging translation
        Address pba = Address(package, die, plane, block, log_page, PAGE);
        event.set_address(pba);
        return SUCCESS;
      }
      
      fprintf(log_file, "[translate] mapped log block has no free page\n");
      
      // this data block needs cleaning
      if (over_erase_limit(data_address) == true) {
        data_address = garbage.remap_data_block(logical_address - page,
                                                data_address, log_address);
        if (log_address == data_address) {
          fprintf(log_file, "[translate] data block remapping failed\n");
          return FAILURE;
        }
      }
      // this log block needs cleaning
      if (over_erase_limit(log_address) == true) {
        log_address = garbage.remap_log_block(logical_address - page,
                                              data_address, log_address);
        if (log_address == data_address) {
          fprintf(log_file, "[translate] log block remapping failed\n");
          return FAILURE;
        }
      }
      if (garbage.clean(logical_address - page, data_address, log_address) == false) {
        fprintf(log_file, "[translate] cleaning failed\n");
        return FAILURE;
      }
      
      // give the first page of this cleaned log block
      std::string offsets = std::to_string((long long) page) + ",";
      log_to_pages[log_address] = offsets;
      fprintf(log_file, 
        "[translate] after cleaning, log block %lu contains %s\n",
        log_address, offsets.c_str());
      map_physical_to_SSD(log_address, &package, &die, &plane, &block, &page);
      // logging translation
      Address pba = Address(package, die, plane, block, 0, PAGE);
      event.set_address(pba);
      return SUCCESS;
    }
    
    // check if there is a free log block
    if (garbage.next_unmapped_log_block(&log_address, &package, &die, &plane, &block)) {
      fprintf(log_file, "[translate] found free log block (%u,%u,%u,%u,0)\n",
        package, die, plane, block); 
      // map log block to data block
      set_log_block(data_address, log_address);
      std::string offsets = std::to_string((long long) page) + ","; 
      log_to_pages.insert({{log_address, offsets}});
      fprintf(log_file, 
        "[translate] log block pba %lu contains %s\n", log_address, offsets.c_str());
      // logging translation
      Address pba = Address(package, die, plane, block, 0, PAGE);
      event.set_address(pba);
      return SUCCESS;
    }

    fprintf(log_file, "[translate] fail to rotate log blocks\n");
    return FAILURE;
  }

  if (operation == READ) {
    // check if page is valid
    if (check_page_empty(logical_address)) {
      fprintf(log_file, "[translate] read a empty page\n");
      return FAILURE; 
    }
    
    // check if log block mapped to data block
    if (check_log_block(data_address, &log_address)) {
      fprintf(log_file, 
        "[translate] data block %lu maps to log block %lu\n",
        data_address, log_address);
      // check if there is a corresponding page in log block
      std::unordered_map<unsigned long, std::string>::const_iterator log_pages =
        log_to_pages.find(log_address);
      fprintf(log_file, "[translate] log block %lu contains %s\n",
        log_address, log_pages->second.c_str());
      unsigned int log_page;
      if (fetch_log_page(log_pages->second, page, &log_page)) {
        // read from recent copy of page in log block
        map_physical_to_SSD(log_address, &package, &die, &plane, &block, &page);
        Address pba = Address(package, die, plane, block, log_page, PAGE);
        event.set_address(pba);
        fprintf(log_file, "[translate] reading page %u in log block\n", log_page);
        return SUCCESS;
      }
    }
    
    // read from original calculated page
    Address pba = Address(package, die, plane, block, page, PAGE);
    event.set_address(pba);
    fprintf(log_file, "[translate] reading original data block page\n");
    return SUCCESS;
  }

  fprintf(log_file, "[translate] unkown operation\n");
  return FAILURE;
}

enum status Garbage_collector::collect(Event &event __attribute__((unused)), enum GC_POLICY policy __attribute__((unused)))
{
  /*
   * No need to use this function
   */
  return FAILURE;
}

enum status Wear_leveler::level( Event &event __attribute__((unused)))
{
  /*
   * No need to use this function
   */
  return FAILURE;
}
