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
#define FORMAT_PIPE_STRING "id:%ld | %s | addr: %p | size: %d | value: 0x%x | st: %s"
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
#if defined(TARGET_IA32)
static long int id = 0;
#else
static uint64_t id = 0;
#endif

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
    #if defined(TARGET_IA32)
    static long int id_in = 0;
    #else
    static uint64_t id_in = 0;
    #endif
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

static

// Move a register or literal to memory
VOID storeReg2Addr(ADDRINT * addr, ADDRINT value, UINT32 size)
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
        MAGIC_LOG(_DEBUG) << "! " << addr << " size : " << size;
        return queryValue(addr, size);
  } else {
        PIN_SafeCopy(&value, addr, sizeof(ADDRINT));
        return value;
  }
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

        MAGIC_LOG(_INFO) << "!Replaced free() in:"  << IMG_Name(img);
    }
}

//__attribute__((unused))
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

typedef struct memaccess {
    ADDRINT memAddr;
    UINT32  accessType; // 1==read, 2==read2, 3==write
    INT32   memAccessSize;
} MEM_ACCESS;

typedef struct memaccessrec
{
    int numMemAccesses;
    ADDRINT ip;
    MEM_ACCESS memAccesses[2];
} MEM_ACCESS_REC;
MEM_ACCESS_REC memAccessRec;
BOOL haveReadAccess = FALSE;
BOOL haveRead2Access = FALSE;
BOOL haveWriteAccess = FALSE;
BOOL haveReadAndWriteAccess = FALSE;
int CompareMemAccess (MEM_ACCESS *memAccess, PIN_MEM_ACCESS_INFO *pinMemAccessInfo)
{
    if (memAccess->accessType==2)
    {
        haveRead2Access = TRUE;
    }
    else if (memAccess->accessType == 1)
    {
        haveReadAccess = TRUE;
    }
    else
    {
         haveWriteAccess = TRUE;
    }
    return ((memAccess->memAddr==pinMemAccessInfo->memoryAddress)
            &&(memAccess->memAccessSize==(INT32)(pinMemAccessInfo->bytesAccessed))
            && ((pinMemAccessInfo->memopType==PIN_MEMOP_LOAD &&
                (memAccess->accessType==1 || memAccess->accessType==2))
                || (pinMemAccessInfo->memopType==PIN_MEMOP_STORE &&  memAccess->accessType==3)));
}

VOID CompareMultiMemAccess(ADDRINT ip, PIN_MULTI_MEM_ACCESS_INFO* multiMemAccessInfo)
{
    /*printf ("offsetof(PIN_MULTI_MEM_ACCESS_INFO, memop) %x\noffsetof(PIN_MEM_ACCESS_INFO, memoryAddress) %x\noffsetof(PIN_MEM_ACCESS_INFO, memopType) %x\noffsetof(PIN_MEM_ACCESS_INFO, bytesAccessed) %x\noffsetof(PIN_MEM_ACCESS_INFO, maskOn) %x\n",
            offsetof(PIN_MULTI_MEM_ACCESS_INFO, memop),
            offsetof(PIN_MEM_ACCESS_INFO, memoryAddress),
            offsetof(PIN_MEM_ACCESS_INFO, memopType),
            offsetof(PIN_MEM_ACCESS_INFO, bytesAccessed),
            offsetof(PIN_MEM_ACCESS_INFO, maskOn));*/
    if ((int)multiMemAccessInfo->numberOfMemops != memAccessRec.numMemAccesses)
    {
        printf ("got different number of mem accesses at ip %p multiMemAccessInfo->numberOfMemops %d memAccessRec.numMemAccesses %d\n",
                (void *)ip, multiMemAccessInfo->numberOfMemops, memAccessRec.numMemAccesses);
        fflush (stdout);
        //exit (1);
    }
    if (memAccessRec.ip != ip)
    {
        printf ("got different ips ip %p memAccessRec.ip %p\n",
                (void *)ip, (void *)memAccessRec.ip);
        fflush (stdout);
        //exit (1);
    }


    if (memAccessRec.numMemAccesses == 1)
    {
        PIN_MEM_ACCESS_INFO *pinMemAccessInfo = &(multiMemAccessInfo->memop[0]);
        if (!CompareMemAccess(&memAccessRec.memAccesses[0], pinMemAccessInfo))
        {
            printf ("different mem accesses at ip %p\n", (void *)ip);
            printf ("  memAccessRec.accessType %d multiAccess.accessType %d\n", memAccessRec.memAccesses[0].accessType, pinMemAccessInfo->memopType);
            printf ("  memAccessRec.memAddr %p multiAccess.memAddr %p\n", (void *)memAccessRec.memAccesses[0].memAddr, (void *)pinMemAccessInfo->memoryAddress);
            printf ("  memAccessRec.memAccessSize %d multiAccess.memAccessSize %d\n", memAccessRec.memAccesses[0].memAccessSize,
                    pinMemAccessInfo->bytesAccessed);
            fflush (stdout);
            //exit (1);
        }
    }
    else
    {
        if (memAccessRec.memAccesses[1].accessType == 1 && memAccessRec.memAccesses[0].accessType==3)
        {
            haveReadAndWriteAccess = TRUE;
        }
        else if (memAccessRec.memAccesses[0].accessType == 1 && memAccessRec.memAccesses[1].accessType==3)
        {
            haveReadAndWriteAccess = TRUE;
        }

        if (! ((CompareMemAccess(&memAccessRec.memAccesses[0], &(multiMemAccessInfo->memop[0]))
                && CompareMemAccess(&memAccessRec.memAccesses[1], &(multiMemAccessInfo->memop[1])))
               ||(CompareMemAccess(&memAccessRec.memAccesses[0], &(multiMemAccessInfo->memop[1]))
                  && CompareMemAccess(&memAccessRec.memAccesses[1], &(multiMemAccessInfo->memop[0])))))
        {
            printf ("different mem accesses at ip %p\n", (void *)ip);
            printf ("  memAccessRec[0].accessType %d memAccessRec[1].accessType %d\n", memAccessRec.memAccesses[0].accessType, memAccessRec.memAccesses[1].accessType);
            printf ("  memAccessRec[0].memAddr %p memAccessRec[1].memAddr %p\n", (void *)memAccessRec.memAccesses[0].memAddr, (void *)memAccessRec.memAccesses[1].memAddr);
            printf ("  memAccessRec[0].memAccessSize %d memAccessRec[1].memAccessSize %d\n", memAccessRec.memAccesses[0].memAccessSize,
                    memAccessRec.memAccesses[1].memAccessSize);
            printf ("  multiMemAccessInfo->memop[0].memopType %d multiMemAccessInfo->memop[1].memopType %d\n", multiMemAccessInfo->memop[0].memopType, multiMemAccessInfo->memop[1].memopType);
            printf ("  multiMemAccessInfo->memop[0].memoryAddress %p multiMemAccessInfo->memop[1].memoryAddress %p\n",
                    (void *)multiMemAccessInfo->memop[0].memoryAddress,  (void *)multiMemAccessInfo->memop[1].memoryAddress);
            printf ("  multiMemAccessInfo->memop[0].bytesAccessed %d multiMemAccessInfo->memop[1].bytesAccessed %d\n", multiMemAccessInfo->memop[0].bytesAccessed, multiMemAccessInfo->memop[1].bytesAccessed);
            fflush (stdout);
            //exit (1);
        }
    }
    memAccessRec.numMemAccesses = 0;
}

//__attribute__((unused))
static VOID EmulateStore(INS ins, VOID* v)
{
    // Find the instructions that move a value from memory to a register
    if (INS_Opcode(ins) == XED_ICLASS_MOV &&
        INS_IsMemoryWrite(ins) &&
        INS_OperandIsMemory(ins, 0))
    {
      std::cerr << "Instrumenting at " << hex << INS_Address(ins) << " " << INS_Disassemble(ins).c_str() << " HAS " << INS_hasKnownMemorySize(ins) << std::endl;
        if(INS_OperandIsReg(ins, 1)){
            INS_InsertCall(ins,
                           IPOINT_BEFORE,
                           AFUNPTR(storeReg2Addr),
                           IARG_MEMORYWRITE_EA,
                           IARG_REG_VALUE,
                           INS_OperandReg(ins, 1),
                           IARG_MEMORYWRITE_SIZE,
                           IARG_END);
    }

        else if(INS_OperandIsImmediate(ins, 1) && !INS_hasKnownMemorySize(ins)){
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
    } else if(INS_OperandIsImmediate(ins, 1) && INS_hasKnownMemorySize(ins)){
            INS_InsertCall(ins, IPOINT_BEFORE, (AFUNPTR)CompareMultiMemAccess,
                                    IARG_INST_PTR,
                                    IARG_MULTI_MEMORYACCESS_EA,
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

    Log::setGLevel((LevelDebug)(KnobLevelDebug.Value() & 0xF));

    MAGIC_LOG(_INFO) << "Wait OCD client...";

    if( initFifo() == false ){
        MAGIC_LOG(_ERROR) << "Error open fifo file: " << KnobInputFifo.Value()
            << " or " << KnobOutputFifo.Value();
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
    
    MAGIC_LOG(_DEBUG) << "Start...";
    PIN_StartProgram();
    
    return 0;
}

