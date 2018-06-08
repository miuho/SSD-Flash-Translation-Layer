/* Copyright 2009, 2010 Brendan Tauras */

/* ssd.h is part of FlashSim. */

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

/* ssd.h
 * Brendan Tauras 2010-07-16
 * Main SSD header file
 * 	Lists definitions of all classes, structures,
 * 		typedefs, and constants used in ssd namespace
 *		Controls options, such as debug asserts and test code insertions
 */

/*
 * @ssd.h
 * 
 * HingOn Miu (hmiu)
 * Carnegie Mellon University
 * 2015-10-15
 */


#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <string>


#ifndef _SSD_H
#define _SSD_H

namespace ssd {

/* define exit codes for errors */
#define MEM_ERR -1
#define FILE_ERR -2

/* Uncomment to disable asserts for production */
#define NDEBUG

/* Simulator configuration from ssd_config.cpp */

/* Configuration file parsing for extern config variables defined below */
void load_entry(char *name, double value, unsigned int line_number);
void load_config(const char *config_name);
void print_config(FILE *stream);

/* Ram class:
 * 	delay to read from and write to the RAM for 1 page of data */
extern const double RAM_READ_DELAY;
extern const double RAM_WRITE_DELAY;

/* Bus class:
 * 	delay to communicate over bus
 * 	max number of connected devices allowed
 * 	flag value to detect free table entry (keep this negative)
 * 	number of time entries bus has to keep track of future schedule usage
 * 	number of simultaneous communication channels - defined by SSD_SIZE */
extern const double BUS_CTRL_DELAY;
extern const double BUS_DATA_DELAY;
extern const unsigned int BUS_MAX_CONNECT;
extern const double BUS_CHANNEL_FREE_FLAG;
extern const unsigned int BUS_TABLE_SIZE;
/* extern const unsigned int BUS_CHANNELS = 4; same as # of Packages, defined by SSD_SIZE */

/* Ssd class:
 * 	number of Packages per Ssd (size) */
extern const unsigned int SSD_SIZE;

/* Package class:
 * 	number of Dies per Package (size) */
extern const unsigned int PACKAGE_SIZE;

/* Die class:
 * 	number of Planes per Die (size) */
extern const unsigned int DIE_SIZE;

/* Plane class:
 * 	number of Blocks per Plane (size)
 * 	delay for reading from plane register
 * 	delay for writing to plane register
 * 	delay for merging is based on read, write, reg_read, reg_write 
 * 		and does not need to be explicitly defined */
extern const unsigned int PLANE_SIZE;
extern const double PLANE_REG_READ_DELAY;
extern const double PLANE_REG_WRITE_DELAY;

/* Block class:
 * 	number of Pages per Block (size)
 * 	number of erases in lifetime of block
 * 	delay for erasing block */
extern const unsigned int BLOCK_SIZE;
extern const unsigned int BLOCK_ERASES;
extern const double BLOCK_ERASE_DELAY;

/* Page class:
 * 	delay for Page reads
 * 	delay for Page writes */
extern const double PAGE_READ_DELAY;
extern const double PAGE_WRITE_DELAY;

/* Overprovisioning */
extern const float OVERPROVISIONING;

/* Log file path */
extern const char LOG_FILE[255];

/* Enumerations to clarify status integers in simulation
 * Do not use typedefs on enums for reader clarity */

/* Page states
 * 	empty   - page ready for writing (and contains no valid data)
 * 	valid   - page has been written to and contains valid data
 * 	invalid - page has been written to and does not contain valid data */
enum page_state{EMPTY, VALID, INVALID};

/* Block states
 * 	free     - all pages in block are empty
 * 	active   - some pages in block are valid, others are empty or invalid
 * 	inactive - all pages in block are invalid */
enum block_state{FREE, ACTIVE, INACTIVE};

/* I/O request event types
 * 	read  - read data from address
 * 	write - write data to address (page state set to valid)
 * 	erase - erase block at address (all pages in block are erased - 
 * 	                                page states set to empty)
 * 	merge - move valid pages from block at address (page state set to invalid)
 * 	           to free pages in block at merge_address */
enum event_type{READ, WRITE, ERASE, MERGE};

/* General return status
 * return status for simulator operations that only need to provide general
 * failure notifications */
enum status{FAILURE, SUCCESS, PAGE_INVALID, BLOCK_INVALID, BLOCK_CORRUPT};

/* Address valid status
 * used for the valid field in the address class
 * example: if valid == BLOCK, then
 * 	the package, die, plane, and block fields are valid
 * 	the page field is not valid */
enum address_valid{NONE, PACKAGE, DIE, PLANE, BLOCK, PAGE};

/* Garbage collection policies
 * FIFO: round-robin.
 * LRU: least recently used.
 * GREEDY: greedy by min effort.
 * COST_BENEFIT: LFS cost-benefit. */
enum GC_POLICY{FIFO, LRU, GREEDY, COST_BENEFIT};
/* Selected garbage collection policy */
extern enum GC_POLICY SELECTED_GC_POLICY;

/* List classes up front for classes that have references to their "parent"
 * (e.g. a Package's parent is a Ssd).
 *
 * The order of definition below follows the order of this list to support
 * cases of agregation where the agregate class should be defined first.
 * Defining the agregate class first enables use of its non-default
 * constructors that accept args
 * (e.g. a Ssd contains a Controller, Ram, Bus, and Packages). */
class Address;
class Event;
class Channel;
class Bus;
class Page;
class Block;
class Plane;
class Die;
class Package;
class Garbage_Collector;
class Wear_Leveler;
class Ftl;
class Ram;
class Controller;
class Ssd;

/* Class to manage physical addresses for the SSD.  It was designed to have
 * public members like a struct for quick access but also have checking,
 * printing, and assignment functionality.  An instance is created for each
 * physical address in the Event class. */
class Address
{
public:
	unsigned int package;
	unsigned int die;
	unsigned int plane;
	unsigned int block;
	unsigned int page;
	enum address_valid valid;
	Address(void);
	Address(const Address &address);
	Address(const Address *address);
	Address(unsigned int package, unsigned int die, unsigned int plane, unsigned int block, unsigned int page, enum address_valid valid);
	~Address();
	enum address_valid check_valid(unsigned int ssd_size = SSD_SIZE, unsigned int package_size = PACKAGE_SIZE, unsigned int die_size = DIE_SIZE, unsigned int plane_size = PLANE_SIZE, unsigned int block_size = BLOCK_SIZE);
	enum address_valid compare(const Address &address) const;
	void print(FILE *stream = stdout);
	Address &operator=(const Address &rhs);
  bool operator==(const Address &rhs);
  bool operator!=(const Address &rhs);
};

/* Class to manage I/O requests as events for the SSD.  It was designed to keep
 * track of an I/O request by storing its type, addressing, and timing.  The
 * SSD class creates an instance for each I/O request it receives. */
class Event 
{
public:
	Event(enum event_type type, unsigned long logical_address, unsigned int size, double start_time);
	~Event(void);
	void consolidate_metaevent(Event &list);
	unsigned long get_logical_address(void) const;
	const Address &get_address(void) const;
	const Address &get_merge_address(void) const;
	unsigned int get_size(void) const;
	enum event_type get_event_type(void) const;
	double get_start_time(void) const;
	double get_time_taken(void) const;
	double get_bus_wait_time(void) const;
	Event *get_next(void) const;
	void set_address(const Address &address);
	void set_merge_address(const Address &address);
	void set_next(Event &next);
	double incr_bus_wait_time(double time);
	double incr_time_taken(double time_incr);
	void print(FILE *stream = stdout);
private:
	double start_time;
	double time_taken;
	double bus_wait_time;
	enum event_type type;
	unsigned long logical_address;
	Address address;
	Address merge_address;
	unsigned int size;
	Event *next;
};

/* Quicksort for Channel class
 * Supply base pointer to array to be sorted along with inclusive range of
 * indices to sort.  The move operations for sorting the first array will also
 * be performed on the second array, or the second array can be NULL.  The
 * second array is useful for the channel scheduling table where we want to
 * sort by one row and keep data pairs in columns together. */
/* extern "C" void quicksort(double *array1, double *array2, long left, long right); */
void quicksort(double *array1, double *array2, long left, long right);
/* internal quicksort functions listed for documentation purposes
 * long partition(double *array1, double *array2, long left, long right);
 * void swap(double *x, double *y); */

/* Single bus channel
 * Simulate multiple devices on 1 bus channel with variable bus transmission
 * durations for data and control delays with the Channel class.  Provide the 
 * delay times to send a control signal or 1 page of data across the bus
 * channel, the bus table size for the maximum number channel transmissions that
 * can be queued, and the maximum number of devices that can connect to the bus.
 * To elaborate, the table size is the size of the channel scheduling table that
 * holds start and finish times of events that have not yet completed in order
 * to determine where the next event can be scheduled for bus utilization. */
class Channel
{
public:
	Channel(double ctrl_delay = BUS_CTRL_DELAY, double data_delay = BUS_DATA_DELAY, unsigned int table_size = BUS_TABLE_SIZE, unsigned int max_connections = BUS_MAX_CONNECT);
	~Channel(void);
	enum status lock(double start_time, double duration, Event &event);
	enum status connect(void);
	enum status disconnect(void);
private:
	void unlock(double current_time);
	unsigned int table_size;
	double * const lock_time;
	double * const unlock_time;
	unsigned int table_entries;
	unsigned int selected_entry;
	unsigned int num_connected;
	unsigned int max_connections;
	double ctrl_delay;
	double data_delay;
};

/* Multi-channel bus comprised of Channel class objects
 * Simulates control and data delays by allowing variable channel lock
 * durations.  The sender (controller class) should specify the delay (control,
 * data, or both) for events (i.e. read = ctrl, ctrl+data; write = ctrl+data;
 * erase or merge = ctrl).  The hardware enable signals are implicitly
 * simulated by the sender locking the appropriate bus channel through the lock
 * method, then sending to multiple devices by calling the appropriate method
 * in the Package class. */
class Bus
{
public:
	Bus(unsigned int num_channels = SSD_SIZE, double ctrl_delay = BUS_CTRL_DELAY, double data_delay = BUS_DATA_DELAY, unsigned int table_size = BUS_TABLE_SIZE, unsigned int max_connections = BUS_MAX_CONNECT);
	~Bus(void);
	enum status lock(unsigned int channel, double start_time, double duration, Event &event);
	enum status connect(unsigned int channel);
	enum status disconnect(unsigned int channel);
	Channel &get_channel(unsigned int channel);
private:
	unsigned int num_channels;
	Channel * const channels;
};

/* The page is the lowest level data storage unit that is the size unit of
 * requests (events).  Pages maintain their state as events modify them. */
class Page 
{
public:
	Page(const Block &parent, double read_delay = PAGE_READ_DELAY, double write_delay = PAGE_WRITE_DELAY);
	~Page(void);
	enum status _read(Event &event);
	enum status _write(Event &event);
	const Block &get_parent(void) const;
	enum page_state get_state(void) const;
	void set_state(enum page_state state);
private:
	enum page_state state;
	const Block &parent;
	double read_delay;
	double write_delay;
/* 	Address next_page; */
};

/* The block is the data storage hardware unit where erases are implemented.
 * Blocks maintain wear statistics for the FTL. */
class Block 
{
public:
	Block(const Plane &parent, unsigned int size = BLOCK_SIZE, unsigned long erases_remaining = BLOCK_ERASES, double erase_delay = BLOCK_ERASE_DELAY);
	~Block(void);
	enum status read(Event &event);
	enum status write(Event &event);
	enum status _erase(Event &event);
	const Plane &get_parent(void) const;
	unsigned int get_pages_valid(void) const;
	unsigned int get_pages_invalid(void) const;
	enum block_state get_state(void) const;
	enum page_state get_state(unsigned int page) const;
	enum page_state get_state(const Address &address) const;
	double get_last_erase_time(void) const;
	unsigned long get_erases_remaining(void) const;
	unsigned int get_size(void) const;
	enum status get_next_page(Address &address) const;
	void invalidate_page(unsigned int page);
private:
	unsigned int size;
	Page * const data;
	const Plane &parent;
	unsigned int pages_valid;
	unsigned int pages_invalid;
	enum block_state state;
	unsigned long erases_remaining;
	double last_erase_time;
	double erase_delay;
};

/* The plane is the data storage hardware unit that contains blocks.
 * Plane-level merges are implemented in the plane.  Planes maintain wear
 * statistics for the FTL. */
class Plane 
{
public:
	Plane(const Die &parent, unsigned int plane_size = PLANE_SIZE, double reg_read_delay = PLANE_REG_READ_DELAY, double reg_write_delay = PLANE_REG_WRITE_DELAY);
	~Plane(void);
	enum status read(Event &event);
	enum status write(Event &event);
	enum status erase(Event &event);
	enum status _merge(Event &event);
	const Die &get_parent(void) const;
	double get_last_erase_time(const Address &address) const;
	unsigned long get_erases_remaining(const Address &address) const;
	void get_least_worn(Address &address) const;
	unsigned int get_size(void) const;
	enum page_state get_state(const Address &address) const;
	void get_free_page(Address &address) const;
	unsigned int get_num_free(const Address &address) const;
	unsigned int get_num_valid(const Address &address) const;
private:
	void update_wear_stats(void);
	enum status get_next_page(void);
	unsigned int size;
	Block * const data;
	const Die &parent;
	unsigned int least_worn;
	unsigned long erases_remaining;
	double last_erase_time;
	double reg_read_delay;
	double reg_write_delay;
	Address next_page;
	unsigned int free_blocks;
};

/* The die is the data storage hardware unit that contains planes and is a flash
 * chip.  Dies maintain wear statistics for the FTL. */
class Die 
{
public:
	Die(const Package &parent, Channel &channel, unsigned int die_size = DIE_SIZE);
	~Die(void);
	enum status read(Event &event);
	enum status write(Event &event);
	enum status erase(Event &event);
	enum status merge(Event &event);
	enum status _merge(Event &event);
	const Package &get_parent(void) const;
	double get_last_erase_time(const Address &address) const;
	unsigned long get_erases_remaining(const Address &address) const;
	void get_least_worn(Address &address) const;
	enum page_state get_state(const Address &address) const;
	void get_free_page(Address &address) const;
	unsigned int get_num_free(const Address &address) const;
	unsigned int get_num_valid(const Address &address) const;
private:
	void update_wear_stats(const Address &address);
	unsigned int size;
	Plane * const data;
	const Package &parent;
	Channel &channel;
	unsigned int least_worn;
	unsigned long erases_remaining;
	double last_erase_time;
};

/* The package is the highest level data storage hardware unit.  While the
 * package is a virtual component, events are passed through the package for
 * organizational reasons, including helping to simplify maintaining wear
 * statistics for the FTL. */
class Package 
{
public:
	Package (const Ssd &parent, Channel &channel, unsigned int package_size = PACKAGE_SIZE);
	~Package ();
	enum status read(Event &event);
	enum status write(Event &event);
	enum status erase(Event &event);
	enum status merge(Event &event);
	const Ssd &get_parent(void);
	double get_last_erase_time (const Address &address) const;
	unsigned long get_erases_remaining (const Address &address) const;
	void get_least_worn (Address &address) const;
	enum page_state get_state(const Address &address) const;
	void get_free_page(Address &address) const;
	unsigned int get_num_free(const Address &address) const;
	unsigned int get_num_valid(const Address &address) const;
private:
	void update_wear_stats (const Address &address);
	unsigned int size;
	Die * const data;
	const Ssd &parent;
	unsigned int least_worn;
	unsigned long erases_remaining;
	double last_erase_time;
};

/* place-holder definitions for GC, WL, FTL, RAM, Controller 
 * please make sure to keep this order when you replace with your definitions */
class Garbage_collector 
{
public:
	Garbage_collector(Ftl &FTL, FILE *log_file);
	~Garbage_collector(void);
	enum status collect(Event &event, enum GC_POLICY policy);
  
  bool clean(unsigned long data_pba, unsigned long log_pba);
  unsigned long next_log_block_to_clean(enum GC_POLICY policy);

  FILE *log_file;
  Ftl &ftl;
};

class Wear_leveler 
{
public:
	Wear_leveler(Ftl &FTL, FILE *log_file);
	~Wear_leveler(void);
	enum status level( Event &event );
  FILE *log_file;
  Ftl &ftl;
};

/* Ftl class has some completed functions that get info from lower-level
 * hardware.  The other functions are in place as suggestions and can
 * be changed as you wish. */
class Ftl 
{
public:
	Ftl(Controller &controller, FILE *log_file);
	~Ftl(void);
	enum status read(Event &event);
	enum status write(Event &event);
  enum status garbage_collect(Event &event);
  FILE *log_file;
  enum status translate( Event &event );
	enum status erase(Event &event);
	enum status merge(Event &event);
	unsigned long get_erases_remaining(const Address &address) const;
	void get_least_worn(Address &address) const;
	enum page_state get_state(const Address &address) const;
    void init_ftl_user();
	Controller &controller;
	Garbage_collector garbage;
	Wear_leveler wear;
// new functions
  	bool fetch_log_page(std::string offsets, unsigned int data_page, unsigned int *log_page);
  	bool next_free_log_page(std::string offsets, unsigned int *page);
  	bool next_unmapped_log_block(unsigned long *log_pba,
                                     unsigned int *pa, unsigned int *d, 
                                     unsigned int *pl, unsigned int *b);
};

/* This is a basic implementation that only provides delay updates to events
 * based on a delay value multiplied by the size (number of pages) needed to
 * be written. */
class Ram 
{
public:
	Ram(double read_delay = RAM_READ_DELAY, double write_delay = RAM_WRITE_DELAY);
	~Ram(void);
	enum status read(Event &event);
	enum status write(Event &event);
private:
	double read_delay;
	double write_delay;
};

/* The controller accepts read/write requests through its event_arrive method
 * and consults the FTL regarding what to do by calling the FTL's read/write
 * methods.  The FTL returns an event list for the controller through its issue
 * method that the controller buffers in RAM and sends across the bus.  The
 * controller's issue method passes the events from the FTL to the SSD.
 *
 * The controller also provides an interface for the FTL to collect wear
 * information to perform wear-leveling.  */
class Controller 
{
public:
	Controller(Ssd &parent, FILE *log_file);
	~Controller(void);
	enum status event_arrive(Event &event);
  FILE *log_file;
  enum status issue(Event &event_list);
private:
	unsigned long get_erases_remaining(const Address &address) const;
	void get_least_worn(Address &address) const;
	double get_last_erase_time(const Address &address) const;
	enum page_state get_state(const Address &address) const;
	void get_free_page(Address &address) const;
	unsigned int get_num_free(const Address &address) const;
	unsigned int get_num_valid(const Address &address) const;
	Ssd &ssd;
	Ftl ftl;
};

/* The SSD is the single main object that will be created to simulate a real
 * SSD.  Creating a SSD causes all other objects in the SSD to be created.  The
 * event_arrive method is where events will arrive from DiskSim. */
class Ssd 
{
public:
	Ssd (FILE *log_file, unsigned int ssd_size = SSD_SIZE);
	~Ssd(void);
	double event_arrive(enum event_type type, unsigned long logical_address, unsigned int size, double start_time, int *status, Address &address);
  unsigned long get_pages_per_block();
  unsigned long get_total_erases_performed();
  unsigned long get_total_writes_observed();
  void write_ref_map(unsigned long lba, Address pba);
  bool is_valid(unsigned long lba, Address validate_with);
  unsigned long get_max_num_erases();
  FILE *log_file;
	friend class Controller;
private:
	enum status read(Event &event);
	enum status write(Event &event);
	enum status erase(Event &event);
	enum status merge(Event &event);
	unsigned long get_erases_remaining(const Address &address) const;
	void update_wear_stats(const Address &address);
	void get_least_worn(Address &address) const;
	double get_last_erase_time(const Address &address) const;	
	Package &get_data(void);
	enum page_state get_state(const Address &address) const;
	void get_free_page(Address &address) const;
	unsigned int get_num_free(const Address &address) const;
	unsigned int get_num_valid(const Address &address) const;
	unsigned int size;
	Controller controller;
	Ram ram;
	Bus bus;
	Package * const data;
	unsigned long erases_remaining;
	unsigned long least_worn;
	double last_erase_time;
  unsigned long total_erases_performed;
  unsigned long total_writes_observed;
  std::map<unsigned long, Address> ref_map;
  unsigned long max_num_erases;
};

} /* end namespace ssd */

#endif
