#ifndef __REPLAYER_H__
#define __REPLAYER_H__

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>

#include "hsautils.hpp"
#include "section.hpp"

enum ReplayMode {
  RE_VC = 1,
  RE_HSACO,
};


class Replayer {
public:
  Replayer();
  ~Replayer();

  int LoadData(const char *fileName);
  void UpdateKernelArgPool();

  uint64_t GetHexValue(std::string &line, const char *key);
  VCSection* GetSection(VCSectionType type);
  void PrintSection(VCSectionType type);
  void PrintKernArgs(void);
  void PrintKernArg(int index);
  void SetDataTypes(std::vector<VCDataType> *argsType);

  void CreateQueue(uint32_t size, hsa_queue_type32_t type);
  void SubmitPacket(void);
  void SubmitPacket(HsacoAql *aql, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs);

private:
  int LoadVectorFile(const char *fileName);
  int LoadHsacoFile(const char *fileName);
  void VCSubmitPacket(void);
  void HsacoSubmitPacket(void);
  //void HsacoSubmitPacket(HsacoAql *aql);
  void HsacoSubmitPacket(HsacoAql *aql, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs);
  bool IsVCMode(void) { return m_mode == RE_VC; }

  std::vector<VCSection*> m_sections;
  hsa_agent_t m_agent;
  HSAExecutable *m_executable;
  HSAQueue *m_queue;
  ReplayMode m_mode;
};

#endif /* __REPLAYER_H__ */