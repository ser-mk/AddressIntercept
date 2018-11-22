/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2018 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
/*! @file
  Generates a trace of malloc/free calls
 */

#include "pin.H"
#include <iostream>
#include <fstream>
#include "addrIntercept.h"
#include "logger.h"

//todo: add magic field
#if defined(TARGET_IA32)
#define FORMAT_PIPE_STRING "id:%lld | %s | addr: %p | size: %d | value: 0x%x | st: %s"
#else
#define FORMAT_PIPE_STRING "id:%ld | %s | addr: %p | size: %d | value: 0x%lx | st: %s"
#endif

static const size_t NUMBER_ARGS_FROM_PIPE_STRING = 6;

static const std::string COMMAND_LOAD("LOAD");
static const std::string COMMAND_STORE("STORE");

LevelDebug Log::gLevel = _ERROR;

using namespace std;

KNOB<string> KnobInputFifo(KNOB_MODE_WRITEONCE, "pintool",
    "in", "in.fifo", "specify file name");

KNOB<string> KnobOutputFifo(KNOB_MODE_WRITEONCE, "pintool",
    "out", "out.fifo", "specify file name");

KNOB<int> KnobLevelDebug(KNOB_MODE_WRITEONCE, "pintool",
    "v", "0", "specify the current scenario to be checked (0-4)");

static ifstream inFifo;
static ofstream outFifo;
static memoryTranslate * addrMap = NULL;
static sizeMemoryTranslate_t sizeMap = 0;
static uint64_t id = 0;


const size_t BUFFER_SIZE = 100;
char pipeBuffer[BUFFER_SIZE] = {0};

static bool isEntryInMap(const ADDRINT * addr){
    if(addrMap == NULL
            || sizeMap == 0 ){
        //std::cerr << "Map addr don't init!" <<  endl;
        return false;
    }
    for(sizeMemoryTranslate_t i = 0; i < sizeMap; i++){
        bool isEntry = addrMap[i].start_addr <= addr;
        isEntry &= addrMap[i].end_addr > addr;
        if(isEntry)
            return true;
    }
    return false;
}

static ADDRINT * local2remoteAddr(const ADDRINT * addr){
    ADDRINT * remoteAddr = NULL;
    for(sizeMemoryTranslate_t i = 0; i < sizeMap; i++){
        bool isEntry = addrMap[i].start_addr <= addr;
        isEntry &= addrMap[i].end_addr > addr;
        if(isEntry){
            remoteAddr = addr - addrMap[i].start_addr + addrMap[i].reference_addr;
            break;
        }
    }
    return remoteAddr;
}

static void sendCommand(const string & command, const ADDRINT * addr, const UINT32 size, const ADDRINT value){

    const ADDRINT * remAddr = local2remoteAddr(addr);
    const int write_size = snprintf(pipeBuffer, BUFFER_SIZE, FORMAT_PIPE_STRING, //"id:%lx | %s | addr: %p | size: %d | value: %lx",
             id, command.c_str(), remAddr, size, value, "");
    if(write_size <= 0){
        MAGIC_LOG(_ERROR) << "small pipe buffer: " << BUFFER_SIZE;
    }

    outFifo << pipeBuffer <<  endl;
}

static bool parseValue(const string & line, ADDRINT & value){
    char command_str[20];
    char status_str[20];

    static uint64_t id_in = 0;

    ADDRINT * addr = NULL;
    UINT32 size = 0;

    const int scan_size = sscanf(line.c_str(), FORMAT_PIPE_STRING, &id_in, command_str, &addr, &size, &value, status_str);
    if(scan_size != NUMBER_ARGS_FROM_PIPE_STRING){
        MAGIC_LOG(_ERROR) << "Can't parse value from pipe, success args: " << scan_size;
        return false;
    }

   MAGIC_LOG(_DEBUG) << " " << id_in << " " << command_str <<" " << addr <<" " << size <<" " << value << " status_str " << status_str;

    if(id != id_in){
        MAGIC_LOG(_ERROR) << "wrong id message: " << id_in << " expected: " << id;
        return false;
    }

    //Todo: check magic number

    return true;
}

static ADDRINT queryValue(const ADDRINT * addr, const UINT32 size){
    std::string   line;
    sendCommand(COMMAND_LOAD, addr, size, 0);
    std::getline(inFifo, line);
    ADDRINT value = 0;
    if(parseValue(line, value) == false){
        value = 0;
    }
    return value;
}


// Move a register or literal to memory
static VOID storeReg2Addr(const ADDRINT * addr, const ADDRINT value, const UINT32 size)
{
    if(isEntryInMap(addr) == false)
        return;
    MAGIC_LOG(_DEBUG) << "!s " << addr << " size : " << size << " value: " << value;
    std::string   line;
    sendCommand(COMMAND_STORE, addr, size, value);
    std::getline(inFifo, line);
    ADDRINT remoteValue = 0;
    if(parseValue(line, remoteValue) == false){
        MAGIC_LOG(_ERROR)<< "error store addr: " << addr ;
    }

    if(remoteValue != value){
        MAGIC_LOG(_ERROR) << "error store addr: " << addr << " expected value: " << value << " recieve: " << remoteValue ;
    }

}

// Move from memory to register
static ADDRINT loadAddr2Reg( ADDRINT * addr, UINT32 size)
{
    ADDRINT value = 0;

    if(isEntryInMap(addr)){
        MAGIC_LOG(_DEBUG)<< "addr : " << addr << " size : " << size;
        value = queryValue(addr, size);
  } else {
        PIN_SafeCopy(&value, addr, size);
  }
    return value;
}

static VOID multiMemAccessStore(const PIN_MULTI_MEM_ACCESS_INFO* multiMemAccessInfo, const ADDRINT value)
{
    if(multiMemAccessInfo->numberOfMemops != 1)
        return;

    const PIN_MEM_ACCESS_INFO *pinMemAccessInfo = &(multiMemAccessInfo->memop[0]);
    const ADDRINT * addr = (ADDRINT *)pinMemAccessInfo->memoryAddress;
    const UINT32 size = pinMemAccessInfo->bytesAccessed;

    if(isEntryInMap(addr) == false)
        return;

    MAGIC_LOG(_DEBUG) << "addr : " << addr << " value: "
              <<  hex << value << " size: " << size;

    storeReg2Addr(addr,value,size);
}

static memoryTranslate * replaceMemoryMapFun( CONTEXT * context, AFUNPTR orgFuncptr,sizeMemoryTranslate_t * size ){
    MAGIC_LOG(_INFO) << "call ... size: " << size ;

        PIN_CallApplicationFunction(context, PIN_ThreadId(),
            CALLINGSTD_DEFAULT, orgFuncptr, NULL,
                 PIN_PARG(memoryTranslate *), &addrMap,
                 PIN_PARG(sizeMemoryTranslate_t*), size,
                 PIN_PARG_END());

        sizeMap = *size;

         MAGIC_LOG(_INFO) << "memoryTranslate : " << addrMap;
    return addrMap;
}


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

static INT32 Usage()
{
    cout <<
        "Help message\n"
        "\n";
    cout << KNOB_BASE::StringKnobSummary();
    cout << endl;
    return -1;
}


/* ===================================================================== */
// Called every time a new image is loaded.
// Look for routines that we want to replace.
static VOID ImageReplace(IMG img, VOID *v)
{
    RTN freeRtn = RTN_FindByName(img, NAME_MEMORY_MAP_FUNCTION);
    if (RTN_Valid(freeRtn))
    {
        PROTO proto_free = PROTO_Allocate( PIN_PARG(memoryTranslate *), CALLINGSTD_DEFAULT,
                                           NAME_MEMORY_MAP_FUNCTION, PIN_PARG(sizeMemoryTranslate_t*),  PIN_PARG_END() );
        
        RTN_ReplaceSignature(
            freeRtn, AFUNPTR( replaceMemoryMapFun ),
            IARG_PROTOTYPE, proto_free,
            IARG_CONTEXT,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);

        MAGIC_LOG(_INFO) << "!Replaced " << NAME_MEMORY_MAP_FUNCTION << " in:"  << IMG_Name(img);
    }
}

//__attribute__((unused))
static VOID EmulateLoad(INS ins, VOID* v)
{
    // Find the instructions that move a value from memory to a register
    if ( (INS_Opcode(ins) == XED_ICLASS_MOV
          || INS_Opcode(ins) == XED_ICLASS_MOVZX
          ) &&
        INS_IsMemoryRead(ins) &&
        INS_OperandIsReg(ins, 0) &&
        INS_OperandIsMemory(ins, 1))
    {
        INS_InsertCall(ins,
                       IPOINT_BEFORE,
                       AFUNPTR(loadAddr2Reg),
                       IARG_MEMORYREAD_EA,
                       IARG_MEMORYREAD_SIZE,
                       IARG_RETURN_REGS,
                       INS_OperandReg(ins, 0),
                       IARG_END);

        // Delete the instruction
        INS_Delete(ins);
    }
}

//__attribute__((unused))
static VOID EmulateStore(INS ins, VOID* v)
{
    // Find the instructions that move a value from memory to a register
    if (INS_Opcode(ins) == XED_ICLASS_MOV &&
        INS_IsMemoryWrite(ins) &&
        INS_OperandIsMemory(ins, 0))
    {
      if(INS_hasKnownMemorySize(ins)){
          if(INS_OperandIsReg(ins, 1)){
              INS_InsertCall(ins,
                             IPOINT_BEFORE,
                             AFUNPTR(multiMemAccessStore),
                             IARG_MULTI_MEMORYACCESS_EA,
                             IARG_REG_VALUE,
                             INS_OperandReg(ins, 1),
                             IARG_END);
      } else if(INS_OperandIsImmediate(ins, 1)){
                      INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)multiMemAccessStore,
                                              IARG_MULTI_MEMORYACCESS_EA,
                                     IARG_UINT64, INS_OperandImmediate(ins, 1),
                                              IARG_END);
          }
      } else {
        if(INS_OperandIsReg(ins, 1)){
            INS_InsertCall(ins,
                           IPOINT_BEFORE,
                           AFUNPTR(storeReg2Addr),
                           IARG_MEMORYWRITE_EA,
                           IARG_REG_VALUE,
                           INS_OperandReg(ins, 1),
                           IARG_MEMORYWRITE_SIZE,
                           IARG_END);
    } else if(INS_OperandIsImmediate(ins, 1)){
        INS_InsertCall(ins,
                           IPOINT_BEFORE,
                           //AFUNPTR(storeImmediate2Addr),
                            AFUNPTR(storeReg2Addr),
                           IARG_MEMORYWRITE_EA,
                           //IARG_UINT32,
                       IARG_UINT64,
                        INS_OperandImmediate(ins, 1),
                       IARG_UINT32,
                       IARG_MEMORYWRITE_SIZE,
                           IARG_END);
            }
        }
    }
}




static bool initFifo(){
    //Warning: order call open function
    outFifo.open(KnobOutputFifo.Value().c_str());
    inFifo.open(KnobInputFifo.Value().c_str());
    return inFifo.is_open() && inFifo.good() && outFifo.is_open() && outFifo.good();    
}


int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) ){
        return Usage();
    }

    Log::setGLevel((LevelDebug)(KnobLevelDebug.Value() & 0xF));

    MAGIC_LOG(_INFO) << "Wait OCD client...";

    if( initFifo() == false ){
        MAGIC_LOG(_ERROR) << "Error open fifo file: " << KnobInputFifo.Value()
            << " or " << KnobOutputFifo.Value();
        return -2;
    }

    IMG_AddInstrumentFunction(ImageReplace, 0);

    INS_AddInstrumentFunction(EmulateLoad, 0);

    INS_AddInstrumentFunction(EmulateStore, 0);

    MAGIC_LOG(_DEBUG) << "Start...";
    PIN_StartProgram();
    
    return 0;
}

