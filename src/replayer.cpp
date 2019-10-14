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

void Replayer::LoadVectorFile(const char *fileName)
{
  std::ifstream vc_file(fileName);
  std::string line;
  VCSectionType type = VC_NULL;
  VCSection *sec = NULL;

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
      m_sections.push_back(sec);
    }
    else if (line.find("@kernel_arg") != std::string::npos) {
      type = VC_KERN_ARG;
      sec = new VCKernArg(GetHexValue(line, "arg_idx"),
                          GetHexValue(line, "offset"),
                          (GetHexValue(line, "addr") != 0),
                          GetHexValue(line, "bytes"), m_agent, MEM_SYS);
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

void Replayer::CreateQueue(uint32_t size, hsa_queue_type32_t type)
{
  if (m_queue != NULL)
    return;
  m_queue = new HSAQueue(m_agent, size, type);
}

void Replayer::SubmitPacket(void)
{
  hsa_kernel_dispatch_packet_t packet;

  VCSection *sec_aql = GetSection(VC_AQL);
  VCSection *sec_kern_obj = GetSection(VC_KERN_OBJ);
  VCSection *sec_pool = GetSection(VC_KERN_ARG_POOL);

  memcpy(&packet, sec_aql->As<uchar*>(), sizeof(packet));
  packet.kernarg_address = sec_pool->As<void*>();
  packet.kernel_object = (uint64_t)sec_kern_obj->As<void*>();

  m_queue->SubmitPacket(packet);
}
