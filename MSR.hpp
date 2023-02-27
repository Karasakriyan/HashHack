#pragma once

#define GLOBAL_COUNTER_CONTROL 0xE01
#define ENABLE_COUNTERS 0x20000000
#define LLC_LOOKUP_MONITOR 0x508f34

uint64_t UNCORE_CBOXES[] = {0x700, 0x708, 0x710, 0x718, 0x720, 0x728};
uint64_t UNCORE_CBOXES_DATA[] = {0x702, 0x70A, 0x712, 0x71A, 0x722, 0x72A};

/// @brief Read value from MSR
/// @param reg Register address to read
/// @return Data from register
uint64_t ReadMSR0(uint32_t reg)
{
    uint64_t data;
    char* msr_filename = "/dev/cpu/0/msr";

    static int fd = open(msr_filename, O_RDONLY);
    if(fd < 0) 
    {
        switch (errno)
        {
        case ENXIO:
            std::cerr << "NO CPU!" << std::endl;
            exit(2);
            break;
        case EIO:
            std::cerr << "CPU doesn't support MSRs!" << std::endl;
            exit(3);
            break;
        default:
            std::cerr << "Error open MSR for reading!" << std::endl;
            exit(127);
            break;
        }
	  }

    if (pread(fd, &data, sizeof data, reg) != sizeof(data))
    {
        std::cerr << "Error reading from msr" << std::endl;
        exit(-1);
    }
        
    close(fd);

    return data;
}

/// @brief Write data to MSR
/// @param reg Register address to write
/// @param values_number Number of values to write
/// @param register_values Values array to write
void WriteMSR0(uint32_t reg, int values_number, uint64_t* register_values)
{
    uint64_t data;
    char* msr_filename = "/dev/cpu/0/msr";

    static int fd = open(msr_filename, O_WRONLY);
    if (fd < 0)
    {
        std::cerr << "Error opening msr to write!" << std::endl;
        exit(-1);
    }

    while (values_number--)
    {
        data = *register_values++;

        if (pwrite(fd, &data, sizeof data, reg) != sizeof(data));
        switch (errno)
        {
        case EIO:
            std::cerr << std::hex << "Cannot write 0x" << data << " to 0x" << reg << std::endl;
            exit(4);
            break;
        default:
            std::cerr << "Unknown error writing to MSR" << std::endl;
            exit(127);
            break;
        }
    }

    close(fd);
}

uint64_t ReadMSR(uint32_t reg)
{
	uint64_t data;
	int cpu = 0;
	char * msr_file_name = "/dev/cpu/0/msr";
	
  static int fd = -1;  
  
  
	if (fd < 0) {
    fd = open(msr_file_name, O_RDONLY);
    if(fd < 0) {
		  if (errno == ENXIO) {
			  fprintf(stderr, "rdmsr: No CPU %d\n", cpu);
			  exit(2);
		  } else if (errno == EIO) {
			  fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
				  cpu);
			  exit(3);
		  } else {
			  perror("rdmsr: open");
			  exit(127);
		  }
	  }
	}

	if (pread(fd, &data, sizeof data, reg) != sizeof data) {
		if (errno == EIO) {
			std::cerr << "rdmsr: CPU cannot read MSR\n";
			exit(4);
		} else {
			perror("rdmsr: pread");
			exit(127);
		}
	}

	//close(fd);

	return data;
}


/*
 * Write to an MSR on CPU 0
 */
void WriteMSR(uint32_t reg, int valcnt, uint64_t *regvals)
{
  uint64_t data;
  char * msr_file_name = "/dev/cpu/0/msr";
  int cpu = 0;

	static int fd = -1;

	if(fd < 0){
	  fd = open(msr_file_name, O_WRONLY);
	  if (fd < 0) {
		  if (errno == ENXIO) {
			  fprintf(stderr, "wrmsr: No CPU %d\n", cpu);
			  exit(2);
		  } else if (errno == EIO) {
			  fprintf(stderr, "wrmsr: CPU %d doesn't support MSRs\n",
				  cpu);
			  exit(3);
		  } else {
			  perror("wrmsr: open");
			  exit(127);
		  }
	  }
	}

	while (valcnt--) {
		data=*regvals++;
		if (pwrite(fd, &data, sizeof data, reg) != sizeof data) {
			if (errno == EIO) {
				fprintf(stderr,
					"wrmsr: CPU %d cannot set MSR "
					"0x%08x to 0x%016lx\n",
					cpu, reg, data);
				exit(4);
			} else {
				perror("wrmsr: pwrite");
				exit(127);
			}
		}
	}

	//close(fd);

	return;
}