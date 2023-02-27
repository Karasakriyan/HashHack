#include "Header.hpp"

#define CACHE_LINE_SIZE_BYTES (8 * sizeof(uint64_t))
#define ARRAY_SIZE_BYTES (1 * 12 * 1024 * 1024)

int main()
{
    int slices_number = GetCacheSlicesNumber();
    cout << "Cache slices number: " << slices_number << endl;

    char* slice_accordance_table = SliceAccordanceTableLoad();    
    uint64_t* arr = CreateArray(ARRAY_SIZE_BYTES, CACHE_LINE_SIZE_BYTES);
    int result = 0;

    try
    {
        for (size_t i = 0; i < ARRAY_SIZE_BYTES / 8; i++)
        {
            uint64_t physical_address = GetPhysicalAddress(arr + i * sizeof(uint64_t));
            if (!PhysicalAddressLookup(slice_accordance_table, physical_address))
            {
                PMON_counters_init(slices_number);
                Polling(arr + i * sizeof(uint64_t));
                SliceAccordanceTableUpdate(slice_accordance_table, physical_address, PMON_counters_check(slices_number));
            }
            else
                continue;
            if (i % 10000 == 0)
            {
                std::cout << i << "/" << ARRAY_SIZE_BYTES/8 << "\n";
            }
        }
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    SliceAccordanceTableSave(slice_accordance_table);
    

    free(slice_accordance_table);
    free(arr);

    return 0;
}


// Load address table *
// Allocate memory *
// for address
    // Translate addr *
    // Check addr in table *
    // init MSR *
    // zaloop address
    // check MSR *
    // enter data in table *