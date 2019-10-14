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
    VC_INVALID = -1,
    VC_ADDR = 1,
    VC_VAL,
};

class VCSection {
public:
    VCSection(VCSectionType secType, uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    virtual ~VCSection(); // call VCKernArg firstly

    const std::string SectionName[VC_NUM] = {
        "AQL",
        "Kernel Object",
        "Kernel Arg Pool",
        "Kernel Arg",
    };
    std::string Name(void) const { return SectionName[m_stype]; }
    VCSectionType SType() { return m_stype; }
    uint32_t Size() { return m_size; }
    void SetValue(std::string &str);
    bool OutOfMemory(uint32_t pos);
    bool Allocated() { return m_mem != NULL; }

    friend std::ostream& operator<< (std::ostream &out, VCSection &sec);

    virtual VCDataType DType() { return VC_INVALID; }
    virtual uint32_t Offset() { return 0; }
    virtual uint32_t Index() { return 0; }
    virtual void SetValue(uint32_t value) { return; }
    virtual uint32_t Value() { return 0; }
    virtual bool IsAddr() { return VC_ADDR; }
    virtual std::ostream& Print(std::ostream &out);

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
    uint32_t m_size;
    HSAMemoryObject *m_mem;
    uint32_t m_pos;
};

class VCKernArg : public VCSection {
public:
    VCKernArg(uint32_t index,
              uint32_t offset, bool isAddr,
              uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    ~VCKernArg(); 


    VCDataType DType() { return m_dtype; }
    uint32_t Offset() { return m_offset; }
    uint32_t Index() { return m_index; }
    void SetValue(uint32_t value) { m_value = value; }
    uint32_t Value() { return m_value; }
    bool IsAddr() { return m_is_addr; }
    std::ostream& Print(std::ostream &out);

private:
    VCDataType m_dtype;
    uint32_t m_index;
    uint32_t m_total_index;
    uint32_t m_offset;
    uint32_t m_value;
    bool m_is_addr;
};

#endif /* __SECTION_H__ */