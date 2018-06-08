/* Copyright 2009, 2010 Brendan Tauras */

/* ssd_ssd.cpp is part of FlashSim. */

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

/* Ssd class
 * Brendan Tauras 2009-11-03
 *
 * The SSD is the single main object that will be created to simulate a real
 * SSD.  Creating a SSD causes all other objects in the SSD to be created.  The
 * event_arrive method is where events will arrive from DiskSim. */

#include <cmath>
#include <new>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <map>
#include "ssd.h"

using namespace ssd;

static bool reads_passed;
static bool writes_passed;
static bool valid_op;
std::map<std::string, std::map<unsigned long, Address>> block_map;

/* use caution when editing the initialization list - initialization actually
 * occurs in the order of declaration in the class definition and not in the
 * order listed here */
Ssd::Ssd(FILE *log_file, unsigned int ssd_size): 
  log_file(log_file),
	size(ssd_size), 
	controller(*this, log_file), 
	ram(RAM_READ_DELAY, RAM_WRITE_DELAY), 
	bus(size, BUS_CTRL_DELAY, BUS_DATA_DELAY, BUS_TABLE_SIZE, BUS_MAX_CONNECT), 

	/* use a const pointer (Package * const data) to use as an array
	 * but like a reference, we cannot reseat the pointer */
	data((Package *) malloc(ssd_size * sizeof(Package))), 

	/* set erases remaining to BLOCK_ERASES to match Block constructor args 
	 *	in Plane class
	 * this is the cheap implementation but can change to pass through classes */
	erases_remaining(BLOCK_ERASES), 

	/* assume all Planes are same so first one can start as least worn */
	least_worn(0), 

	/* assume hardware created at time 0 and had an implied free erasure */
	last_erase_time(0.0),
  total_erases_performed(0),
  total_writes_observed(0),
  max_num_erases(0)
{
	unsigned int i;

	/* new cannot initialize an array with constructor args so
	 *		malloc the array
	 *		then use placement new to call the constructor for each element
	 * chose an array over container class so we don't have to rely on anything
	 * 	i.e. STL's std::vector */
	/* array allocated in initializer list:
	 * data = (Package *) malloc(ssd_size * sizeof(Package)); */
	if(data == NULL){
		fprintf(log_file, "Ssd error: %s: constructor unable to allocate Package data\n", __func__);
		exit(MEM_ERR);
	}
	for (i = 0; i < ssd_size; i++)
	{
		(void) new (&data[i]) Package(*this, bus.get_channel(i), PACKAGE_SIZE);
	}

  reads_passed = true;
  writes_passed = true;
	valid_op = true;
  return;
}

Ssd::~Ssd(void)
{
	unsigned int i;
	/* explicitly call destructors and use free
	 * since we used malloc and placement new */
	for (i = 0; i < size; i++)
	{
		data[i].~Package();
	}
	free(data);
	return;
}

/* This is the function that will be called by DiskSim
 * Provide the event (request) type (see enum in ssd.h),
 * 	logical_address (page number), size of request in pages, and the start
 * 	time (arrive time) of the request
 * The SSD will process the request and return the time taken to process the
 * 	request.  Remember to use the same time units as in the config file. */
double Ssd::event_arrive(enum event_type type, unsigned long logical_address, unsigned int size, double start_time, int *status, Address &address)
{
	assert(start_time >= 0.0);
	assert((long long int) logical_address < (long long int) SSD_SIZE * PACKAGE_SIZE * DIE_SIZE * PLANE_SIZE * BLOCK_SIZE);

	/* allocate the event and address dynamically so that the allocator can
	 * handle efficiency issues for us */
	Event *event = NULL;

	if((event = new Event(type, logical_address, size, start_time)) == NULL)
	{
		fprintf(log_file, "Ssd error: %s: could not allocate Event\n", __func__);
		exit(MEM_ERR);
	}

	/* REAL SSD ONLY */
  *status = controller.event_arrive(*event);
  address = event->get_address();
	if(*status != SUCCESS)
	{
		fprintf(log_file, "Ssd error: %s: request failed:\n", __func__);
		event -> print(log_file);
	}

  //event -> print(log_file);

	/* use start_time as a temporary for returning time taken to service event */
	start_time = event -> get_time_taken();
	delete event;
	return start_time;
}

unsigned long Ssd::get_total_writes_observed()
{
  return total_writes_observed;
}

unsigned long Ssd::get_pages_per_block()
{
  return BLOCK_SIZE;
}

/* read write erase and merge should only pass on the event
 * 	the Controller should lock the bus channels
 * technically the Package is conceptual, but we keep track of statistics
 * 	and addresses with Packages, so send Events through Package but do not 
 * 	have Package do anything but update its statistics and pass on to Die */
enum status Ssd::read(Event &event)
{
  char block_str_copy[255] = "";
  std::map<unsigned long, Address>::iterator inner_block_map_it;
	assert(data != NULL && event.get_address().package < size && event.get_address().valid >= PACKAGE);
  /*
   * We remove the entry for the LBA which is read at least once, since it has to be read at least once
   * before cleaning is performed.
   */
  snprintf(block_str_copy, 255, "%u,%u,%u,%u", event.get_address().package, event.get_address().die, event.get_address().plane, event.get_address().block);
  std::string block_str = block_str_copy;
  inner_block_map_it = (block_map[block_str]).find(event.get_logical_address());
  if(inner_block_map_it != (block_map[block_str]).end()) {
    (block_map[block_str]).erase(event.get_logical_address());
  }
	return data[event.get_address().package].read(event);
}

enum status Ssd::write(Event &event)
{
  Address prev_addr;
  std::map<unsigned long, Address>::iterator ref_map_it;
  std::map<unsigned long, Address>::iterator inner_block_map_it;
  char block_str_copy[255] = "";
  assert(data != NULL && event.get_address().package < size && event.get_address().valid >= PACKAGE);
  /*
   * We insert an Address to block_map to validate if we perform a read on it before erases occur.
   */
  ref_map_it = ref_map.find(event.get_logical_address());
  if(ref_map_it != ref_map.end()) {
    /*
     * Remove stale entry.
     */
    prev_addr = ref_map[event.get_logical_address()];
    snprintf(block_str_copy, 255, "%u,%u,%u,%u", prev_addr.package, prev_addr.die, prev_addr.plane, prev_addr.block);
    std::string block_str = block_str_copy;
    inner_block_map_it = (block_map[block_str]).find(event.get_logical_address());
    if(inner_block_map_it != (block_map[block_str]).end()) {
      (block_map[block_str]).erase(event.get_logical_address());
    }
  }
  snprintf(block_str_copy, 255, "%u,%u,%u,%u", event.get_address().package, event.get_address().die, event.get_address().plane, event.get_address().block);
  std::string block_str = block_str_copy;
  (block_map[block_str])[event.get_logical_address()] = event.get_address();
  total_writes_observed++;

  /*
   * update ref_map with latest location of write.
   */
  ref_map[event.get_logical_address()] = event.get_address();
	return data[event.get_address().package].write(event);
}

enum status Ssd::erase(Event &event)
{
  Address addr;
  std::map<unsigned long, Address>::iterator inner_block_map_it;
  std::map<unsigned long, Address>::iterator ref_map_it;
  char block_str_copy[255] = "";

	assert(data != NULL && event.get_address().package < size && event.get_address().valid >= PACKAGE);
  /*
   * First check if block_map contains an empty map, only then are we sure that
   * we have seen at least one read for each page in ref_map before we are about
   * to erase the block.
   */
  snprintf(block_str_copy, 255, "%u,%u,%u,%u", event.get_address().package, event.get_address().die, event.get_address().plane, event.get_address().block);
  std::string block_str = block_str_copy;
  if((block_map[block_str]).size() != 0) {
    reads_passed = false;
    return FAILURE;
  }

  for(inner_block_map_it = (block_map[block_str]).begin(); inner_block_map_it != (block_map[block_str]).end(); inner_block_map_it++) {
    ref_map_it = ref_map.find(inner_block_map_it->first);
    if(ref_map_it != ref_map.end()) {
      addr = ref_map_it->second;
      if((addr.package == event.get_address().package) && (addr.die == event.get_address().die) && (addr.plane == event.get_address().plane) && (addr.block == event.get_address().block)) {
        writes_passed = false;
        return FAILURE;
      }
    }
  }

	enum status status = data[event.get_address().package].erase(event);
  total_erases_performed++;
	/* update values if no errors */
	if (status == SUCCESS) {
		update_wear_stats(event.get_address());
    if(get_erases_remaining(event.get_address()) < (BLOCK_ERASES-max_num_erases)) {
      max_num_erases = (BLOCK_ERASES - get_erases_remaining(event.get_address()));
    }
  }
	return status;
}

enum status Ssd::merge(Event &event)
{
  valid_op = false;
  return FAILURE;
	assert(data != NULL && event.get_address().package < size && event.get_address().valid >= PACKAGE);
	return data[event.get_address().package].merge(event);
}

/* add up the erases remaining for all packages in the ssd*/
unsigned long Ssd::get_erases_remaining(const Address &address) const
{
	assert (data != NULL);
	
	if (address.package < size && address.valid >= PACKAGE)
		return data[address.package].get_erases_remaining(address);
	else return erases_remaining;
}

void Ssd::update_wear_stats(const Address &address)
{
	assert(data != NULL);
	unsigned int i;
	unsigned int max_index = 0;
	unsigned long max = data[0].get_erases_remaining(address);
	for(i = 1; i < size; i++)
		if(data[i].get_erases_remaining(address) > max)
			max_index = i;
	least_worn = max_index;
	erases_remaining = max;
	last_erase_time = data[max_index].get_last_erase_time(address);
	return;
}

void Ssd::get_least_worn(Address &address) const
{
  valid_op = false;
  return;
	assert(data != NULL && least_worn < size);
	address.package = least_worn;
	address.valid = PACKAGE;
	data[least_worn].get_least_worn(address);
	return;
}

double Ssd::get_last_erase_time(const Address &address) const
{
  valid_op = false;
  return 0.0;
	assert(data != NULL);
	if(address.package < size && address.valid >= PACKAGE)
		return data[address.package].get_last_erase_time(address);
	else
		return last_erase_time;
}

enum page_state Ssd::get_state(const Address &address) const
{
	assert(data != NULL);
	assert(address.package < size && address.valid >= PACKAGE);
	return data[address.package].get_state(address);
}

void Ssd::get_free_page(Address &address) const
{
  valid_op = false;
  return;
	assert(address.package < size && address.valid >= PACKAGE);
	data[address.package].get_free_page(address);
	return;
}

unsigned int Ssd::get_num_free(const Address &address) const
{
  valid_op = false;
  return 0;
	assert(address.package < size && address.valid >= PACKAGE);
  return data[address.package].get_num_free(address);
}

unsigned int Ssd::get_num_valid(const Address &address) const
{
  valid_op = false;
  return 0;
	assert(address.valid >= PACKAGE);
	return data[address.package].get_num_valid(address);
}

unsigned long Ssd::get_total_erases_performed()
{
  return total_erases_performed;
}

void Ssd::write_ref_map(unsigned long lba, Address pba)
{
  ref_map[lba] = pba;
}

unsigned long Ssd::get_max_num_erases()
{
  /*
   * This function returns the max_erases left of
   * any block in the entire SSD.
   */
  Address addr;
  unsigned int package, die, plane, block;
  unsigned long max_erases_remaining = 0;
  for(package = 0; package < SSD_SIZE; package++) {
    for(die = 0; die < PACKAGE_SIZE; die++) {
      for(plane = 0; plane < DIE_SIZE; plane++) {
        for(block = 0; block < PLANE_SIZE; block++) {
          addr = Address(package, die, plane, block, 0, PAGE);
          if(data[addr.package].get_erases_remaining(addr) > max_erases_remaining) {
            max_erases_remaining = data[addr.package].get_erases_remaining(addr);
          }
        }
      }
    }
  }
  return max_erases_remaining;
  //return max_num_erases;
}

bool Ssd::is_valid(unsigned long lba, Address validate_with)
{
  std::map<unsigned long, Address>::iterator validate_it = ref_map.find(lba);
  Address addr;
  if(validate_it != ref_map.end()) {
    addr = validate_it->second;
  } else {
    fprintf(log_file, "LBA %lu is mapped to wrong physical address\n", lba);
    return false;
  }

  if(reads_passed == false) {
    fprintf(log_file, "Either you have performed less reads than required, or are performing reads and writes in an order that violates data consistency.\n");
    return false;
  }

  if(writes_passed == false) {
    fprintf(log_file, "Durability has been violated. You are erasing before ensuring that data is safe\n");
    return false;
  }

  if(valid_op == false) {
    fprintf(log_file, "Invalid / illegal operation used\n");
    return false;
  }

  if(addr == validate_with) {
    return true;
  }
  fprintf(log_file, "LBA %lu is mapped to wrong physical address\n", lba);

  return false;
}
