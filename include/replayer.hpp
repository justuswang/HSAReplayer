#ifndef __REPLAYER_H__
#define __REPLAYER_H__

#include <iostream>
#include <fstream>
#include <vector>
#include "hsautils.hpp"

enum VCSectionType {
  VC_NULL = -1,
  VC_AQL = 0,
  VC_KERN_OBJ,
  VC_KERN_ARG_POOL,
  VC_KERN_ARG,
  VC_NUM,
};

enum VCDataType {
    VC_ADDR = 1,
    VC_VAL,
};

class VCSection {
public:
    VCSection(VCSectionType secType, uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    VCSection(VCSectionType secType, uint32_t num, uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    VCSection(VCSectionType secType, uint32_t index,
              uint32_t offset, bool isAddr,
              uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    ~VCSection();

    const std::string SectionName[VC_NUM] = {
        "AQL",
        "Kernel Object",
        "Kernel Arg Pool",
        "Kernel Arg",
    };
    std::string Name(void) const { return SectionName[m_stype]; }
    VCSectionType SType() { return m_stype; }
    uint32_t Size() { return m_size; }
    bool IsAddr() { return m_is_addr; }
    uint32_t NumberOfIndex() { return m_arg_total_index; }

    VCDataType DType() { return m_dtype; } // KernelArg
    uint32_t Offset() { return m_arg_offset; } // KernelArg
    uint32_t Index() { return m_arg_index; } // KernelArg
    void SetValue(uint32_t value) { m_arg_value = value; }; // KernelArg
    uint32_t Value() { return m_arg_value; } // KernelArg

    template<typename T>
    void SetValue(T val, uint32_t offset);
    template<typename T>
    T GetValue(uint32_t offset);
    void SetValue(std::string &str);
    bool OutOfMemory(uint32_t pos);

    template<typename T>
    size_t Number() { return m_size / sizeof(T); }

    template<typename T>
    T As() { return m_mem->As<T>(); }

protected:
    VCSectionType m_stype;
    VCDataType m_dtype; // KernelArg
    uint32_t m_arg_index; // KernelArg
    uint32_t m_arg_total_index; // KernelArg
    uint32_t m_arg_offset; // KernelArg
    uint32_t m_arg_value; // KernelArg
    uint32_t m_size;
    HSAMemoryObject *m_mem;
    uint32_t m_pos;
    bool m_is_addr;
};

class Replayer {
public:
  Replayer() { return; }
  ~Replayer();

  void LoadVectorFile(const char *fileName, hsa_agent_t agent);

  template<typename T>
  void RetrieveData(std::vector<T> &data, std::string &str);
  uint64_t GetHexValue(std::string &line, const char *key);

  void UpdateKernelArgPool();
  VCSection* GetSection(VCSectionType type);
  void ShowSection(VCSectionType type);

private:
  std::vector<VCSection*> m_sections;
  void ShowKernelArgs(void);
};

#endif /* __REPLAYER_H__ */