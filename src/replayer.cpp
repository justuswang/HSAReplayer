#include "replayer.hpp"

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

int Replayer::LoadHsacoFile(const char *fileName)
{
  m_executable->LoadCodeObject(fileName, "vector_add");
  return 0;
}

int Replayer::LoadData(const char *fileName)
{
  int ret = 0;
  auto SetMode = [&] () -> int {
    std::string str(fileName);
    if (str.rfind(".hsaco") != std::string::npos) {
      std::cout << "load .hsaco" << std::endl;
      m_mode = RE_HSACO;
    }
    else if (str.rfind(".rpl") != std::string::npos) {
      std::cout << "load .rpl" << std::endl;
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
    ret = LoadHsacoFile(fileName);
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

void Replayer::SubmitPacket(void)
{
  if (m_mode == RE_VC)
    VCSubmitPacket();
  else if (m_mode == RE_HSACO)
    HsacoSubmitPacket();
  else
    std::cerr << "Unknown replay mode!" << std::endl;
}

void Replayer::HsacoSubmitPacket(void)
{
  const int V_LEN = 16;
  hsa_kernel_dispatch_packet_t packet;

  HSAMemoryObject in_A(V_LEN * sizeof(float), m_agent, MEM_SYS);
  HSAMemoryObject in_B(V_LEN * sizeof(float), m_agent, MEM_SYS);
  HSAMemoryObject in_C(V_LEN * sizeof(float), m_agent, MEM_SYS);
  for (int i = 0; i < V_LEN; i++) in_A.As<float*>()[i] = i;
  in_B.Fill<float>((float)1.0);
  in_C.Fill<float>((float)0.0);

  auto init_pkg_vector_add = [&]() {
    packet.setup |= 1 << HSA_KERNEL_DISPATCH_PACKET_SETUP_DIMENSIONS;
    packet.workgroup_size_x = (uint16_t)V_LEN;
    packet.workgroup_size_y = (uint16_t)1;
    packet.workgroup_size_z = (uint16_t)1;
    packet.grid_size_x = (uint16_t)V_LEN;
    packet.grid_size_y = (uint16_t)1;
    packet.grid_size_z = (uint16_t)1;

    // allocate kernel arguments from kernarg region
    // they can be queried one by one as below format
    struct kern_args_t {
      float *in_A;
      float *in_B;
      float *in_C;
    };
    HSAMemoryObject args(sizeof(struct kern_args_t), m_agent, MEM_KERNARG);
    args.As<struct kern_args_t*>()->in_A = in_A.As<float*>();
    args.As<struct kern_args_t*>()->in_B = in_B.As<float*>();
    args.As<struct kern_args_t*>()->in_C = in_C.As<float*>();

    packet.kernarg_address = args.As<void*>();

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

  init_pkg_vector_add();
  m_queue->SubmitPacket(packet);

  for (int i = 0; i < V_LEN; i++) {
    std::cout << in_C.As<float*>()[i] << std::endl;
  }
}

void Replayer::SubmitPacket(HsacoAql *aql, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs)
{
  if (m_mode == RE_VC)
    VCSubmitPacket();
  else if (m_mode == RE_HSACO)
    HsacoSubmitPacket(aql, kernArgs);
  else
    std::cerr << "Unknown replay mode!" << std::endl;
}

void Replayer::HsacoSubmitPacket(HsacoAql *aql, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs)
{
  const int V_LEN = 16;
  hsa_kernel_dispatch_packet_t packet = { 0 };
  std::vector<std::unique_ptr<HsacoKernArg>> kArgs;

  auto type_size = [] (VCDataType &type) -> int {
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
  };

  auto fill_mem = [] (HSAMemoryObject *mem, VCDataType type, uint64_t value) {
    if (type == VC_INT)
      mem->Fill<int>((int)value);
    else if (type == VC_FLOAT)
      mem->Fill<float>((float)value);
    else if (type == VC_DOUBLE)
      mem->Fill<double>((double)value);
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

  for (int i = 0; i < (int)kernArgs->size(); i++) {
    if (kernArgs->at(i).get()->sType == HC_ADDR) {
      HsacoKernArg *kArg = new HsacoKernArg();
      kArg->mem.reset(new HSAMemoryObject(
                     kernArgs->at(i).get()->size * type_size(kernArgs->at(i).get()->dType), m_agent, MEM_SYS));
      kArg->mem.get()->Fill<float>(1.1);
      kArgs.push_back(std::unique_ptr<HsacoKernArg>(kArg));
    } else if (kernArgs->at(i).get()->sType == HC_VALUE){
      HsacoKernArg *kArg = new HsacoKernArg();
      kArg->value = 10;
      kArgs.push_back(std::unique_ptr<HsacoKernArg>(kArg));
    }
  }
  std::cout << "size of kArgs: " << kArgs.size() << std::endl;

  std::unique_ptr<HSAMemoryObject> mem_kernArgs(new HSAMemoryObject(16 * sizeof(uint64_t), m_agent, MEM_KERNARG));

  HSAMemoryObject in_A(V_LEN * sizeof(float), m_agent, MEM_SYS);
  HSAMemoryObject in_B(V_LEN * sizeof(float), m_agent, MEM_SYS);
  HSAMemoryObject in_C(V_LEN * sizeof(float), m_agent, MEM_SYS);
  for (int i = 0; i < V_LEN; i++) in_A.As<float*>()[i] = i;
  in_B.Fill<float>((float)1.0);
  in_C.Fill<float>((float)0.0);

  int num_of_kernargs = kArgs.size();
  HSAMemoryObject args(sizeof(uint64_t) * num_of_kernargs, m_agent, MEM_KERNARG);

  auto set_kernarg_addr = [&](char **addr, HSAMemoryObject* mem) {
    *((uint64_t*)(*addr)) = (uint64_t)mem->As<void*>();
    *addr += sizeof(uint64_t);
  };

  auto init_pkg_kernarg_addr = [&]() {
    char *p = args.As<char*>();
    set_kernarg_addr(&p, &in_A);
    set_kernarg_addr(&p, &in_B);
    set_kernarg_addr(&p, &in_C);

    p = mem_kernArgs.get()->As<char*>();

    for (size_t i = 0; i < kArgs.size(); ++i){
      set_kernarg_addr(&p, kArgs[i]->mem.get());
    }
    //set_kernarg_addr(&p, &in_A);
    //set_kernarg_addr(&p, &in_B);
    //set_kernarg_addr(&p, &in_C);

    // debug
    for (int i = 0; i < 10; i++) {
      std::cout << "0x" << std::setw(8) << std::setfill('0') << std::hex << args.As<uint32_t*>()[i] << std::endl;
    }
    for (int i = 0; i < 10; i++)
      std::cout << "0x" << std::setw(8) << std::setfill('0') << std::hex << mem_kernArgs.get()->As<uint32_t*>()[i] << std::endl;
    //packet.kernarg_address = args.As<void*>();
    packet.kernarg_address = mem_kernArgs.get()->As<void*>();
  };

  auto init_pkg_vector_add = [&]() {
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
  init_pkg_kernarg_addr();
  init_pkg_vector_add();
  m_queue->SubmitPacket(packet);

  for (int i = 0; i < V_LEN; i++) {
    std::cout << kArgs[2]->mem.get()->As<float*>()[i] << std::endl;
  }
  //for (int i = 0; i < V_LEN; i++) {
  //  std::cout << in_C.As<float*>()[i] << std::endl;
  //}
}
