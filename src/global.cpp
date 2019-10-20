#include "global.hpp"

VCDataType DataType(const char *type)
{
  VCDataType t = VC_INVALID;
  if (strncmp(type, "float", sizeof("float")-1) == 0)
    t = VC_FLOAT;
  else if (strncmp(type, "int", sizeof("int")-1) == 0)
    t = VC_INT;
  else if (strncmp(type, "double", sizeof("double")-1) == 0)
    t = VC_DOUBLE;
return t;
}

size_t SizeOfType(VCDataType type)
{
  if (type == VC_INT)
    return sizeof(int);
  else if (type == VC_FLOAT)
    return sizeof(float);
  else if (type == VC_DOUBLE)
    return sizeof(double);
  else if (type == VC_UINT32)
    return sizeof(uint32_t);
  else {
    return 0;
    std::cout << "Unknown data type" << std::endl;
  }
}
