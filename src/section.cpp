#include "section.hpp"

VCSection::~VCSection()
{
  if (m_mem)
    m_mem->~HSAMemoryObject();
}

VCSection::VCSection(VCSectionType secType, uint32_t size, hsa_agent_t agent, MemoryRegionType memType)
  : m_stype(secType),
  m_size(size),
  m_mem(NULL),
  m_pos(0),
  m_is_addr(true)
{
  if (m_size != 0) {
    m_mem = new HSAMemoryObject(m_size, agent, memType);
    m_pos = 0;
  }
}

VCSection::VCSection(VCSectionType secType, uint32_t num, uint32_t size, hsa_agent_t agent, MemoryRegionType memType)
  : m_stype(secType),
  m_arg_total_index(num),
  m_size(size),
  m_mem(NULL),
  m_pos(0),
  m_is_addr(true)
{
  if (m_size != 0) {
    m_mem = new HSAMemoryObject(m_size, agent, memType);
    m_pos = 0;
  }
}

VCSection::VCSection(VCSectionType secType, uint32_t index,
              uint32_t offset, bool isAddr,
              uint32_t size, hsa_agent_t agent, MemoryRegionType memType)
  : m_stype(secType),
  m_arg_index(index),
  m_arg_offset(offset),
  m_size(size),
  m_mem(NULL),
  m_is_addr(false)
{
    if (isAddr && (m_size != 0)) {
        m_mem = new HSAMemoryObject(m_size, agent, memType);
        m_pos = 0;
        m_is_addr = true;
    } else {
        m_arg_value = 0;
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
