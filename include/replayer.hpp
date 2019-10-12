#ifndef __REPLAYER_H__
#define __REPLAYER_H__

#include <iostream>
#include <fstream>
#include <vector>

#include "hsautils.hpp"
#include "section.hpp"

class Replayer {
public:
  Replayer();
  ~Replayer();

  void LoadVectorFile(const char *fileName);
  void UpdateKernelArgPool();

  uint64_t GetHexValue(std::string &line, const char *key);
  VCSection* GetSection(VCSectionType type);
  void ShowSection(VCSectionType type);

  void CreateQueue(uint32_t size, hsa_queue_type32_t type);
  void SubmitPacket(void);

private:
  void ShowKernelArgs(void);

  std::vector<VCSection*> m_sections;
  hsa_agent_t m_agent;
  HSAQueue *m_queue;
};

#endif /* __REPLAYER_H__ */