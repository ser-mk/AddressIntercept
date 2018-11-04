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


using namespace std;

/* ===================================================================== */
/* Global Variables */
/* ===================================================================== */


// Move a register or literal to memory
VOID storeReg2Addr(ADDRINT * op0, ADDRINT op1)
{
  std::cerr << "w op0 "<< hex << op0 << " reg " << hex << op1  <<endl;
}

// Move a register or literal to memory
VOID storeImmediate2Addr(ADDRINT * op0, ADDRINT op1)
{
  std::cerr << "Immediate op0 "<< hex << op0 << " reg " << hex << op1  <<endl;
}


// Move from memory to register
ADDRINT loadAddr2Reg(ADDRINT * addr)
{
    ADDRINT value;

    
    if(addr != (UINT64*)(0x123456-0x443)
      &&
      addr != (UINT64*)(0x123456)
      ){
     
    PIN_SafeCopy(&value, addr, sizeof(ADDRINT));
    return value;
  } else {
    std::cerr << "! " << addr <<  endl;
    return 0x0;
  }
}


memoryTranslate * replaceFun( CONTEXT * context, AFUNPTR orgFuncptr,sizeMemoryTranslate_t * size ){

    memoryTranslate * res = NULL;

    cerr << "call ... size: " << size  << endl;

        PIN_CallApplicationFunction(context, PIN_ThreadId(),
            CALLINGSTD_DEFAULT, orgFuncptr, NULL,
                 PIN_PARG(memoryTranslate *), &res,
                 PIN_PARG(sizeMemoryTranslate_t*), size,
                 PIN_PARG_END());

        cerr << "call end"  << endl;

         cerr << "memoryTranslate : " << res << endl;
    return res;
}


/* ===================================================================== */
/* Commandline Switches */
/* ===================================================================== */

INT32 Usage()
{
    cerr <<
        "Help message\n"
        "\n";
    cerr << KNOB_BASE::StringKnobSummary();
    cerr << endl;
    return -1;
}


/* ===================================================================== */
// Called every time a new image is loaded.
// Look for routines that we want to replace.
VOID ImageReplace(IMG img, VOID *v)
{
    RTN freeRtn = RTN_FindByName(img, NAME_MEMORY_MAP_FUNCTION);
    if (RTN_Valid(freeRtn))
    {
        PROTO proto_free = PROTO_Allocate( PIN_PARG(memoryTranslate *), CALLINGSTD_DEFAULT,
                                           NAME_MEMORY_MAP_FUNCTION, PIN_PARG(sizeMemoryTranslate_t*),  PIN_PARG_END() );
        
        RTN_ReplaceSignature(
            freeRtn, AFUNPTR( replaceFun ),
            IARG_PROTOTYPE, proto_free,
            IARG_CONTEXT,
            IARG_ORIG_FUNCPTR,
            IARG_FUNCARG_ENTRYPOINT_VALUE, 0,
            IARG_END);

        cout << "!Replaced free() in:"  << IMG_Name(img) << endl;
    }
}

VOID EmulateLoad(INS ins, VOID* v)
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
                       IARG_RETURN_REGS,
                       INS_OperandReg(ins, 0),
                       IARG_END);

        // Delete the instruction
        INS_Delete(ins);
    }
}

VOID EmulateStore(INS ins, VOID* v)
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
                           IARG_END);
    } else if(INS_OperandIsImmediate(ins, 1)){ 
        INS_InsertCall(ins,
                           IPOINT_BEFORE,
                           AFUNPTR(storeImmediate2Addr),
                           IARG_MEMORYWRITE_EA,
                           IARG_UINT32,
                        INS_OperandImmediate(ins, 1),
                           IARG_END);
    }

    }
}


/* ===================================================================== */

int main(int argc, CHAR *argv[])
{
    PIN_InitSymbols();

    if( PIN_Init(argc,argv) )
    {
        return Usage();
    }

    IMG_AddInstrumentFunction(ImageReplace, 0);

    INS_AddInstrumentFunction(EmulateLoad, 0);

    INS_AddInstrumentFunction(EmulateStore, 0);
    
    
    PIN_StartProgram();
    
    return 0;
}