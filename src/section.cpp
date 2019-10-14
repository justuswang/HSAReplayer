#include "section.hpp"

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
}

bool VCSection::OutOfMemory(uint32_t pos)
{
  return (pos >= Number<uint32_t>());
}

void VCSection::SetValue(std::string &str)
{
  if (!IsAddr())
    return;

  if (str.rfind("0x", 2) != std::string::npos) {
    if (OutOfMemory(m_pos)) {
      std::cerr << "Out of memory size!" << std::endl;
      return;
    }
    char *end;
    m_mem->As<uint32_t*>()[m_pos++] = std::strtol(str.c_str(), &end, 16);
  }
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
  m_is_addr(false)
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
