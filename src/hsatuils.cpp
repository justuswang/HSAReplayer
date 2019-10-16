#include "hsautils.hpp"

// ================================================================================
// HSA Agent
// ================================================================================

HSAAgent::HSAAgent(hsa_device_type_t type)
{
  m_agent = GetDefaultDevice(type);
}

hsa_agent_t HSAAgent::GetDefaultDevice(hsa_device_type_t target_device)
{
  auto get_gpu = [](hsa_agent_t agent, void *data) {
      hsa_device_type_t device_type;
      EXPECT_SUCCESS(hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type));

      if (device_type == HSA_DEVICE_TYPE_GPU) {
          hsa_agent_t *p = (hsa_agent_t*)data;
          *p = agent;
          return HSA_STATUS_INFO_BREAK;
      }
      return HSA_STATUS_SUCCESS;
  };
  auto get_cpu = [](hsa_agent_t agent, void *data) {
      hsa_device_type_t device_type;
      EXPECT_SUCCESS(hsa_agent_get_info(agent, HSA_AGENT_INFO_DEVICE, &device_type));

      if (device_type == HSA_DEVICE_TYPE_GPU) {
          hsa_agent_t *p = (hsa_agent_t*)data;
          *p = agent;
          return HSA_STATUS_INFO_BREAK;
      }
      return HSA_STATUS_SUCCESS;
  };
  hsa_agent_t agent;
  EXPECT_FOUND(hsa_iterate_agents((target_device == HSA_DEVICE_TYPE_GPU) ? get_gpu : get_cpu,
                                  &agent));
  return agent;
}

// ================================================================================
// HSA Memory Object
// ================================================================================

HSAMemoryObject::HSAMemoryObject(size_t size, hsa_agent_t agent, MemoryRegionType type)
  : m_size(size),
  m_agent(agent),
  m_type(type),
  m_ptr(NULL)
{
  hsa_status_t status;

  status = hsa_agent_iterate_regions(m_agent,
                                      get_memory_region_cb(m_type),
                                      &m_region);
  EXPECT_FOUND(status);
  
  status = hsa_memory_allocate(m_region, size, (void**)&m_ptr);
  EXPECT_SUCCESS(status);
}

HSAMemoryObject::~HSAMemoryObject()
{
  EXPECT_SUCCESS(hsa_memory_free(m_ptr));
  m_ptr = NULL;
}

hsa_status_t HSAMemoryObject::get_local_memory_cb(hsa_region_t region, void *data)
{
  hsa_region_segment_t segment;
  bool host_acc = false;
  hsa_region_t *p = (hsa_region_t*)data;

  EXPECT_SUCCESS(hsa_region_get_info(region,
                                     HSA_REGION_INFO_SEGMENT,
                                     &segment));
  if (segment != HSA_REGION_SEGMENT_GLOBAL)
    goto out;

  EXPECT_SUCCESS(hsa_region_get_info(region,
                                     (hsa_region_info_t)HSA_AMD_REGION_INFO_HOST_ACCESSIBLE,
                                     &host_acc));
  if (host_acc)
    goto out;

  *p = region;

  return HSA_STATUS_INFO_BREAK;

out:
  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSAMemoryObject::get_system_memory_cb(hsa_region_t region, void *data)
{
  hsa_region_segment_t segment;
  bool host_acc = false;
  hsa_region_global_flag_t flags;
  hsa_region_t *p = (hsa_region_t*)data;

  EXPECT_SUCCESS(hsa_region_get_info(region,
                                     HSA_REGION_INFO_SEGMENT,
                                     &segment));
  if (segment != HSA_REGION_SEGMENT_GLOBAL)
    goto out;

  EXPECT_SUCCESS(hsa_region_get_info(region,
                                     (hsa_region_info_t)HSA_AMD_REGION_INFO_HOST_ACCESSIBLE,
                                     &host_acc));
  if (!host_acc)
    goto out;

  EXPECT_SUCCESS(hsa_region_get_info(region,
                                     HSA_REGION_INFO_GLOBAL_FLAGS,
                                     &flags));
  if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG)
    goto out;

  *p = region;

  return HSA_STATUS_INFO_BREAK;

out:
  return HSA_STATUS_SUCCESS;
}

hsa_status_t HSAMemoryObject::get_kernarg_memory_cb(hsa_region_t region, void *data)
{
  hsa_region_global_flag_t flags;
  hsa_region_t *p = (hsa_region_t*)data;

  EXPECT_SUCCESS(hsa_region_get_info(region,
                                    HSA_REGION_INFO_GLOBAL_FLAGS,
                                    &flags));
  if (flags & HSA_REGION_GLOBAL_FLAG_KERNARG)
    return HSA_STATUS_SUCCESS;

  *p = region;
  return HSA_STATUS_INFO_BREAK;
}

// ================================================================================
// HSA Signal
// ================================================================================

HSASignal::HSASignal(hsa_signal_value_t value)
{
  EXPECT_SUCCESS(hsa_signal_create(value, 0, NULL, &m_signal));
}

void HSASignal::Wait(hsa_signal_condition_t condition, hsa_signal_value_t value)
{
  while (hsa_signal_wait_scacquire(m_signal,
                                   condition, value,
                                   UINT64_MAX,
                                   HSA_WAIT_STATE_ACTIVE));
}
HSASignal::~HSASignal(void)
{
  EXPECT_SUCCESS(hsa_signal_destroy(m_signal));
}

// ================================================================================
// HSA executable
// ================================================================================

HSAExecutable::HSAExecutable(hsa_agent_t agent,
                  hsa_profile_t profile,
                  hsa_default_float_rounding_mode_t rounding_mode)
  : m_agent(agent),
    m_profile(profile),
    m_round_mode(rounding_mode)
{
  m_executable.handle = 0;
  EXPECT_SUCCESS(hsa_executable_create_alt(m_profile,
                                     m_round_mode,
                                     NULL, // options
                                     &m_executable));
}

HSAExecutable::~HSAExecutable(void)
{
  if (m_executable.handle != 0)
    EXPECT_SUCCESS(hsa_executable_destroy(m_executable));
  else
    return;
}

hsa_status_t HSAExecutable::LoadCodeObject(const char *fileName, const char *kernelName)
{
  hsa_status_t status;
  hsa_code_object_reader_t reader;

  m_fileName = fileName;
  m_kernelName = kernelName;
  std::cout << "Code object file: " << m_fileName << std::endl;
  std::cout << "Kernel name: " << m_kernelName << std::endl;

  int fd = open(m_fileName.c_str(), O_RDONLY);
  if (fd <= 0) {
    std::cout << "Failed to open file: " << m_fileName << std::endl;
    return HSA_STATUS_ERROR;
  }

  EXPECT_SUCCESS(hsa_code_object_reader_create_from_file(fd, &reader));
  close(fd);

  status = hsa_executable_load_agent_code_object(m_executable, m_agent,
                                        reader,
                                        NULL, // options
                                        NULL); // loaded_code_object
  EXPECT_SUCCESS(status);

  status = hsa_code_object_reader_destroy(reader);
  EXPECT_SUCCESS(status);

  status = hsa_executable_freeze(m_executable, NULL/* options */);
  EXPECT_SUCCESS(status);

  status = hsa_executable_get_symbol_by_name(m_executable,
                                             m_kernelName.c_str(),
                                             &m_agent,
                                             &m_symbol);
  EXPECT_SUCCESS(status);
  return status;
}

// ================================================================================
// HSA Queue
// ================================================================================

HSAQueue::HSAQueue(hsa_agent_t agent, uint32_t size, hsa_queue_type32_t type)
  : m_agent(agent),
  m_size(size),
  m_type(type),
  m_queue(NULL),
  m_executable(agent)
{
  EXPECT_SUCCESS(hsa_queue_create(m_agent, m_size, m_type,
                                  NULL, NULL,
                                  UINT32_MAX,   // private_segment_size
                                  UINT32_MAX,   // group_segment_size
                                   &m_queue)
                );
}

HSAQueue::~HSAQueue(void)
{
  if (m_queue) {
    EXPECT_SUCCESS(hsa_queue_destroy(m_queue));
    m_queue = NULL;
  }
}

hsa_status_t HSAQueue::LoadCodeObject(const char *fileName, const char *kernelName)
{
  hsa_status_t status;

  status = m_executable.LoadCodeObject(fileName, kernelName);
  EXPECT_SUCCESS(status);

  return status;
}

hsa_status_t HSAQueue::SubmitPacket(hsa_kernel_dispatch_packet_t &pkt)
{
  uint64_t index = hsa_queue_add_write_index_screlease(m_queue, 1);
  hsa_status_t status = HSA_STATUS_SUCCESS;

  hsa_kernel_dispatch_packet_t *packet = (hsa_kernel_dispatch_packet_t*)m_queue->base_address + index % m_size;
  memcpy(packet, &pkt, sizeof(pkt));

  // Create a complete signal for the packet
  HSASignal complete(1);
  packet->completion_signal = complete.Get();

  uint16_t header = 0;
  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_ACQUIRE_FENCE_SCOPE;
  header |= HSA_FENCE_SCOPE_SYSTEM << HSA_PACKET_HEADER_RELEASE_FENCE_SCOPE; // if wait signal
  header |= HSA_PACKET_TYPE_KERNEL_DISPATCH << HSA_PACKET_HEADER_TYPE;
  __atomic_store_n((uint16_t*)(&packet->header), header, __ATOMIC_RELEASE);

  // Submit the packet to GPU
  hsa_signal_store_screlease(m_queue->doorbell_signal, index);

  complete.Wait(HSA_SIGNAL_CONDITION_EQ, 0);

  return status;
}