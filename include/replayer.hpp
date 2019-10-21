#ifndef __REPLAYER_H__
#define __REPLAYER_H__

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>

#include "hsautils.hpp"
#include "section.hpp"
#include "global.hpp"

enum ReplayMode {
  RE_VC = 1,
  RE_HSACO,
};

class HsacoKernArg
{
public:
  HCStoreType sType;
  VCDataType dType;
  std::unique_ptr<HSAMemoryObject> mem;
  union {
      int i;
      float f;
      double d;
  }value;
  std::ostream& Print(std::ostream &out);
  friend std::ostream& operator<< (std::ostream &out, HsacoKernArg &ka) {
    return ka.Print(out);
  };
};


class Replayer {
public:
  Replayer();
  ~Replayer();

  int LoadData(const char *fileName, const char *symbol = NULL);
  void UpdateKernelArgPool();

  uint64_t GetHexValue(std::string &line, const char *key);
  VCSection* GetSection(VCSectionType type);
  void PrintSection(VCSectionType type);
  void PrintKernArgs(void);
  void PrintKernArg(int index);
  void SetDataTypes(std::vector<VCDataType> *argsType);

  void CreateQueue(uint32_t size, hsa_queue_type32_t type);
  void SubmitPacket(JsonKernObj *kernObj = NULL, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs = NULL);

private:
  int LoadVectorFile(const char *fileName);
  int LoadHsacoFile(const char *fileName, const char *symbol);
  void VCSubmitPacket(void);
  void HsacoSubmitPacket(JsonKernObj *kernObj, std::vector<std::unique_ptr<JsonKernArg>> *kernArgs);
  bool IsVCMode(void) { return m_mode == RE_VC; }

  std::vector<VCSection*> m_sections;
  hsa_agent_t m_agent;
  HSAExecutable *m_executable;
  HSAQueue *m_queue;
  ReplayMode m_mode;
};

#endif /* __REPLAYER_H__ */