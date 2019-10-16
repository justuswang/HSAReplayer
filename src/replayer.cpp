#include "replayer.hpp"

Replayer::Replayer()
  : m_queue(NULL)
{
  HSAAgent agent;
  m_agent = agent.Get();
}

Replayer::~Replayer()
{
  // clean up resource before hsa shutdown
  for (int i = 0; i < (int)m_sections.size(); i++) {
    m_sections[i]->~VCSection();
  }
  if (m_queue) {
    m_queue->~HSAQueue();
    m_queue = NULL;
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

int Replayer::LoadVectorFile(const char *fileName)
{
  auto SanityCheck = [&]() {
    std::ifstream inputFile(fileName);
    VCSectionType type = VC_NULL;
    std::string line;
    if (!inputFile.is_open()) {
      std::cerr << "Failed to open file: " << fileName << std::endl;
      return -1;
    }
    while (!inputFile.eof()) {
      std::getline(inputFile, line);
      if (line.find("@aql_pkt") != std::string::npos)
        type = VC_AQL;
    }
    if (type == VC_AQL)
      return 0;
    else {
      std::cerr << "Unsupported format!" << std::endl;
      return -1;
    }
  };

  std::ifstream vc_file(fileName);
  std::string line;
  VCSectionType type = VC_NULL;
  VCSection *sec = NULL;

  if (SanityCheck() != 0)
      return -1;
  while (!vc_file.eof()) {
    std::getline(vc_file, line);
    if (line.find("@aql_pkt") != std::string::npos) {
      type = VC_AQL;
      sec = new VCSection(type, 64, m_agent, MEM_SYS);
      m_sections.push_back(sec);
    } else if (line.find("@kernel_obj") != std::string::npos) {
      type = VC_KERN_OBJ;
      sec = new VCSection(type, GetHexValue(line, "bytes"), m_agent, MEM_LOC);
      m_sections.push_back(sec);
    } else if (line.find("@kernel_arg_pool") != std::string::npos) {
      type = VC_KERN_ARG_POOL;
      sec = new VCSection(type, GetHexValue(line, "bytes"), m_agent, MEM_KERNARG);
      sec->SetArgsNum(GetHexValue(line, "args_num"));
      m_sections.push_back(sec);
    }
    else if (line.find("@kernel_arg") != std::string::npos) {
      type = VC_KERN_ARG;
      sec = new VCKernArg(GetHexValue(line, "arg_idx"),
                          GetHexValue(line, "offset"),
                          (GetHexValue(line, "addr") != 0),
                          GetHexValue(line, "bytes"), m_agent, MEM_SYS);
      //if (sec->IsAddr()) {
      //  std::cout << "Kernel Arg: " << sec->Index() << std::endl;
      //  std::cout << "addr: 0x" << std::hex << (uint64_t)sec->As<void*>() << std::endl;
      //}
      m_sections.push_back(sec);
    }
    
    if ((type == VC_NULL) && line.empty())
      continue;

    if (sec != NULL)
      sec->SetValue(line);
  }

  UpdateKernelArgPool();
  return 0;
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

  //std::cout << "Pools: " << std::endl;
  //for (size_t j = 0; j < pool->Number<uint32_t>(); j++) {
  //    std::cout << std::hex << pool->As<uint32_t*>()[j] << std::endl;
  //}
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

VCSection* Replayer::GetKernArg(int index)
{
  for (int i = 0; i < (int)m_sections.size(); i++) {
    VCSection* sec = m_sections[i];
    if ((sec->SType() == VC_KERN_ARG) && (sec->Index() == (uint32_t)index))
      return sec;
  }
  return NULL;
}

// for debug to show a specific kernel arg data
void Replayer::PrintKernArg(int index, KernArgDataType argsType)
{
  if (argsType >= KA_MAX) {
    std::cerr << "Invalid kernel argement type!" << std::endl;
    return;
  }
  if ((uint32_t)index >= GetSection(VC_KERN_ARG_POOL)->ArgsNum()) {
    std::cerr << "Invalid kernel argement type!" << std::endl;
    return;
  }
  std::cout << *(GetKernArg(index)) << std::endl;
}

void Replayer::SetDataTypes(std::vector<KernArgDataType> *argsType)
{
  if (argsType == NULL) {
    return;
  }
  if (argsType->size() < GetSection(VC_KERN_ARG_POOL)->ArgsNum()) {
    std::cerr << "Invalid kernel argement type list!" << std::endl;
    return;
  }
  for (int i = 0; i < (int)m_sections.size(); i++) {
    VCSection* sec = m_sections[i];
    if (sec->SType() == VC_KERN_ARG)
      sec->SetDataType(argsType->at(sec->Index()));
  }
}

void Replayer::PrintKernArgs()
{
  for (int i = 0; i < (int)m_sections.size(); i++) {
    VCSection* sec = m_sections[i];
    if (sec->SType() == VC_KERN_ARG)
      std::cout << *sec << std::endl;
  }
}

void Replayer::PrintSection(VCSectionType type)
{
  if ((type == VC_NULL) || (m_sections.size() == 0))
    return;

  if (type == VC_KERN_ARG) {
    PrintKernArgs();
  } else if (type == VC_TYPE_MAX) {
    for (int i = 0; i < (int)m_sections.size(); i++) {
        std::cout << *(m_sections[i]) << std::endl;
    }
  } else {
    std::cout << *GetSection(type) << std::endl;
  }
}

void Replayer::CreateQueue(uint32_t size, hsa_queue_type32_t type)
{
  if (m_queue != NULL)
    return;
  m_queue = new HSAQueue(m_agent, size, type);
}

void Replayer::SubmitPacket(void)
{
  hsa_kernel_dispatch_packet_t packet;

  memcpy(&packet, GetSection(VC_AQL)->As<uchar*>(), sizeof(packet));
  packet.kernarg_address = GetSection(VC_KERN_ARG_POOL)->As<void*>();
  packet.kernel_object = (uint64_t)GetSection(VC_KERN_OBJ)->As<void*>();

  m_queue->SubmitPacket(packet);
}
