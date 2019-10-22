#include "section.hpp"
#include <iomanip>

VCSection::~VCSection()
{
  if (m_mem) {
    m_mem->~HSAMemoryObject();
    m_mem = NULL;
  }
}

VCSection::VCSection(VCSectionType secType, uint32_t size, hsa_agent_t agent, MemoryRegionType memType)
  : m_stype(secType),
  m_size(size),
  m_mem(NULL),
  m_pos(0)
{
  if (m_size != 0) {
    m_mem = new HSAMemoryObject(m_size, agent, memType);
    m_pos = 0;
  }
  m_seperator = "======================";
}

bool VCSection::OutOfMemory(uint32_t pos)
{
  return (pos >= Number<uint32_t>());
}

void VCSection::SetValue(std::string &str)
{
  if (!IsAddr())
    return;

  str = str.substr(str.length() - 10, str.length());
  if (str.find("0x") != std::string::npos) {
    if (OutOfMemory(m_pos)) {
      std::cerr << "Out of memory size!" << std::endl;
      return;
    }
    char *end;
    m_mem->As<uint32_t*>()[m_pos++] = std::strtol((str.substr(str.find("0x"))).c_str(), &end, 16);
  }
}

std::ostream& VCSection::Print(std::ostream &out)
{
  if (!Allocated()) {
    out << "No data to print!" << std::endl;
    return out;
  }
  out << Name() << ":" << std::endl;
  out << m_seperator << std::endl;
  for (int i = 0; i < (int)Number<uint32_t>(); i++)
    out << "0x" << std::setw(8) << std::setfill('0') << std::hex << As<uint32_t*>()[i] << std::endl;

  return out;
}

std::ostream& operator<< (std::ostream &out, VCSection &sec)
{
  return sec.Print(out);
}

/**
 * free gpu memory as it's allocated by VCKernArg
 * then VCSection will not free it later, it's freed here already.
 * ~VCSection() requires virtual function, otherwise it will not be called.
 */
VCKernArg::~VCKernArg()
{
  if (m_mem) {
    m_mem->~HSAMemoryObject();
    m_mem = NULL;
  }
}

VCKernArg::VCKernArg(uint32_t index,
              uint32_t offset, bool isAddr,
              uint32_t size, hsa_agent_t agent, MemoryRegionType memType)
  : VCSection(VC_KERN_ARG, 0, agent, memType), // do not allocate memory here, handled by KernArg
  m_index(index),
  m_offset(offset),
  m_is_addr(false),
  m_dtype(VC_UINT32)
{
    m_size = size;
    if (isAddr && (m_size != 0)) {
        m_mem = new HSAMemoryObject(m_size, agent, memType);
        m_pos = 0;
        m_is_addr = true;
    } else {
        m_value = 0;
    }
}

std::ostream& VCKernArg::Print(std::ostream &out)
{
  out << m_seperator << std::endl;
  out << Name() << ":" << Index() << std::endl;
  if (IsAddr()) {
    if (!Allocated()) {
      out << "No data to print!" << std::endl;
      return out;
    }
    if (m_dtype == VC_UINT32) {
      for (int i = 0; i < (int)Number<uint32_t>(); i++)
        out << "0x" << std::setw(8) << std::setfill('0') << std::hex << As<uint32_t*>()[i] << std::endl;
    } else if (m_dtype == VC_INT) {
      for (int i = 0; i < (int)Number<uint32_t>(); i++)
        out << As<int*>()[i] << std::endl;
    } else if (m_dtype == VC_FLOAT) {
      for (int i = 0; i < (int)Number<uint32_t>(); i++)
        out << As<float*>()[i] << std::endl;
    } else if (m_dtype == VC_DOUBLE) {
      for (int i = 0; i < (int)Number<uint64_t>(); i++)
        out << As<double*>()[i] << std::endl;
    } else {
        out << "Unknown type: " << m_dtype << std::endl;
    }
  } else {
      out << Value() << std::endl;
  }

  return out;
}
