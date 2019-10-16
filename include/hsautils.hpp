#ifndef __HSA_UTILS_H__
#define __HSA_UTILS_H__

#include <hsa.h>
#include <hsa_ext_amd.h>
#include <fcntl.h>
#include <assert.h>
#include <cstring>
#include <string>
#include <iostream>
#include <unistd.h> // close()

#define EXPECT_SUCCESS(val) assert(HSA_STATUS_SUCCESS == (val))
#define EXPECT_FOUND(val) assert(HSA_STATUS_INFO_BREAK == (val))

typedef hsa_status_t (*memory_region_cb_t)(hsa_region_t, void*);
typedef unsigned char uchar;

class HSAInit {
  public:
    HSAInit() { EXPECT_SUCCESS(hsa_init()); }        // start before everything
    ~HSAInit() { EXPECT_SUCCESS(hsa_shut_down()); }; // end after everything
};

class HSAAgent {
  public:
    HSAAgent(hsa_device_type_t type = HSA_DEVICE_TYPE_GPU);
    hsa_agent_t Get(void) { return m_agent; }

  private:
    hsa_agent_t GetDefaultDevice(hsa_device_type_t target_device);
    hsa_agent_t m_agent;
};

// define memory object
enum MemoryRegionType {
  MEM_SYS = 0,
  MEM_LOC,
  MEM_KERNARG,
  MEM_REG_TYPE_MAX,
};

class HSAMemoryObject {
  public:
    HSAMemoryObject(size_t size, hsa_agent_t agent, MemoryRegionType type);
    ~HSAMemoryObject();

    size_t Size(void) const { return m_size; }
    template<typename DataType>
    DataType As() {
      return reinterpret_cast<DataType>(m_ptr);
    }
    template<typename T>
    void Fill(T value) {
      for (size_t i = 0; i < m_size / sizeof(T); i++) reinterpret_cast<T*>(m_ptr)[i] = value;
    }

  private:
    size_t m_size;
    hsa_agent_t m_agent;
    MemoryRegionType m_type;
    void *m_ptr;
    hsa_region_t m_region;

    static hsa_status_t get_system_memory_cb(hsa_region_t region, void *data);
    static hsa_status_t get_local_memory_cb(hsa_region_t region, void *data);
    static hsa_status_t get_kernarg_memory_cb(hsa_region_t region, void *data);

    memory_region_cb_t memory_region_cb_pool[MEM_REG_TYPE_MAX] = {
      &get_system_memory_cb,  // MEM_SYS
      &get_local_memory_cb,   // MEM_LOC
      &get_kernarg_memory_cb, // MEM_KERNARG
    };

    memory_region_cb_t get_memory_region_cb(MemoryRegionType type) {
      assert(type < MEM_REG_TYPE_MAX);
      return memory_region_cb_pool[type];
    }
};

class HSASignal {
  public:
    HSASignal(hsa_signal_value_t value = 1);
    ~HSASignal();

    hsa_signal_t Get(void) const { return m_signal; };
    void Wait(hsa_signal_condition_t condition, hsa_signal_value_t value);

  private:
    hsa_signal_t m_signal;
};

class HSAExecutable {
  public:
    // FIXME: HSA_PROFILE_BASE doens't work when loads code object
    HSAExecutable(hsa_agent_t agent,
                  hsa_profile_t profile = HSA_PROFILE_FULL,
                  hsa_default_float_rounding_mode_t rounding_mode = HSA_DEFAULT_FLOAT_ROUNDING_MODE_DEFAULT);
    ~HSAExecutable();
    hsa_status_t LoadCodeObject(const char *fileName, const char *kernelName);
    hsa_executable_symbol_t GetSymbol(void) const { return m_symbol; }

  private:
    hsa_agent_t m_agent;
    hsa_executable_t m_executable;
    hsa_profile_t m_profile;
    hsa_default_float_rounding_mode_t m_round_mode;
    std::string m_fileName;
    std::string m_kernelName;
    hsa_executable_symbol_t m_symbol;
};

class HSAQueue {
  public:
    HSAQueue(hsa_agent_t agent, uint32_t size, hsa_queue_type32_t type);
    ~HSAQueue();
    hsa_status_t LoadCodeObject(const char *fileName, const char *kernelName);
    hsa_status_t SubmitPacket(hsa_kernel_dispatch_packet_t &pkt);

  protected:
    hsa_agent_t m_agent;
    uint32_t m_size;
    hsa_queue_type32_t m_type;
    hsa_queue_t *m_queue;
};
#endif /* __HSA_UTILS_H__ */
