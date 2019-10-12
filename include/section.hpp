#ifndef __SECTION_H__
#define __SECTION_H__

#include <iostream>
#include <string>
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

    void SetValue(std::string &str);
    bool OutOfMemory(uint32_t pos);

    // The implementation of a non-specialized template must be visible to a translation unit that uses it.
    // https://stackoverflow.com/questions/10632251/undefined-reference-to-template-function
    template<typename T>
    void SetValue(T val, uint32_t offset)
    {
        uint32_t pos = offset / sizeof(uint32_t);
        if (OutOfMemory(pos)) {
            std::cerr << "Out of memory size!" << std::endl;
            return;
        }
        *((T*)(m_mem->As<uint32_t*>() + pos)) = val;
    }

    template<typename T>
    T GetValue(uint32_t offset)
    {
        uint32_t pos = offset / sizeof(uint32_t);
        if (OutOfMemory(pos)) {
            std::cerr << "Out of memory size!" << std::endl;
            return -1;
        }
        return *((T*)(m_mem->As<uint32_t*>() + pos));
    }

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

#endif /* __SECTION_H__ */