#ifndef EMULATOR_HPP
#define EMULATOR_HPP

#include "../inc/util.hpp"

class Emulator
{
private:
  bool emulation = true;

  vector<EmulatorMemoryEntry> memory;
  vector<unsigned int> regs;
  vector<unsigned int> csrRegs;

public:
  Emulator() {}
  ~Emulator() {}

  void loadMemory(ifstream &);
  void initRegisters();
  void printOutput();
  unsigned char findByte(unsigned int);
  Instruction getInstruction();
  unsigned int getFromMemory(unsigned int);
  void addToMemory(unsigned int, unsigned int);
  void emulate();
  void handleCall(Instruction&);
  void handleJump(Instruction&);
  void handleArit(Instruction&);
  void handleLogic(Instruction&);
  void handleShift(Instruction&);
  void handleStore(Instruction&);
  void handleLoad(Instruction&);
};

#endif