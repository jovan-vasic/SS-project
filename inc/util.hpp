#ifndef UTIL_HPP
#define UTIL_HPP

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
using namespace std;

constexpr auto PC_START = 0x40000000;

constexpr auto STATUS_REG = 0;
constexpr auto HANDLER_REG = 1;
constexpr auto CAUSE_REG = 2;
constexpr auto ACC_REG = 13;
constexpr auto SP_REG = 14;
constexpr auto PC_REG = 15;

constexpr auto HALT_OC = 0b00000000;
constexpr auto INT_OC = 0b00010000;
constexpr auto CALL_OC = 0b00100000;
constexpr auto JUMP_OC = 0b00110000;
constexpr auto XCHG_OC = 0b01000000;
constexpr auto ARIT_OC = 0b01010000;
constexpr auto LOGIC_OC = 0b01100000;
constexpr auto SHIFT_OC = 0b01110000;
constexpr auto STORE_OC = 0b10000000;
constexpr auto LOAD_OC = 0b10010000;

constexpr auto CALL_MOD0 = 0b0000;
constexpr auto CALL_MOD1 = 0b0001;

constexpr auto JMP_MOD0 = 0b0000;
constexpr auto JMP_MOD1 = 0b0001;
constexpr auto JMP_MOD2 = 0b0010;
constexpr auto JMP_MOD3 = 0b0011;
constexpr auto JMP_MOD4 = 0b1000;
constexpr auto JMP_MOD5 = 0b1001;
constexpr auto JMP_MOD6 = 0b1010;
constexpr auto JMP_MOD7 = 0b1011;

constexpr auto ADD_MOD = 0b0000;
constexpr auto SUB_MOD = 0b0001;
constexpr auto MUL_MOD = 0b0010;
constexpr auto DIV_MOD = 0b0011;

constexpr auto NOT_MOD = 0b0000;
constexpr auto AND_MOD = 0b0001;
constexpr auto OR_MOD = 0b0010;
constexpr auto XOR_MOD = 0b0011;

constexpr auto SHL_MOD = 0b0000;
constexpr auto SHR_MOD = 0b0001;

constexpr auto STORE_MOD0 = 0b0000;
constexpr auto STORE_MOD1 = 0b0010;
constexpr auto STORE_MOD2 = 0b0001;

constexpr auto LOAD_MOD0 = 0b0000;
constexpr auto LOAD_MOD1 = 0b0001;
constexpr auto LOAD_MOD2 = 0b0010;
constexpr auto LOAD_MOD3 = 0b0011;
constexpr auto LOAD_MOD4 = 0b0100;
constexpr auto LOAD_MOD5 = 0b0101;
constexpr auto LOAD_MOD6 = 0b0110;
constexpr auto LOAD_MOD7 = 0b0111;

constexpr auto WIDTH = 14;

constexpr uint8_t getByte(uint32_t value, int byteNum)
{
  return (value >> (8 * byteNum)) & 0xFF;
}

template <typename T>
constexpr int complement2(T value)
{
  if (is_signed<T>::value)
  {
    return value < 0 ? ((value & 0x7FF) | 0x800) : value;
  }
  else
  {
    return (value & 0x800) ? static_cast<int>(value | 0xFFFFF000) : static_cast<int>(value);
  }
}

class Instruction
{
public:
  unsigned char op;
  unsigned char A;
  unsigned char B;
  unsigned char C;
  unsigned int D;
};

class SymbolEntry
{
public:
  string name;
  int sectionId = 0;
  int offset = 0;
  bool isGlobal = false;
  bool isSection = false;
};

class RelocationEntry
{
public:
  int offset;
  int symbolId;
  int addend = 0;
};

class SectionEntry
{
public:
  string name;
  int size = 0;
  vector<char> memory;
  vector<RelocationEntry> relocs;
  unsigned int baseAddress = 0;
};

class ForwardLinkEntry // backpatching
{
public:
  int section;
  int locationCounter;
  string symbol;
  bool isRelative = false;
};

class FileEntry
{
public:
  string name;
  vector<SymbolEntry> symbolTable;
  vector<SectionEntry> sectionTable;
  int sectionCnt = 0;
};

class SectionPlace
{
public:
  string sectionName;
  unsigned baseAddress;
};

class LinkerMemoryEntry
{
public:
  string sectionName;
  vector<char> memory;
  unsigned int baseAddress = 0;
};

class EmulatorMemoryEntry
{
public:
  unsigned int address;
  char value;
};

template <typename Stream>
void printSymbols(Stream &output, vector<SymbolEntry> &table)
{
  output << "#.symtab" << endl
         << left
         << setw(WIDTH) << "Num"
         << setw(WIDTH) << "Value"
         << setw(WIDTH) << "Type"
         << setw(WIDTH) << "Bind"
         << setw(WIDTH) << "Ndx"
         << setw(WIDTH) << "Name" << endl;

  int i = 0;
  for (const auto &symbol : table)
  {
    output << left
           << setw(WIDTH) << i++
           << setw(WIDTH) << static_cast<unsigned>(symbol.offset)
           << setw(WIDTH) << (symbol.isSection ? "SCTN" : "NOTYP")
           << setw(WIDTH) << (symbol.isGlobal ? "GLOB" : "LOC")
           << setw(WIDTH) << (symbol.sectionId == 0 ? "UND" : to_string(symbol.sectionId))
           << setw(WIDTH) << symbol.name << endl;
  }
  output << endl;
}

template <typename Stream>
void printSections(Stream &output, vector<SectionEntry> &table)
{
  constexpr auto LINE_BREAK = 8;

  for (const auto &section : table)
  {
    if (section.name.empty() || section.name == "UND")
      continue; // Skip empty sections and the section named "UND"

    output << "#." << section.name << endl;
    int j = 0;
    for (const auto &content : section.memory)
    {
      output << setw(2) << setfill('0') << right << hex << static_cast<int>(static_cast<unsigned char>(content)) << " ";
      if (++j % LINE_BREAK == 0)
        output << "\n";
    }
    output << endl;
    if (j % LINE_BREAK != 0)
      output << "\n";

    output << dec << setfill(' ');
  }
}

template <typename Stream>
void printRelocations(Stream &output, vector<SectionEntry> &table)
{
  for (const auto &section : table)
  {
    if (section.name.empty() || section.name == "UND")
      continue; // Skip empty sections and the section named "UND"

    output << "#.rela." << section.name << endl
           << left
           << setw(WIDTH) << "Offset"
           << setw(WIDTH) << "Symbol"
           << setw(WIDTH) << "Addend" << endl;

    for (const auto &reloc : section.relocs)
    {
      output << left
             << setw(WIDTH) << reloc.offset
             << setw(WIDTH) << reloc.symbolId
             << setw(WIDTH) << reloc.addend << endl;
    }
    output << endl;
  }
}

#endif