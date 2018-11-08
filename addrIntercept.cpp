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

//todo: add magic field
#define FORMAT_PIPE_STRING "id:%lx | %s | addr: %p | size: %d | value: %lx"
static const size_t NUMBER_ARGS_FROM_PIPE_STRING = 5;

static const std::string COMMAND_LOAD("LOAD");
static const std::string COMMAND_STORE("STORE");

using namespace std;

KNOB<string> KnobInputFifo(KNOB_MODE_WRITEONCE, "pintool",
    "in", "in.fifo", "specify file name");

KNOB<string> KnobOutputFifo(KNOB_MODE_WRITEONCE, "pintool",
    "out", "out.fifo", "specify file name");

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

static void sendCommand(const string & command, const ADDRINT * addr, const UINT32 size, const ADDRINT value){

    const int write_size = snprintf(pipeBuffer, BUFFER_SIZE, FORMAT_PIPE_STRING, //"id:%lx | %s | addr: %p | size: %d | value: %lx",
             id, command.c_str(), addr, size, value);
    if(write_size <= 0){
        std::cerr << "small pipe buffer: " << BUFFER_SIZE <<  endl;
    }

    outFifo << pipeBuffer <<  endl;
}

static bool parseValue(const string & line, ADDRINT & value){
    char command_str[20];
    uint64_t id_in = 0;
    ADDRINT * addr = NULL;
    UINT32 size = 0;

    const int scan_size = sscanf(line.c_str(), FORMAT_PIPE_STRING, &id_in, command_str, &addr, &size, &value);
    if(scan_size != NUMBER_ARGS_FROM_PIPE_STRING){
        std::cerr << "Can't parse value from pipe, success args: " << scan_size <<  endl;
        return false;
    }

    std::cerr << " " << id_in << " " << command_str <<" " << addr <<" " << size <<" " << value << endl;

    if(id != id_in){
        std::cerr << "wrong id message: " << id_in << " expected: " << id <<  endl;
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

static

// Move a register or literal to memory
VOID storeReg2Addr(ADDRINT * addr, ADDRINT value, UINT32 size)
{
    if(addr < (ADDRINT *)0x100)
  std::cerr << "w op0 "<< hex << addr << " reg " << hex << value << " size: " << size <<endl;
    if(isEntryInMap(addr) == false)
        return;
    std::cerr << "!s " << addr << " size : " << size << " value: " << value << endl;
    std::string   line;
    sendCommand(COMMAND_STORE, addr, size, value);
    std::getline(inFifo, line);
    ADDRINT remoteValue = 0;
    if(parseValue(line, remoteValue) == false){
        std::cerr << "error store addr: " << addr << endl;
    }

    if(remoteValue != value){
        std::cerr << "error store addr: " << addr << " expected value: " << value << " recieve: " << remoteValue << endl;
    }

}
/*
// Move a register or literal to memory
static VOID storeImmediate2Addr(ADDRINT * op0, ADDRINT op1)
{
  //std::cerr << "Immediate op0 "<< hex << op0 << " reg " << hex << op1  <<endl;
}
*/

// Move from memory to register
static ADDRINT loadAddr2Reg(ADDRINT * addr, UINT32 size)
{
    ADDRINT value;

    if(isEntryInMap(addr)){
        std::cerr << "! " << addr << " size : " << size <<  endl;
        return queryValue(addr, size);
  } else {
        PIN_SafeCopy(&value, addr, sizeof(ADDRINT));
        return value;
  }
}


static memoryTranslate * replaceMemoryMapFun( CONTEXT * context, AFUNPTR orgFuncptr,sizeMemoryTranslate_t * size ){
    cerr << "call ... size: " << size  << endl;

        PIN_CallApplicationFunction(context, PIN_ThreadId(),
            CALLINGSTD_DEFAULT, orgFuncptr, NULL,
                 PIN_PARG(memoryTranslate *), &addrMap,
                 PIN_PARG(sizeMemoryTranslate_t*), size,
                 PIN_PARG_END());

        sizeMap = *size;

        cerr << "call end"  << endl;

         cerr << "memoryTranslate : " << addrMap << endl;
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

        cout << "!Replaced free() in:"  << IMG_Name(img) << endl;
    }
}

static VOID EmulateLoad(INS ins, VOID* v)
{
    // Find the instructions that move a value from memory to a register
    if (INS_Opcode(ins) == XED_ICLASS_MOV &&
        INS_IsMemoryRead(ins) &&
        INS_OperandIsReg(ins, 0) &&
        INS_OperandIsMemory(ins, 1))
    {
      //std::cerr << "Instrumenting at " << hex << INS_Address(ins) << " " << INS_Disassemble(ins).c_str() << std::endl;

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

static VOID EmulateStore(INS ins, VOID* v)
{
    // Find the instructions that move a value from memory to a register
    if (INS_Opcode(ins) == XED_ICLASS_MOV &&
        INS_IsMemoryWrite(ins) &&
        INS_OperandIsMemory(ins, 0))
    {
      //std::cerr << "Instrumenting at " << hex << INS_Address(ins) << " " << INS_Disassemble(ins).c_str() << std::endl;

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
                           IARG_UINT32,
                        INS_OperandImmediate(ins, 1),
                       IARG_MEMORYWRITE_SIZE,
                           IARG_END);
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

    cout << "Wait OCD client..." << endl;

    if( initFifo() == false ){
        cerr << "Error open fifo file: " << KnobInputFifo.Value() 
            << " or " << KnobOutputFifo.Value() << endl;
        return -2;
    }
/*
    string line;
    while(std::getline(inFifo, line)){
    cout << "line " << line << endl;
    }
*/
    IMG_AddInstrumentFunction(ImageReplace, 0);

    INS_AddInstrumentFunction(EmulateLoad, 0);

    INS_AddInstrumentFunction(EmulateStore, 0);
    
    cout << "Start..." << endl;
    PIN_StartProgram();
    
    return 0;
}

