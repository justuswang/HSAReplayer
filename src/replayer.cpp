#include "replayer.hpp"
#include <typeinfo>

Replayer::Replayer()
  : m_executable(NULL),
  m_queue(NULL)
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
  if (m_executable) {
    delete m_executable;
    m_executable = NULL;
  }
  if (m_queue) {
    delete m_queue;
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
      if (line.find("@aql_pkt") != std::string::npos) {
        type = VC_AQL;
        break;
      }
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
#if 0
      if (sec->IsAddr()) {
        std::cout << "Kernel Arg: " << sec->Index() << std::endl;
        std::cout << "addr: 0x" << std::hex << (uint64_t)sec->As<void*>() << std::endl;
      }
#endif
      m_sections.push_back(sec);
    }
    
    if ((type == VC_NULL) || line.empty() || (line.find("@") != std::string::npos))
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
#if 0
  std::cout << "Pools: " << std::endl;
  for (size_t j = 0; j < pool->Number<uint32_t>(); j++) {
      std::cout << std::hex << pool->As<uint32_t*>()[j] << std::endl;
  }
#endif
}

int Replayer::LoadHsacoFile(const char *fileName, const char *symbol)
{
  m_executable->LoadCodeObject(fileName, symbol);
  return 0;
}

int Replayer::LoadData(const char *fileName, const char *symbol)
{
  int ret = 0;
  auto SetMode = [&] () -> int {
    std::string str(fileName);
    if (str.rfind(".hsaco") != std::string::npos) {
      DBG("load .hsaco");
      m_mode = RE_HSACO;
    }
    else if (str.rfind(".rpl") != std::string::npos) {
      DBG("load .rpl");
      m_mode = RE_VC;
    } else {
      return -1;
    }
    return 0;
  };
  if (SetMode() != 0) {
    std::cerr << "Unknown type input file!" << std::endl;
    return -1;
  }

  if (m_mode == RE_VC) {
    ret = LoadVectorFile(fileName);
  } else {
    ret = LoadHsacoFile(fileName, symbol);
  }

  return ret;
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

// for debug to show a specific kernel arg data
void Replayer::PrintKernArg(int index)
{
  if ((uint32_t)index >= GetSection(VC_KERN_ARG_POOL)->ArgsNum()) {
    std::cerr << "Invalid kernel argement type!" << std::endl;
    return;
  }
  for (int i = 0; i < (int)m_sections.size(); i++) {
    VCSection* sec = m_sections[i];
    if ((sec->SType() == VC_KERN_ARG) && (sec->Index() == (uint32_t)index))
      std::cout << *sec << std::endl;
  }
}

void Replayer::SetDataTypes(std::vector<VCDataType> *argsType)
{
  if (!IsVCMode())
    return;
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
  if (!IsVCMode())
    return;
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
  if (m_executable == NULL)
    m_executable = new HSAExecutable(m_agent);

  if (m_queue == NULL)
    m_queue = new HSAQueue(m_agent, size, type);
}

void Replayer::VCSubmitPacket(void)
{
  hsa_kernel_dispatch_packet_t packet;

  memcpy(&packet, GetSection(VC_AQL)->As<uchar*>(), sizeof(packet));
  packet.kernarg_address = GetSection(VC_KERN_ARG_POOL)->As<void*>();
  packet.kernel_object = (uint64_t)GetSection(VC_KERN_OBJ)->As<void*>();

  m_queue->SubmitPacket(packet);
}

void Replayer::SubmitPacket(JsonKernObj *aql, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs)
{
  if (m_mode == RE_VC)
    VCSubmitPacket();
  else if (m_mode == RE_HSACO)
    HsacoSubmitPacket(aql, kernArgs);
  else
    std::cerr << "Unknown replay mode!" << std::endl;
}

std::ostream& HsacoKernArg::Print(std::ostream &out)
{
  if (sType == HC_ADDR) {
    out << *(mem.get()) << std::endl;
  } else {
    if (dType == VC_INT)
      out << value.i << std::endl;
    else if (dType == VC_FLOAT)
      out << value.f << std::endl;
    else if (dType == VC_DOUBLE)
      out << value.d << std::endl;
  }
  return out;
}

void Replayer::HsacoSubmitPacket(JsonKernObj *aql, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs)
{
  if (aql == NULL || kernArgs == NULL || kernArgs->size() == 0) {
    std::cerr << "Invalid parameters!" << std::endl;
    return;
  }
  hsa_kernel_dispatch_packet_t packet = { 0 };
  std::vector<std::unique_ptr<HsacoKernArg>> kArgs;

  auto init_kern_args_value = [&]() {
    for (int i = 0; i < (int)kernArgs->size(); i++) {
      HsacoKernArg *kArg = new HsacoKernArg();
      kArg->sType = kernArgs->at(i).get()->sType;
      kArg->dType = kernArgs->at(i).get()->dType;

      if (kernArgs->at(i).get()->sType == HC_ADDR) {
        kArg->mem.reset(new HSAMemoryObject(
                      kernArgs->at(i).get()->size * SizeOfType(kernArgs->at(i).get()->dType), m_agent, MEM_SYS));
        kArg->mem.get()->Fill(kernArgs->at(i).get());
        kArg->mem.get()->SetDataType(kernArgs->at(i).get()->dType);
      } else if (kernArgs->at(i).get()->sType == HC_VALUE){
        memcpy(&kArg->value, &kernArgs->at(i).get()->value, sizeof(kArg->value));
      }
      kArgs.push_back(std::unique_ptr<HsacoKernArg>(kArg));
    }
    DBG("size of kArgs: " << kArgs.size());
  };

  int num_of_kernargs = kernArgs->size();
  std::unique_ptr<HSAMemoryObject> mem_kernArgs(new HSAMemoryObject(num_of_kernargs * sizeof(uint64_t), m_agent, MEM_KERNARG));

  auto init_pkg_kernarg_addr = [&]() {
    auto set_kernarg_addr = [](char **addr, HSAMemoryObject* mem) {
      *((uint64_t*)(*addr)) = (uint64_t)mem->As<void*>();
      *addr += sizeof(uint64_t);
    };
    auto set_kernarg_val = [](char **addr, HsacoKernArg* ka) {
      if (ka->dType == VC_INT) {
        *((int*)(*addr)) = ka->value.i;
        *addr += sizeof(int);
      } else if (ka->dType == VC_FLOAT) {
        *((float*)(*addr)) = ka->value.f;
        *addr += sizeof(float);
      } else if (ka->dType == VC_DOUBLE) {
        *((double*)(*addr)) = ka->value.d;
        *addr += sizeof(double);
      }
    };
    char *p = mem_kernArgs.get()->As<char*>();
    for (size_t i = 0; i < kArgs.size(); ++i){
      if (kArgs[i].get()->sType == HC_ADDR) {
        set_kernarg_addr(&p, kArgs[i]->mem.get());
      } else if (kArgs[i].get()->sType == HC_VALUE) {
        set_kernarg_val(&p, kArgs[i].get());
      }
    }
    DBG("generated kernel args:");
    for (size_t i = 0; i < mem_kernArgs.get()->Size(); i++) {
      DBG("0x" << std::setw(8) << std::setfill('0') << std::hex <<
          mem_kernArgs.get()->As<uint32_t*>()[i] << std::dec << std::setw(0));
    }
    packet.kernarg_address = mem_kernArgs.get()->As<void*>();
  };

  auto init_pkg_dim = [&] () {
    packet.setup = aql->dim << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    packet.workgroup_size_x = (uint16_t)aql->workgroup_size_x;
    packet.workgroup_size_y = (uint16_t)aql->workgroup_size_y;
    packet.workgroup_size_z = (uint16_t)aql->workgroup_size_z;
    packet.grid_size_x = (uint16_t)aql->grid_size_x;
    packet.grid_size_y = (uint16_t)aql->grid_size_y;
    packet.grid_size_z = (uint16_t)aql->grid_size_z;
  };

  auto init_pkg_kernel_object = [&]() {
    hsa_status_t status;
    status = hsa_executable_symbol_get_info(m_executable->GetSymbol(),
                                    HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_OBJECT,
                                    &packet.kernel_object);
    EXPECT_SUCCESS(status);
    status = hsa_executable_symbol_get_info(m_executable->GetSymbol(),
                                    HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_PRIVATE_SEGMENT_SIZE,
                                    &packet.private_segment_size);
    EXPECT_SUCCESS(status);
    status = hsa_executable_symbol_get_info(m_executable->GetSymbol(),
                                    HSA_EXECUTABLE_SYMBOL_INFO_KERNEL_GROUP_SEGMENT_SIZE,
                                    &packet.group_segment_size);
    EXPECT_SUCCESS(status);
  };

  init_pkg_dim();
  init_kern_args_value();
  init_pkg_kernarg_addr();
  init_pkg_kernel_object();
  m_queue->SubmitPacket(packet);

  for (size_t i = 0; i < kArgs.size(); ++i) {
    std::cout << "kernel arg " << i << ":" << std::endl;
    std::cout << *(kArgs[i].get());
  }
}
