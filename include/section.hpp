#ifndef __SECTION_H__
#define __SECTION_H__

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include "hsautils.hpp"

enum VCDataType {
    VC_INVALID = -1,
    VC_UINT32 = 0,
    VC_INT,
    VC_FLOAT,
    VC_DOUBLE,
    VC_MAX,
};

VCDataType DataType(const char *type);
size_t SizeOfType(VCDataType type);
// TODO: collect all data type in a global header file
class HsacoAql
{
  public:
    void SetAll(int d, int wg_x, int wg_y, int wg_z, int g_x, int g_y, int g_z)
    {
      dim = d;
      workgroup_size_x = wg_x;
      workgroup_size_y = wg_y;
      workgroup_size_z = wg_z;
      grid_size_x = g_x;
      grid_size_y = g_y;
      grid_size_z = g_z;
    }

    int dim;
    int workgroup_size_x;
    int workgroup_size_y;
    int workgroup_size_z;
    int grid_size_x;
    int grid_size_y;
    int grid_size_z;
};

enum HCStoreType {
    HC_ADDR = 0,
    HC_VALUE,
};

class JsonKernArg
{
  public:
    int index;
    HCStoreType sType;
    VCDataType dType;
    int size;
    union {
        int i;
        float f;
        double d;
    }value;

    void SetAll(int idx, int s_type, const char *d_type, int sz)
    {
      index = idx;
      sType = (HCStoreType)s_type;
      size = sz;
      if (strncmp(d_type, "float", sizeof("float")-1) == 0)
        dType = VC_FLOAT;
      else if (strncmp(d_type, "double", sizeof("double")-1) == 0)
        dType = VC_DOUBLE;
      else if (strncmp(d_type, "int", sizeof("int")-1) == 0)
        dType = VC_INT;
      else if (strncmp(d_type, "uint32", sizeof("uint32")-1) == 0)
        dType = VC_UINT32;
      else
        dType = VC_INVALID;
    }
};

class HsacoKernArg
{
public:
  HCStoreType sType;
  VCDataType dType;
  std::unique_ptr<HSAMemoryObject> mem;
  union {
      int i;
      float f;
      double d;
  }value;
};

enum VCSectionType {
  VC_NULL = -1,
  VC_AQL = 0,
  VC_KERN_OBJ,
  VC_KERN_ARG_POOL,
  VC_KERN_ARG,
  VC_TYPE_MAX,
};

class VCSection {
public:
    VCSection(VCSectionType secType, uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    virtual ~VCSection(); // call VCKernArg firstly

    const std::string SectionName[VC_TYPE_MAX] = {
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
    uint32_t ArgsNum() { return m_args_num; }
    void SetArgsNum(uint32_t num) { m_args_num = num; }

    friend std::ostream& operator<< (std::ostream &out, VCSection &sec);

    virtual VCDataType DType() { return VC_INVALID; }
    virtual uint32_t Offset() { return 0; }
    virtual uint32_t Index() { return 0; }
    virtual void SetValue(uint32_t value) { return; }
    virtual uint32_t Value() { return 0; }
    virtual bool IsAddr() { return true; }
    virtual std::ostream& Print(std::ostream &out);
    virtual void SetDataType(VCDataType type) { return; };

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
    std::string m_seperator;
    uint32_t m_args_num;
};

class VCKernArg : public VCSection {
public:
    VCKernArg(uint32_t index,
              uint32_t offset, bool isAddr,
              uint32_t size, hsa_agent_t agent, MemoryRegionType memType);
    ~VCKernArg(); 


    void SetDataType(VCDataType type) { m_dtype = type; }
    VCDataType DType() { return m_dtype; }
    uint32_t Offset() { return m_offset; }
    uint32_t Index() { return m_index; }
    void SetValue(uint32_t value) { m_value = value; }
    uint32_t Value() { return m_value; }
    bool IsAddr() { return m_is_addr; }
    std::ostream& Print(std::ostream &out);

private:
    uint32_t m_index;
    uint32_t m_total_index;
    uint32_t m_offset;
    uint32_t m_value;
    bool m_is_addr;
    VCDataType m_dtype;
};

#endif /* __SECTION_H__ */