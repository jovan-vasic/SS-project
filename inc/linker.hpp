#ifndef LINKER_HPP
#define LINKER_HPP

#include "../inc/util.hpp"
#include <unordered_set>

class Linker
{
private:
  int sectionCnt = 0;

  vector<FileEntry> fileEntries;
  vector<LinkerMemoryEntry> linkerMemory;
  unordered_set<string> processedSections;

public:
  Linker() {}
  ~Linker() {}

  // Getters
  vector<FileEntry> &getFileEntries() { return fileEntries; }
  vector<LinkerMemoryEntry> &getLinkerMemory() { return linkerMemory; }

  // For main
  vector<SectionPlace> extractSectionPlaces(int, char *argv[]);
  vector<string> extractInputFiles(int, int, char *argv[]);

  // Other
  void parseSymbols(ifstream &, FileEntry &);
  void parseSections(ifstream &, FileEntry &);
  void parseRelocs(ifstream &, FileEntry &);
  void fillMemory0() {}
  void fillMemory(vector<SectionPlace>);
  void resolveSymbols();
  void resolveRelocs();
  void writeMemContent(ofstream &, unsigned int &, LinkerMemoryEntry &, int, int);
  int writeMem(ofstream &, bool &, LinkerMemoryEntry &);
  int fillLine(ofstream &, bool, bool &, LinkerMemoryEntry &);
  void writeLinkerOutput(ofstream &);
};

#endif