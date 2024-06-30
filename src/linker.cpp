#include "../inc/linker.hpp"
#include "../inc/util.hpp"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

int main(int argc, char *argv[])
{
    cout << "LINKER | Start" << endl;

    if (argc < 5)
    {
        cout << "ERROR | Bad arguments" << endl;
        return -1;
    }

    // Find the index of the output file in the command line arguments
    int outputId = -1;
    for (int i = 1; i < argc; i++)
    {
        if (string(argv[i]) == "-o")
        {
            outputId = ++i;
            break;
        }
    }
    if (outputId == -1 || outputId >= argc)
    {
        cout << "ERROR | Output file not specified correctly" << endl;
        return -1;
    }

    Linker linker;

    // Parsing input files
    vector<string> inputFiles = linker.extractInputFiles(outputId, argc, argv);
    for (const string &f : inputFiles)
    {
        ifstream file(f);
        if (!file.is_open())
        {
            cout << "ERROR | Failed to open the file: " << f << endl;
            return -1;
        }
        cout << "LINKER | Parsing file: " << f << endl;

        FileEntry entry({f});
        linker.parseSymbols(file, entry);
        linker.parseSections(file, entry);
        linker.parseRelocs(file, entry);

        file.close();
        linker.getFileEntries().push_back(entry);
    }

    // @
    vector<SectionPlace> sectionPlaces = linker.extractSectionPlaces(outputId, argv);
    sort(sectionPlaces.begin(), sectionPlaces.end(), [](const SectionPlace &lhs, const SectionPlace &rhs)
         { return lhs.baseAddress < rhs.baseAddress; });
    sectionPlaces.empty() ? linker.fillMemory0() : linker.fillMemory(sectionPlaces);

    // Resolve
    linker.resolveSymbols();
    linker.resolveRelocs();

    // Info output
    string outputTextFile = "linker.txt";
    ofstream outputFile(outputTextFile);
    if (!outputFile)
    {
        cout << "ERROR | Failed to open the file: " << outputTextFile << endl;
        return -1;
    }
    outputFile << "#.LinkerMemory" << endl
               << left
               << setw(WIDTH) << "Section"
               << setw(WIDTH) << "Base"
               << setw(WIDTH) << "Size" << endl;
    for (const auto &mem_entry : linker.getLinkerMemory())
    {
        outputFile << left
                   << setw(WIDTH) << mem_entry.sectionName
                   << setw(WIDTH) << mem_entry.baseAddress
                   << setw(WIDTH) << mem_entry.memory.size() << endl;
    }
    outputFile << endl;
    for (auto &f : linker.getFileEntries())
    {
        outputFile << "---------------------------------- " << f.name << " ----------------------------------" << endl;
        printSymbols(outputFile, f.symbolTable);
        printSections(outputFile, f.sectionTable);
        printRelocations(outputFile, f.sectionTable);
    }

    // Linker output
    string linkerOutputFile = string(argv[outputId]);
    ofstream linkerOutput(linkerOutputFile);
    if (!linkerOutput)
    {
        cout << "ERROR | Failed to open the file: " << linkerOutputFile << endl;
        return -1;
    }
    linker.writeLinkerOutput(linkerOutput);

    cout << "LINKER | End" << endl;

    return 0;
}

// For main
vector<SectionPlace> Linker::extractSectionPlaces(int id, char *argv[])
{
    vector<SectionPlace> sectionPlaces;
    for (int i = 2; i < id - 1; i++)
    {
        string argument = argv[i];
        size_t pos = argument.find("=");
        string extractedString = argument.substr(pos + 1);

        pos = extractedString.find("@");
        string text = extractedString.substr(0, pos);
        string str = extractedString.substr(pos + 1);
        unsigned int num = stoul(str, nullptr, 16);

        sectionPlaces.push_back({text, num});
    }
    return sectionPlaces;
}

// For main
vector<string> Linker::extractInputFiles(int id, int argc, char *argv[])
{
    vector<string> files;
    for (int i = id + 1; i < argc; i++)
    {
        files.push_back(argv[i]);
    }
    return files;
}

void Linker::parseSymbols(ifstream &file, FileEntry &fEntry)
{
    sectionCnt = 0;
    string line;
    getline(file, line); // Skip #.
    getline(file, line); // Skip var names

    while (getline(file, line))
    {
        if (line.empty())
            break;

        stringstream ss(line);
        string num, value, type, bind, ndx, name;
        ss >> num >> value >> type >> bind >> ndx >> name;

        SymbolEntry entry({name});
        int id = (ndx != "UND") ? stoi(ndx) : 0;
        entry.sectionId = id;
        entry.offset = stoi(value);
        entry.isGlobal = (bind == "GLOB");
        entry.isSection = (type == "SCTN");

        if (type == "SCTN")
            sectionCnt++;

        fEntry.symbolTable.push_back(entry);
    }

    sectionCnt--; // UND section
    fEntry.sectionCnt = sectionCnt;
}

void Linker::parseSections(ifstream &file, FileEntry &fEntry)
{
    // UND section
    SectionEntry entry({"UND"});
    processedSections.insert("UND");
    fEntry.sectionTable.push_back(entry);

    string line;
    int section_counter = 0;
    streampos currentPosition;

    while (getline(file, line) && section_counter < sectionCnt)
    {
        if (!line.empty() && line[0] == '#')
        {
            section_counter++;
            SectionEntry entry({line.substr(2)});

            string contentLine;
            while (getline(file, contentLine))
            {
                currentPosition = file.tellg();
                if (contentLine.empty())
                    break;

                stringstream ss(contentLine);
                string hexNumber;
                while (ss >> hexNumber)
                {
                    int byte = stoi(hexNumber, nullptr, 16);
                    entry.memory.push_back(static_cast<char>(byte));
                }
                entry.size = entry.memory.size();
            }
            fEntry.sectionTable.push_back(entry);
        }
    }
    file.seekg(currentPosition);
}

void Linker::parseRelocs(ifstream &file, FileEntry &fEntry)
{
    string line;
    while (getline(file, line))
    {
        if (line.empty())
            break;

        if (line[0] == '#')
        {
            string sectionName = line.substr(line.find_last_of('.') + 1);
            string entryLine;

            while (getline(file, entryLine))
            {
                if (entryLine.empty())
                    break;
                if (entryLine[0] == 'O')
                    continue;

                stringstream ss(entryLine);
                int offset, symbol, addend;
                ss >> offset >> symbol >> addend;

                for (auto &section : fEntry.sectionTable)
                {
                    if (section.name == sectionName)
                    {
                        RelocationEntry entry({offset, symbol, addend});

                        section.relocs.push_back(entry);
                        break;
                    }
                }
            }
        }
    }
}

void Linker::fillMemory(vector<SectionPlace> section_places)
{
    // Sections from command line
    for (const auto &place : section_places)
    {
        string sectionName = place.sectionName;
        unsigned int baseAddress = place.baseAddress;
        LinkerMemoryEntry entry({sectionName});
        entry.baseAddress = baseAddress;

        for (auto &obj : fileEntries)
        {
            // Find section and process it
            SectionEntry *section = nullptr;
            for (auto &sec : obj.sectionTable)
            {
                if (sec.name == sectionName)
                {
                    section = &sec;
                    break;
                }
            }
            if (!section)
                continue;
            section->baseAddress = baseAddress;
            baseAddress += section->size;
            entry.memory.insert(entry.memory.end(), section->memory.begin(), section->memory.end());
            processedSections.insert(sectionName);
        }
        linkerMemory.push_back(entry);
    }

    // Check for overlapping sections
    for (int i = 0; i < linkerMemory.size() - 1; i++)
    {
        if (linkerMemory[i].memory.size() + section_places[i].baseAddress >= linkerMemory[i + 1].baseAddress)
        {
            cout << "ERROR | Sections " << i << " and " << i + 1 << " are overlapping" << endl;
            exit(-1);
        }
    }

    // Find new base address
    const string &name = section_places.back().sectionName;
    unsigned int new_base_address = 0;

    for (const auto &memory : linkerMemory)
    {
        if (memory.sectionName == name)
        {
            new_base_address = memory.memory.size() + memory.baseAddress;
        }
    }

    // Remaining sections
    vector<string> section_names;
    for (const auto &obj : fileEntries)
    {
        if (obj.sectionCnt == 0)
            continue;

        for (const auto &section : obj.sectionTable)
        {
            if (processedSections.find(section.name) != processedSections.end())
                continue;

            if (find(section_names.begin(), section_names.end(), section.name) == section_names.end())
            {
                section_names.push_back(section.name);
            }
        }
    }
    for (const auto &sectionName : section_names)
    {
        LinkerMemoryEntry entry({sectionName});
        entry.baseAddress = new_base_address;

        for (auto &obj : fileEntries)
        {
            SectionEntry *section = nullptr;
            for (auto &sec : obj.sectionTable)
            {
                if (sec.name == sectionName)
                {
                    section = &sec;
                    break;
                }
            }
            if (!section)
                continue;
            section->baseAddress = new_base_address;
            new_base_address += section->size;
            entry.memory.insert(entry.memory.end(), section->memory.begin(), section->memory.end());
            processedSections.insert(sectionName);
        }

        linkerMemory.push_back(entry);
    }
}

void Linker::resolveSymbols()
{
    // Global symbols
    for (auto &obj : fileEntries)
    {
        for (auto it = obj.symbolTable.begin() + 1; it != obj.symbolTable.end(); ++it)
        {
            if (it->sectionId != 0)
            {
                int sectionId = it->sectionId;
                it->offset += obj.sectionTable[sectionId].baseAddress;
                // cout << obj.name << ": "
                //      << (it->isGlobal ? "Global" : "Local")
                //      << " symbol " << it->name << " = "
                //      << static_cast<unsigned>(it->offset) << endl;
            }
        }
    }

    // Extern symbols
    for (auto &obj : fileEntries)
    {
        for (auto it = obj.symbolTable.begin() + 1; it != obj.symbolTable.end(); ++it)
        {
            if (it->sectionId == 0)
            {
                for (const auto &obj : fileEntries)
                {
                    for (const auto &objSymb : obj.symbolTable)
                    {
                        if (objSymb.name == it->name && objSymb.isGlobal && objSymb.sectionId != 0)
                        {
                            it->offset = objSymb.offset;
                            // cout << obj.name << ": "
                            //      << "Extern symbol " << it->name << " = "
                            //      << static_cast<unsigned>(it->offset) << endl;
                            break;
                        }
                    }
                }
                if (it->offset == 0)
                {
                    cout << "ERROR | Extern symbol " << it->name << " is not defined as global anywhere" << endl;
                    exit(-1);
                }
            }
        }
    }
}

void Linker::resolveRelocs()
{
    for (auto &obj : fileEntries)
    {
        for (auto &section : obj.sectionTable)
        {
            int sectionId = -1;
            for (int i = 0; i < linkerMemory.size(); i++)
            {
                if (linkerMemory[i].sectionName == section.name)
                {
                    sectionId = i;
                    break;
                }
            }

            if (sectionId == -1)
                continue;

            for (auto &reloc : section.relocs)
            {
                unsigned int val = obj.symbolTable[reloc.symbolId].offset;
                unsigned int off = reloc.offset + section.baseAddress - linkerMemory[sectionId].baseAddress;
                auto &memory = linkerMemory[sectionId].memory;

                for (int j = 0; j < 4; ++j)
                {
                    memory[off + j] = getByte(val, j);
                }
            }
        }
    }
}

void Linker::writeMemContent(ofstream &file, unsigned int &currAddress, LinkerMemoryEntry &entry, int i, int k)
{
    stringstream addrStream;
    addrStream << hex << setfill('0') << setw(4) << currAddress;
    string hexAddress = addrStream.str();

    file << hexAddress << ": ";

    for (int j = 0; j < k; j++)
    {
        file << setw(2) << setfill('0') << right << hex << static_cast<int>(static_cast<unsigned char>(entry.memory[j + i * 8])) << " ";
        file << dec << setfill(' ');
    }
}

int Linker::writeMem(ofstream &file, bool &div, LinkerMemoryEntry &entry)
{
    div = entry.memory.size() % 8 != 0;
    unsigned int currAddress = entry.baseAddress;

    int i = 0;
    for (; i < entry.memory.size() / 8; i++)
    {
        writeMemContent(file, currAddress, entry, i, 8);
        file << endl;
        currAddress += 8;
    }

    if (div)
    {
        // Need to print last 4 bytes
        writeMemContent(file, currAddress, entry, i, 4);
        currAddress += 4;
    }

    return currAddress;
}

int Linker::fillLine(ofstream &file, bool fill0, bool &div, LinkerMemoryEntry &entry)
{
    for (int j = 0; j < 4; j++)
    {
        int value = fill0 ? 0 : static_cast<int>(static_cast<unsigned char>(entry.memory[j]));
        file << setw(2) << setfill('0') << right << hex << value << " ";
    }

    file << dec << setfill(' ') << endl;

    if (!fill0)
    {
        entry.memory.erase(entry.memory.begin(), entry.memory.begin() + 4);
        entry.baseAddress += 4;
    }

    return writeMem(file, div, entry);
}

void Linker::writeLinkerOutput(ofstream &file)
{
    bool outputDiv = false;
    unsigned int currAddress = 0;
    for (LinkerMemoryEntry &entry : linkerMemory)
    {
        bool consecutive = currAddress == entry.baseAddress;

        if (outputDiv)
        {
            currAddress = fillLine(file, !consecutive, outputDiv, entry);
        }
        else
        {
            currAddress = writeMem(file, outputDiv, entry);
        }
    }

    if (outputDiv)
    {
        for (int i = 0; i < 4; i++)
        {
            file << setw(2) << setfill('0') << right << hex << 0 << " ";
        }
        file << dec << setfill(' ') << endl;
    }
}