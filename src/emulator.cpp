#include "../inc/emulator.hpp"

#include <iostream>
#include <fstream>
#include <string>
using namespace std;

Instruction PUSH_PC{STORE_OC | STORE_MOD2, SP_REG, 0, PC_REG, static_cast<unsigned int>(-4)};
Instruction PUSH_STATUS{STORE_OC | STORE_MOD2, SP_REG, 0, STATUS_REG, static_cast<unsigned int>(-4)};

int main(int argc, char *argv[])
{
  cout << "EMULATOR | Start" << endl;

  if (argc != 2)
  {
    cout << "ERROR: Bad arguments" << endl;
    exit(-1);
  }

  ifstream inputFile(argv[1]);
  if (!inputFile)
  {
    cout << "ERROR | Cannot open input file!" << endl;
    exit(-1);
  }

  Emulator emulator;
  emulator.loadMemory(inputFile);
  emulator.initRegisters();
  emulator.emulate();

  cout << "EMULATOR | End" << endl;

  emulator.printOutput();

  return 0;
}

void Emulator::loadMemory(ifstream &inputFile)
{
  string line;
  while (getline(inputFile, line))
  {
    const unsigned int address = stoul(line.substr(0, 8), nullptr, 16);
    unsigned int cursor = 10;

    for (int i = 0; i < 8; i++)
    {
      const char value = static_cast<char>(stoul(line.substr(cursor, 2), nullptr, 16));

      memory.emplace_back(EmulatorMemoryEntry{address + i, value});
      cursor += 3;
    }
  }
  inputFile.close();
}

void Emulator::initRegisters()
{
  // Initialize 15 regs with 0
  regs.resize(15, 0);

  // Initialize the program counter register with PC_START
  regs.push_back(PC_START);

  // Initialize 3 CSR regs with 0
  csrRegs.resize(3, 0);
}

void Emulator::printOutput()
{
  cout << "-----------------------------------------------------------------" << endl;
  cout << "Emulated processor executed halt instruction" << endl;
  cout << "Emulated processor state:" << endl;

  int i = 0;
  for (const auto &reg : regs)
  {
    cout << (i < 10 ? " " : "") << "r" << dec << i << "=0x" << setw(8) << setfill('0') << hex << reg << " ";
    if (++i % 4 == 0)
      cout << endl;
  }
}

unsigned char Emulator::findByte(unsigned int addr)
{
  for (const auto &entry : memory)
  {
    if (entry.address == addr)
      return entry.value;
  }

  cout << "ERROR | Empty memory @ " << addr << endl;
  exit(-1);
}

Instruction Emulator::getInstruction()
{
  Instruction ins;
  unsigned char byte = findByte(regs[PC_REG]);
  ins.op = byte;
  regs[PC_REG]++;

  byte = findByte(regs[PC_REG]);
  ins.A = (byte & 0xF0) >> 4;
  ins.B = byte & 0x0F;
  regs[PC_REG]++;

  byte = findByte(regs[PC_REG]);
  ins.C = (byte & 0xF0) >> 4;
  ins.D = (byte & 0x0F) << 8;
  regs[PC_REG]++;

  byte = findByte(regs[PC_REG]);
  ins.D |= byte;
  regs[PC_REG]++;

  return ins;
}

unsigned int Emulator::getFromMemory(unsigned int addr)
{
  unsigned int val = 0;
  for (int i = 0; i < 4; ++i)
  {
    val |= static_cast<unsigned char>(findByte(addr + i)) << (8 * i);
  }
  return val;
}

void Emulator::addToMemory(unsigned int address, unsigned int value)
{
  int i = 0;
  for (; i < memory.size(); i++)
  {
    if (memory[i].address == address)
      break;
  }

  if (i == memory.size())
  {
    for (int j = 0; j < 4; j++)
    {
      EmulatorMemoryEntry entry;
      entry.address = address + j;
      entry.value = getByte(value, j);
      memory.push_back(entry);
    }
  }
  else
  {
    for (int j = 0; j < 4; j++)
    {
      memory[i + j].value = getByte(value, j);
    }
  }
}

void Emulator::emulate()
{
  while (emulation)
  {
    cout << "EMULATOR | " << hex << "SP=" << regs[SP_REG] << ", PC=" << regs[PC_REG] << endl;
    Instruction ins = getInstruction();
    switch (ins.op & 0xF0)
    {
    case HALT_OC:
      // Zaustavlja procesor kao i dalje izvršavanje narednih instrukcija.
      cout << "halt" << endl;
      emulation = false;
      break;
    case INT_OC:
      // push status; push pc; cause<=4; status<=status&(~0x1); pc<=handle;
      cout << "int" << endl;
      handleStore(PUSH_STATUS);
      handleStore(PUSH_PC);
      csrRegs[CAUSE_REG] = 4;
      csrRegs[STATUS_REG] = csrRegs[STATUS_REG] & (~0x1);
      regs[PC_REG] = csrRegs[HANDLER_REG];
      break;
    case CALL_OC:
      handleCall(ins);
      break;
    case JUMP_OC:
      handleJump(ins);
      break;
    case XCHG_OC:
      // temp<=gpr[B]; gpr[B]<=gpr[C]; gpr[C]<=temp;
      cout << "xchg" << endl;
      swap(regs[ins.B], regs[ins.C]);
      break;
    case ARIT_OC:
      handleArit(ins);
      break;
    case LOGIC_OC:
      handleLogic(ins);
      break;
    case SHIFT_OC:
      handleShift(ins);
      break;
    case STORE_OC:
      handleStore(ins);
      break;
    case LOAD_OC:
      handleLoad(ins);
      break;
    }
  }
}

void Emulator::handleCall(Instruction &ins)
{
  switch (ins.op)
  {
  case CALL_OC | CALL_MOD0:
    // push pc; pc<=gpr[A]+gpr[B]+D;
    cout << "callMod0" << endl;
    handleStore(PUSH_PC);
    regs[PC_REG] = regs[ins.A] + regs[ins.B] + complement2(ins.D);
    break;
  case CALL_OC | CALL_MOD1:
    // push pc; pc<=mem32[gpr[A]+gpr[B]+D];
    cout << "callMod1" << endl;
    handleStore(PUSH_PC);
    regs[PC_REG] = getFromMemory(regs[ins.A] + regs[ins.B] + complement2(ins.D));
    break;
  }
}

void Emulator::handleJump(Instruction &ins)
{
  switch (ins.op)
  {
  case JUMP_OC | JMP_MOD0:
    // pc<=gpr[A]+D;
    cout << "jmpMod0" << endl;
    regs[PC_REG] = regs[ins.A] + complement2(ins.D);
    break;
  case JUMP_OC | JMP_MOD1:
    // if (gpr[B] == gpr[C]) pc<=gpr[A]+D;
    cout << "jmpMod1" << endl;
    if (regs[ins.B] == regs[ins.C])
    {
      regs[PC_REG] = regs[ins.A] + complement2(ins.D);
    }
    break;
  case JUMP_OC | JMP_MOD2:
    // if (gpr[B] != gpr[C]) pc<=gpr[A]+D;
    cout << "jmpMod2" << endl;
    if (regs[ins.B] != regs[ins.C])
    {
      regs[PC_REG] = regs[ins.A] + complement2(ins.D);
    }
    break;
  case JUMP_OC | JMP_MOD3:
    // if (gpr[B] signed> gpr[C]) pc<=gpr[A]+D;
    cout << "jmpMod3" << endl;
    if ((int)regs[ins.B] > (int)regs[ins.C])
    {
      regs[PC_REG] = regs[PC_REG] = regs[ins.A] + complement2(ins.D);
    }
    break;
  case JUMP_OC | JMP_MOD4:
    // pc<=mem32[gpr[A]+D];
    cout << "jmpMod4" << endl;
    regs[PC_REG] = getFromMemory(regs[ins.A] + complement2(ins.D));
    break;
  case JUMP_OC | JMP_MOD5:
    // if (gpr[B] == gpr[C]) pc<=mem32[gpr[A]+D];
    cout << "jmpMod5" << endl;
    if (regs[ins.B] == regs[ins.C])
    {
      regs[PC_REG] = getFromMemory(regs[ins.A] + complement2(ins.D));
    }
    break;
  case JUMP_OC | JMP_MOD6:
    // if (gpr[B] != gpr[C]) pc<=mem32[gpr[A]+D];
    cout << "jmpMod6" << endl;
    if (regs[ins.B] != regs[ins.C])
    {
      regs[PC_REG] = getFromMemory(regs[ins.A] + complement2(ins.D));
    }
    break;
  case JUMP_OC | JMP_MOD7:
    // if (gpr[B] signed> gpr[C]) pc<=mem32[gpr[A]+D];
    cout << "jmpMod7" << endl;
    if (regs[ins.B] > regs[ins.C])
    {
      regs[PC_REG] = getFromMemory(regs[ins.A] + complement2(ins.D));
    }
    break;
  }
}

void Emulator::handleArit(Instruction &ins)
{
  switch (ins.op)
  {
  case ARIT_OC | ADD_MOD:
    // gpr[A]<=gpr[B] + gpr[C];
    cout << "add" << endl;
    regs[ins.A] = regs[ins.B] + regs[ins.C];
    break;
  case ARIT_OC | SUB_MOD:
    // gpr[A]<=gpr[B] - gpr[C];
    cout << "sub" << endl;
    regs[ins.A] = regs[ins.B] - regs[ins.C];
    break;
  case ARIT_OC | MUL_MOD:
    // gpr[A]<=gpr[B] * gpr[C];
    cout << "mul" << endl;
    regs[ins.A] = regs[ins.B] * regs[ins.C];
    break;
  case ARIT_OC | DIV_MOD:
    // gpr[A]<=gpr[B] / gpr[C];
    cout << "div" << endl;
    regs[ins.A] = regs[ins.B] / regs[ins.C];
    break;
  }
}

void Emulator::handleLogic(Instruction &ins)
{
  switch (ins.op)
  {
  case LOGIC_OC | NOT_MOD:
    // gpr[A]<=~gpr[B];
    cout << "not" << endl;
    regs[ins.A] = ~regs[ins.B];
    break;
  case LOGIC_OC | AND_MOD:
    // gpr[A]<=gpr[B] & gpr[C];
    cout << "and" << endl;
    regs[ins.A] = regs[ins.B] & regs[ins.C];
    break;
  case LOGIC_OC | OR_MOD:
    // gpr[A]<=gpr[B] | gpr[C]
    cout << "or" << endl;
    regs[ins.A] = regs[ins.B] | regs[ins.C];
    break;
  case LOGIC_OC | XOR_MOD:
    // gpr[A]<=gpr[B] ^ gpr[C];
    cout << "xor" << endl;
    regs[ins.A] = regs[ins.B] ^ regs[ins.C];
    break;
  }
}

void Emulator::handleShift(Instruction &inst)
{
  switch (inst.op)
  {
  case SHIFT_OC | SHL_MOD:
    // gpr[A]<=gpr[B] << gpr[C];
    cout << "shl" << endl;
    regs[inst.A] = regs[inst.B] << regs[inst.C];
    break;
  case SHIFT_OC | SHR_MOD:
    // gpr[A]<=gpr[B] >> gpr[C];
    cout << "shr" << endl;
    regs[inst.A] = regs[inst.B] >> regs[inst.C];
    break;
  }
}

void Emulator::handleStore(Instruction &ins)
{
  switch (ins.op)
  {
  case STORE_OC | STORE_MOD0:
    // mem32[gpr[A]+gpr[B]+D]<=gpr[C];
    cout << "storeMod0" << endl;
    addToMemory(regs[ins.A] + regs[ins.B] + complement2(ins.D), regs[ins.C]);
    break;
  case STORE_OC | STORE_MOD1:
    // mem32[mem32[gpr[A]+gpr[B]+D]]<=gpr[C];
    cout << "storeMod1" << endl;
    addToMemory(getFromMemory(regs[ins.A] + regs[ins.B] + complement2(ins.D)), regs[ins.C]);
    break;
  case STORE_OC | STORE_MOD2:
    // gpr[A]<=gpr[A]+D; mem32[gpr[A]]<=gpr[C];
    cout << "storeMod2" << endl;
    regs[ins.A] += complement2(ins.D);
    addToMemory(regs[ins.A], regs[ins.C]);
    break;
  }
}

void Emulator::handleLoad(Instruction &ins)
{
  switch (ins.op)
  {
  case LOAD_OC | LOAD_MOD0:
    // gpr[A]<=csr[B];
    cout << "loadMod0" << endl;
    regs[ins.A] = csrRegs[ins.B];
    break;
  case LOAD_OC | LOAD_MOD1:
    // gpr[A]<=gpr[B]+D;
    cout << "loadMod1" << endl;
    regs[ins.A] = regs[ins.B] + complement2(ins.D);
    break;
  case LOAD_OC | LOAD_MOD2:
    // gpr[A]<=mem32[gpr[B]+gpr[C]+D];
    cout << "loadMod2" << endl;
    regs[ins.A] = getFromMemory(regs[ins.B] + regs[ins.C] + complement2(ins.D));
    break;
  case LOAD_OC | LOAD_MOD3:
    // gpr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
    cout << "loadMod3" << endl;
    regs[ins.A] = getFromMemory(regs[ins.B]);
    regs[ins.B] += complement2(ins.D);
    break;
  case LOAD_OC | LOAD_MOD4:
    // csr[A]<=gpr[B];
    cout << "loadMod4" << endl;
    csrRegs[ins.A] = regs[ins.B];
    break;
  case LOAD_OC | LOAD_MOD5:
    // csr[A]<=csr[B]|D;
    cout << "loadMod5" << endl;
    csrRegs[ins.A] = csrRegs[ins.B] | complement2(ins.D);
    break;
  case LOAD_OC | LOAD_MOD6:
    // csr[A]<=mem32[gpr[B]+gpr[C]+D];
    cout << "loadMod6" << endl;
    csrRegs[ins.A] = getFromMemory(regs[ins.B] + regs[ins.C] + complement2(ins.D));
    break;
  case LOAD_OC | LOAD_MOD7:
    // csr[A]<=mem32[gpr[B]]; gpr[B]<=gpr[B]+D;
    cout << "loadMod7" << endl;
    csrRegs[ins.A] = getFromMemory(regs[ins.B]);
    regs[ins.B] += complement2(ins.D);
    break;
  }
}
