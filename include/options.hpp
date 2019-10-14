#ifndef __OPTIOINS_H__
#define __OPTIOINS_H__

#include <string>
#include <stdio.h>
#include <unistd.h>
#include "section.hpp"
//#include "replayer.hpp"

class Options {
public:
  Options() : m_type(VC_NULL) { };
  int get_opts(int argc, char **argv);
  VCSectionType Type() const { return m_type; }
  const char * FileName() const { return m_fileName.c_str(); }

private:
  void SetName(char** name) { m_progName = *name; }
  void PrintHelp();

  std::string m_progName;
  std::string m_fileName;
  VCSectionType m_type;
};

extern Options g_opts;

#endif /* OPTIOINS_H__ */
