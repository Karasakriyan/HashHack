#pragma once

#include <iostream>
#include <iomanip>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <chrono>
#include <fcntl.h>
#include <unistd.h>
#include <immintrin.h>
#include <sys/mman.h>
#include "MSR.hpp"

#define POLLING_COUNT 10000
#define SLICE_TABLE_SIZE (128 * 1024 * 1024)
#define DEFAULT_TABLE_PATH "SliceAccordanceTable.bin"

using namespace std::chrono;
using std::cout;
using std::endl;

typedef struct {
    uint64_t pfn : 55;
    unsigned int soft_dirty : 1;
    unsigned int file_page : 1;
    unsigned int swapped : 1;
    unsigned int present : 1;
} PagemapEntry;

uint64_t* CreateArray(size_t size_b, size_t alignment_b)
{
	std::random_device rd;
	std::mt19937_64 gen(rd());
	std::uniform_int_distribution<> dist(0, UINT32_MAX);

	size_t size = size_b / sizeof(uint64_t);
	uint64_t* tmp;
	try
	{
		tmp = (uint64_t*)aligned_alloc(alignment_b, size * sizeof(uint64_t));
		mlock(tmp, size * sizeof(uint64_t));
		for (size_t i = 0; i < size; i++)
			tmp[i] = dist(gen);
	}
	catch (const std::exception& e)
	{
		std::cerr << e.what() << std::endl;
	}

	return tmp;
}

/// @brief Loading table of address-to-slice accordance
/// @param path Path to table
/// @return Pointer to table in memory
char* SliceAccordanceTableLoad(std::string path = DEFAULT_TABLE_PATH)
{
	char* table = (char*)malloc(SLICE_TABLE_SIZE * sizeof(char));

	std::ifstream in(path, std::ios::binary | std::ios::in);
	if (!in.bad())
		in.read(table, SLICE_TABLE_SIZE * sizeof(char));
	else
		memset(table, -1, SLICE_TABLE_SIZE * sizeof(char));

	in.close();

	return table;
}

/// @brief Saving table of address-to-slice accordance to the disk
/// @param table Pointer to table
/// @param path Path to file
void SliceAccordanceTableSave(char* table, std::string path = DEFAULT_TABLE_PATH)
{
	std::ofstream out(path, std::ios::binary | std::ios::out);

	out.write(table, SLICE_TABLE_SIZE * sizeof(char));

	out.close();
}

/// @brief Update table of address-to-slice accordance with new entry
/// @param table Pointer to table
/// @param address Address to update
/// @param slice Slice to appoint
inline void SliceAccordanceTableUpdate(char* table, uint64_t address, int slice)
{
	bool high_part;
	if ((address & (1 << 6)) == (1 << 6))
		high_part = true;

	uint64_t table_addr = address >> 7;
	if (high_part)
	{
		table[table_addr] |= (slice << 4);
		table[table_addr] |= (1 << 7);
	}
	else
	{
		table[table_addr] |= slice;
		table[table_addr] |= (1 << 3);
	}
}

/// @brief Look, if given address is already checked
/// @param table Pointer to table
/// @param address Address to check
/// @return Is address checked
bool PhysicalAddressLookup(char* table, uint64_t address)
{
	bool high_part;
	if ((address & (1 << 6)) == (1 << 6))
		high_part = true;

	uint64_t table_addr = address >> 7;

	if (high_part)
		if ((table[table_addr] & (1 << 7)) == (1 << 7))
			return true;
		else
			return false;
	else
		if ((table[table_addr] & (1 << 3)) == (1 << 3))
			return true;
		else
			return false;
}

/// @brief Access cache slice accorded to address
/// @param addr Address to poll
void Polling(uintptr_t addr)
{
	register int i asm("eax");
	register uintptr_t ptr asm("ebx") = addr;
	for (i = 0; i < POLLING_COUNT; i++) {
		_mm_clflush((void*)ptr);
	}
}

/// @brief Verifying processor's cache slice number
/// @return Last 4 bits of 0x396 MSR register's value
uint64_t GetCacheSlicesNumber()
{
	return ReadMSR(0x396) & (1 + (1 << 1) + (1 << 2) + (1 << 3));
}

/// @brief Init LLC event counters for 'slices number' performance monitors
/// @param slices_number Number of slices
inline void PMON_counters_init(int slices_number)
{
	uint64_t val[] = {0x0};
	WriteMSR(GLOBAL_COUNTER_CONTROL, 1, val);	// Stop counters

	for (int i = 0; i < slices_number; i++)
	WriteMSR(UNCORE_CBOXES_DATA[i], 1, val); // Reset all counters
	
	val[0] = LLC_LOOKUP_MONITOR;
	for (int i = 0; i < slices_number; i++)
		WriteMSR(UNCORE_CBOXES[i], 1, val); //Configure cboxes
	val[0] = {ENABLE_COUNTERS};
	WriteMSR(GLOBAL_COUNTER_CONTROL, 1, val);	// Start counters
}

/// @brief Get number of slice with maximal number of LLC_LOOKUP events
/// @param slices_number Number of slices
/// @return Number of most used slice
inline int PMON_counters_check(int slices_number)
{
	uint64_t val[] = {0x0};
	WriteMSR(GLOBAL_COUNTER_CONTROL, 1, val);	// Stop counters

	int* cboxes = (int*)malloc(slices_number * sizeof(int));
	for (int i = 0; i < slices_number; i++)
		cboxes[i] = ReadMSR(UNCORE_CBOXES_DATA[i]);

	val[0] = 0x0;
	for (int i = 0; i < slices_number; i++)
		WriteMSR(UNCORE_CBOXES[i], 1, val); //Configure cboxes

	int max_elem = 0;
	int max_slice = 0;
	for (int i = 0; i < slices_number; i++)
	{
		if (cboxes[i] > max_elem)
		{
			max_slice = i;
			max_elem = cboxes[i];
		}
	}
	
	return max_slice + 1;
}

/// @brief Pagemap lookup for virtual address
/// @param pagemap_fd File descriptor for pagemap
/// @param virtual_address Virtual address to search
/// @return Pagemap entry with data
PagemapEntry* ParsePagemap(int pagemap_fd, uintptr_t virtual_address)
{
	size_t nread;
    ssize_t ret;
    uint64_t data;
    uintptr_t vpn;
	PagemapEntry* entry = (PagemapEntry*)malloc(sizeof(PagemapEntry));

    vpn = virtual_address / sysconf(_SC_PAGE_SIZE);
    nread = 0;
    while (nread < sizeof(data)) 
	{
        ret = pread(pagemap_fd, ((uint8_t*)&data) + nread, sizeof(data) - nread, vpn * sizeof(data) + nread);
        nread += ret;
        if (ret <= 0)
            std::cerr << "RDERR" << endl;
    }
    entry->pfn = data & (((uint64_t)1 << 55) - 1);
    entry->soft_dirty = (data >> 55) & 1;
    entry->file_page = (data >> 61) & 1;
    entry->swapped = (data >> 62) & 1;
    entry->present = (data >> 63) & 1;
    return entry;
}

/// @brief Translating virtual address to physical
/// @param virt_addr Virtual address
/// @return Physical address
uintptr_t GetPhysicalAddress(uint64_t virtual_address)
{
	pid_t pid = getpid();
	char pagemap_file[BUFSIZ];
    int pagemap_fd;
	uint64_t physical_address;

	snprintf(pagemap_file, sizeof(pagemap_file), "/proc/%ju/pagemap", (uintmax_t)pid);
	pagemap_fd = open(pagemap_file, O_RDONLY);
	if (pagemap_fd < 0)
		std::cerr << "Error opening pagemap";

	PagemapEntry* entry = ParsePagemap(pagemap_fd, virtual_address);
	close(pagemap_fd);

	physical_address = (entry->pfn * sysconf(_SC_PAGE_SIZE)) + (virtual_address % sysconf(_SC_PAGE_SIZE));
	
    return physical_address;
}