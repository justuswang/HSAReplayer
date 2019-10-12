#include "replayer.hpp"

// ==============================
// VCSection
// ==============================

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

template<typename T>
void VCSection::SetValue(T val, uint32_t offset)
{
  uint32_t pos = offset / sizeof(uint32_t);
  if (OutOfMemory(pos)) {
    std::cerr << "Out of memory size!" << std::endl;
    return;
  }
  *((T*)(m_mem->As<uint32_t*>() + pos)) = val;
}

template<typename T>
T VCSection::GetValue(uint32_t offset)
{
  uint32_t pos = offset / sizeof(uint32_t);
  if (OutOfMemory(pos)) {
    std::cerr << "Out of memory size!" << std::endl;
    return -1;
  }
  return *((T*)(m_mem->As<uint32_t*>() + pos));
}

// ==============================
// Replayer
// ==============================

Replayer::~Replayer()
{
  // clean up resource before hsa shutdown
  for (int i = 0; i < (int)m_sections.size(); i++) {
    m_sections[i]->~VCSection();
  }
}

uint64_t Replayer::GetHexValue(std::string &line, const char *key)
{
    uint64_t value = 0;
    char format[128];
    int pos = line.find(key);
    std::string temp = line.substr(pos);

    snprintf(format, sizeof(format), "%s%s", key, "=0x%lx");
    sscanf(temp.c_str(), format, &value);

    return value;
}

void Replayer::LoadVectorFile(const char *fileName, hsa_agent_t agent)
{
  std::ifstream vc_file(fileName);
  std::string line;
  VCSectionType type = VC_NULL;
  VCSection *sec = NULL;

  while (!vc_file.eof()) {
    std::getline(vc_file, line);
    if (line.find("@aql_pkt") != std::string::npos) {
      type = VC_AQL;
      sec = new VCSection(type, 64, agent, MEM_SYS);
      m_sections.push_back(sec);
    } else if (line.find("@kernel_obj") != std::string::npos) {
      type = VC_KERN_OBJ;
      sec = new VCSection(type, GetHexValue(line, "bytes"), agent, MEM_LOC);
      m_sections.push_back(sec);
    } else if (line.find("@kernel_arg_pool") != std::string::npos) {
      type = VC_KERN_ARG_POOL;
      sec = new VCSection(type,
                          GetHexValue(line, "args_num"),
                          GetHexValue(line, "bytes"), agent, MEM_KERNARG);
      m_sections.push_back(sec);
    }
    else if (line.find("@kernel_arg") != std::string::npos) {
      type = VC_KERN_ARG;
      sec = new VCSection(type,
                          GetHexValue(line, "arg_idx"),
                          GetHexValue(line, "offset"),
                          (GetHexValue(line, "addr") != 0),
                          GetHexValue(line, "bytes"), agent, MEM_SYS);
      if (sec->IsAddr()) {
        std::cout << "Kernel Arg: " << sec->Index() << std::endl;
        std::cout << "addr: 0x" << std::hex << (uint64_t)sec->As<void*>() << std::endl;
      }
      m_sections.push_back(sec);
    }
    
    if ((type == VC_NULL) && line.empty())
      continue;

    if (sec != NULL)
      sec->SetValue(line);
  }
  UpdateKernelArgPool();
}

void Replayer::UpdateKernelArgPool()
{
  VCSection *pool = GetSection(VC_KERN_ARG_POOL);

  for (int i = 0; i < (int)m_sections.size(); i++) {
      VCSection *sec = m_sections[i];
      if (sec->SType() == VC_KERN_ARG) {
          if (sec->IsAddr()) {
              pool->SetValue<uint64_t>((uint64_t)sec->As<void*>(), sec->Offset());
          } else {
              sec->SetValue(pool->GetValue<uint32_t>(sec->Offset()));
          }
      }
  }

  std::cout << "Pools: " << std::endl;
  for (size_t j = 0; j < pool->Number<uint32_t>(); j++) {
      std::cout << std::hex << pool->As<uint32_t*>()[j] << std::endl;
  }
}

VCSection* Replayer::GetSection(VCSectionType type)
{
  for (int i = 0; i < (int)m_sections.size(); i++) {
    if (m_sections[i]->SType() == type) {
        return m_sections[i];
    }
  }
  return NULL;
}

void Replayer::ShowKernelArgs()
{
  if (m_sections.size() == 0)
    return;
  for (int i = 0; i < (int)m_sections.size(); i++) {
    VCSection* sec = m_sections[i];
    if (sec->SType() == VC_KERN_ARG) {
      std::cout << sec->Name() << ":" << sec->Index() << std::endl;
      if (sec->IsAddr()) {
        for (int j = 0; j < (int)sec->Number<uint32_t>(); j++)
          std::cout << sec->As<float*>()[j] << std::endl;
      } else {
          std::cout << sec->Value() << std::endl;
      }
    }
  }
}

void Replayer::ShowSection(VCSectionType type)
{
  if (type == VC_KERN_ARG)
    ShowKernelArgs();
}

int main(int argc, char **argv)
{
  HSAInit hsaInit;

  HSAAgent agent(HSA_DEVICE_TYPE_GPU);
  Replayer replayer;

  replayer.LoadVectorFile("/home/zjunwei/tmp/clang_vectoradd_co_v10.rpl", agent.Get());

  hsa_kernel_dispatch_packet_t packet;
  HSAQueue queue(agent.Get(), 64, HSA_QUEUE_TYPE_SINGLE);

  VCSection *sec_aql = replayer.GetSection(VC_AQL);
  VCSection *sec_kern_obj = replayer.GetSection(VC_KERN_OBJ);
  VCSection *sec_pool = replayer.GetSection(VC_KERN_ARG_POOL);

  auto init_pkg_vector_add = [&]() {
    memcpy(&packet, sec_aql->As<uchar*>(), sizeof(packet));
    packet.kernarg_address = sec_pool->As<void*>();
    packet.kernel_object = (uint64_t)sec_kern_obj->As<void*>();
  };

  init_pkg_vector_add();
  queue.SubmitPacket(packet);

  replayer.ShowSection(VC_KERN_ARG);

  return 0;
}
