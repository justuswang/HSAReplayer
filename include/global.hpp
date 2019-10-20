#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <vector>
#include <memory>
#include <string>
#include <cstring>
#include <iostream>

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
      dType = DataType(d_type);
    }
};

enum VCSectionType {
  VC_NULL = -1,
  VC_AQL = 0,
  VC_KERN_OBJ,
  VC_KERN_ARG_POOL,
  VC_KERN_ARG,
  VC_TYPE_MAX,
};

#endif /* GLOBAL_H__ */