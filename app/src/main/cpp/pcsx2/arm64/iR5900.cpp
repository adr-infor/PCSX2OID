// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "iR5900.h"
#include "R5900OpcodeTables.h"
#include "VU.h"
#include "vtlb.h"
#include "IopBios.h"
#include "IopHw.h"
#include "Common.h"
#include "VMManager.h"
#include "R5900.h"
#include "iR5900Arit.h"
#include "iR5900Shift.h"
#include "iR5900Move.h"
#include "iR5900LoadStore.h"
#include "iR5900Branch.h"
#include "iR5900Jump.h"
#include "iR5900AritImm.h"
#include "iR5900MultDiv.h"
#include "iR5900COP0.h"
#include "iR5900COP1.h"
#include "iR5900COP2.h"

#include "common/AlignedMalloc.h"
#include "common/Perf.h"
#include "DebugTools/Breakpoints.h"

using namespace R5900;

// Constant propagation globals
GPR_reg64 g_cpuConstRegs[32];
u32 g_cpuHasConstReg = 0;
u32 g_cpuFlushedConstReg = 0;
bool g_cpuFlushedPC = false;
bool g_cpuFlushedCode = false;
bool g_maySignalException = false;

static bool eeRecNeedsReset = false;
static bool eeCpuExecuting = false;
static bool eeRecExitRequested = false;

#define PC_GETBLOCK(x) PC_GETBLOCK_(x, recLUT)

u32 maxrecmem = 0;
alignas(16) static uptr recLUT[_64kb];
alignas(16) static u32 hwLUT[_64kb];

static __fi u32 HWADDR(u32 mem) { return hwLUT[mem >> 16] + mem; }

u32 s_nBlockCycles = 0; // cycles of current block recompiling
bool s_nBlockInterlocked = false; // Block is VU0 interlocked
u32 pc; // recompiler pc
int g_branch; // set for branch

alignas(16) GPR_reg64 g_cpuConstRegs[32] = {};
u32 g_cpuHasConstReg = 0, g_cpuFlushedConstReg = 0;
bool g_cpuFlushedPC, g_cpuFlushedCode, g_recompilingDelaySlot, g_maySignalException;

eeProfiler EE::Profiler;

////////////////////////////////////////////////////////////////
// Static Private Variables - R5900 Dynarec ARM64

static BASEBLOCK* recRAM = nullptr; // and the ptr to the blocks here
static BASEBLOCK* recROM = nullptr; // and here
static BASEBLOCK* recROM1 = nullptr; // also here
static BASEBLOCK* recROM2 = nullptr; // also here

static BaseBlocks recBlocks;
static u8* recPtr = nullptr;
static u8* recPtrEnd = nullptr;
EEINST* s_pInstCache = nullptr;
static u32 s_nInstCacheSize = 0;

static BASEBLOCK* s_pCurBlock = nullptr;
static BASEBLOCKEX* s_pCurBlockEx = nullptr;
u32 s_nEndBlock = 0; // what pc the current block ends
u32 s_branchTo;
static bool s_nBlockFF;

// save states for branches
GPR_reg64 s_saveConstRegs[32];
static u32 s_saveHasConstReg = 0, s_saveFlushedConstReg = 0;
static EEINST* s_psaveInstInfo = nullptr;

static u32 s_savenBlockCycles = 0;

static void iBranchTest(u32 newpc = 0xffffffff);
static void ClearRecLUT(BASEBLOCK* base, int count);
static u32 scaleblockcycles();
static void recExitExecution();

// =====================================================================================================
//  R5900 Dispatchers
// =====================================================================================================

static void recRecompile(const u32 startpc);
static void dyna_block_discard(u32 start, u32 sz);
static void dyna_page_reset(u32 start, u32 sz);

static const void* DispatcherEvent = nullptr;
static const void* DispatcherReg = nullptr;
static const void* JITCompile = nullptr;
static const void* EnterRecompiledCode = nullptr;
static const void* DispatchBlockDiscard = nullptr;
static const void* DispatchPageReset = nullptr;

static void recEventTest()
{
	_cpuEventTest_Shared();

	if (eeRecExitRequested)
	{
		eeRecExitRequested = false;
		recExitExecution();
	}
}

// The address for all cleared blocks.  It recompiles the current pc and then
// dispatches to the recompiled block address.
static const void* _DynGen_JITCompile()
{
	pxAssertMsg(DispatcherReg != NULL, "Please compile the DispatcherReg subroutine *before* JITComple.  Thanks.");

	armAlignAsmPtr();
	u8* retval = armGetCurrentCodePointer();

	// Load PC and recompile
	armLoad(RXARG1, PTR_CPU(cpuRegs.pc));
	armEmitCall(reinterpret_cast<void*>(recRecompile));

	// Jump to dispatcher
	armEmitJmp(DispatcherReg);

	return retval;
}

static void recReserve()
{
	if (recRAM)
		return; // already reserved

	// Allocate recompiler memory
	recRAM = (BASEBLOCK*)SysMemory::AllocateAlignedMemory(0x200000, 4096);
	recROM = (BASEBLOCK*)SysMemory::AllocateAlignedMemory(0x400000, 4096);
	recROM1 = (BASEBLOCK*)SysMemory::AllocateAlignedMemory(0x400000, 4096);
	recROM2 = (BASEBLOCK*)SysMemory::AllocateAlignedMemory(0x400000, 4096);

	if (!recRAM || !recROM || !recROM1 || !recROM2)
	{
		SysMemory::FreeAlignedMemory(recRAM);
		SysMemory::FreeAlignedMemory(recROM);
		SysMemory::FreeAlignedMemory(recROM1);
		SysMemory::FreeAlignedMemory(recROM2);
		recRAM = nullptr;
		pxFailRel("Failed to allocate recompiler memory");
		return;
	}

	// Initialize LUTs
	ClearRecLUT(recRAM, 0x200000 / 4);
	ClearRecLUT(recROM, 0x400000 / 4);
	ClearRecLUT(recROM1, 0x400000 / 4);
	ClearRecLUT(recROM2, 0x400000 / 4);

	// Allocate code buffer
	u8* recCode = SysMemory::AllocateAlignedMemory(EE_RECOMPILE_SIZE, 4096);
	if (!recCode)
	{
		SysMemory::FreeAlignedMemory(recRAM);
		SysMemory::FreeAlignedMemory(recROM);
		SysMemory::FreeAlignedMemory(recROM1);
		SysMemory::FreeAlignedMemory(recROM2);
		recRAM = nullptr;
		pxFailRel("Failed to allocate recompiler code buffer");
		return;
	}

	recPtr = recCode;
	recPtrEnd = recCode + EE_RECOMPILE_SIZE;

	// Generate dispatchers
	_DynGen_Dispatchers();

	eeRecNeedsReset = true;
}

static void recShutdown()
{
	if (!recRAM)
		return;

	SysMemory::FreeAlignedMemory(recRAM);
	SysMemory::FreeAlignedMemory(recROM);
	SysMemory::FreeAlignedMemory(recROM1);
	SysMemory::FreeAlignedMemory(recROM2);

	if (recPtr)
		SysMemory::FreeAlignedMemory(recPtr);

	recRAM = nullptr;
	recROM = nullptr;
	recROM1 = nullptr;
	recROM2 = nullptr;
	recPtr = nullptr;
	recPtrEnd = nullptr;

	if (s_pInstCache)
	{
		free(s_pInstCache);
		s_pInstCache = nullptr;
		s_nInstCacheSize = 0;
	}

	recBlocks.Reset();
}

static void recResetEE()
{
	if (!recRAM)
		return;

	// Clear all blocks
	recBlocks.Reset();

	// Reset LUTs
	ClearRecLUT(recRAM, 0x200000 / 4);
	ClearRecLUT(recROM, 0x400000 / 4);
	ClearRecLUT(recROM1, 0x400000 / 4);
	ClearRecLUT(recROM2, 0x400000 / 4);

	// Reset code buffer
	recPtr = (u8*)SysMemory::GetEERecStart();
	recPtrEnd = recPtr + EE_RECOMPILE_SIZE;

	// Reset constant propagation
	g_cpuHasConstReg = 0;
	g_cpuFlushedConstReg = 0;
	std::memset(g_cpuConstRegs, 0, sizeof(g_cpuConstRegs));

	maxrecmem = 0;
	pc = 0;
	g_branch = 0;
	s_nBlockCycles = 0;
	s_nBlockInterlocked = false;

	eeRecNeedsReset = false;
}

static s32 recExecute(s32 eeCycles)
{
	if (!recRAM)
		return 0;

	// Execute recompiled code
	eeCpuExecuting = true;
	
	while (eeCpuExecuting)
	{
		// Get the current block
		BASEBLOCK* pblock = PC_GETBLOCK(cpuRegs.pc);
		
		// If the block is not recompiled, recompile it
		if (pblock->GetFnptr() == (uptr)JITCompile)
		{
			recRecompile(cpuRegs.pc);
			pblock = PC_GETBLOCK(cpuRegs.pc);
		}
		
		// Execute the recompiled block
		// Cast the function pointer to the correct type and call it
		typedef void (*RecBlockFunc)();
		RecBlockFunc func = (RecBlockFunc)pblock->GetFnptr();
		
		if (func)
		{
			func();
		}
		else
		{
			// Fallback to interpreter if block is not available
			intCpu.ExecuteBlock();
		}
		
		// Check for exit request
		if (eeRecExitRequested)
		{
			eeCpuExecuting = false;
		}
	}
	
	eeRecExitRequested = false;
	return eeCycles;
}

static s32 recStep()
{
	if (!recRAM)
		return 0;

	// TODO: Implement single step
	return 0;
}

static void recSafeExitExecution()
{
	eeRecExitRequested = true;
}

static void recCancelInstruction()
{
	// TODO: Implement
}

static void recClear(u32 addr, u32 size)
{
	if (!recRAM)
		return;

	// Clear blocks in the specified range
	dyna_block_discard(addr, size);
}

static void ClearRecLUT(BASEBLOCK* base, int count)
{
	std::memset(recLUT, 0, sizeof(recLUT));
	std::memset(hwLUT, 0, sizeof(hwLUT));

	// Setup LUT for RAM
	for (int i = 0; i < count; i++)
	{
		recLUT[(0x00000000 >> 16) + i] = (uptr)&base[i];
		hwLUT[(0x00000000 >> 16) + i] = 0x00000000;
	}
}

// called when jumping to variable pc address
static const void* _DynGen_DispatcherReg()
{
	armAlignAsmPtr();
	u8* retval = armGetCurrentCodePointer();

	// C equivalent:
	// u32 addr = cpuRegs.pc;
	// void(**base)() = (void(**)())recLUT[addr >> 16];
	// base[addr >> 2]();

	armLoad(EAX, PTR_CPU(cpuRegs.pc));
	armAsm->Lsr(ECX, EAX, 16);
	armAsm->Ldr(RXVIXLSCRATCH, PTR_CPU(recLUT));
	armAsm->Ldr(RCX, a64::MemOperand(RXVIXLSCRATCH, ECX, a64::LSL, 3));
	armAsm->Lsr(EAX, EAX, 2);
	armAsm->Ldr(RAX, a64::MemOperand(RCX, EAX, a64::LSL, 3));
	armAsm->Br(RAX);

	return retval;
}

static const void* _DynGen_DispatcherEvent()
{
	u8* retval = armGetCurrentCodePointer();

	armEmitCall(reinterpret_cast<const void*>(recEventTest));

	return retval;
}

static const void* _DynGen_EnterRecompiledCode()
{
	pxAssertMsg(DispatcherReg, "Dynamically generated dispatchers are required prior to generating EnterRecompiledCode!");

	armAlignAsmPtr();
	u8* retval = armGetCurrentCodePointer();

#ifdef _WIN32
	static constexpr u32 stack_size = 32 + 8;
#else
	static constexpr u32 stack_size = 16;
#endif

	armAsm->Sub(a64::sp, a64::sp, stack_size);

	// From memory to registry
	armMoveAddressToReg(RSTATE_x29, &recLUT);
	armMoveAddressToReg(RSTATE_PSX, &psxRegs);
	armMoveAddressToReg(RSTATE_CPU, &g_cpuRegistersPack);

	if (CHECK_FASTMEM) {
		armAsm->Ldr(RFASTMEMBASE, PTR_CPU(vtlbdata.fastmem_base));
	}

	armEmitJmp(DispatcherReg);

	return retval;
}

static const void* _DynGen_DispatchBlockDiscard()
{
	u8* retval = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(dyna_block_discard));
	armEmitJmp(DispatcherReg);
	return retval;
}

static const void* _DynGen_DispatchPageReset()
{
	u8* retval = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(dyna_page_reset));
	armEmitJmp(DispatcherReg);
	return retval;
}

static void _DynGen_Dispatchers()
{
	const u8* start = armGetCurrentCodePointer();

	DispatcherEvent = _DynGen_DispatcherEvent();
	DispatcherReg = _DynGen_DispatcherReg();
	JITCompile = _DynGen_JITCompile();
	EnterRecompiledCode = _DynGen_EnterRecompiledCode();
	DispatchBlockDiscard = _DynGen_DispatchBlockDiscard();
	DispatchPageReset = _DynGen_DispatchPageReset();

	recBlocks.SetJITCompile(JITCompile);

	Perf::any.Register(start, static_cast<u32>(armGetCurrentCodePointer() - start), "EE Dispatcher");
}

static void recRecompile(const u32 startpc)
{
	// Initialize constant propagation state
	g_cpuHasConstReg = 0;
	g_cpuFlushedConstReg = 0;
	std::memset(g_cpuConstRegs, 0, sizeof(g_cpuConstRegs));
	g_cpuConstRegs[0].UD[0] = 0; // R0 is always 0
	g_cpuHasConstReg |= 1; // Mark R0 as constant
	g_cpuFlushedConstReg |= 1; // Mark R0 as flushed

	// Initialize register allocation
	_initArm64regs();
	_initQregs();

	// Start code generation
	armAsm->Reset();
	
	u32 pc = startpc;
	u32 s_nEndBlock = startpc + 1000; // Simple block size limit
	int branch = 0;
	bool recompiling = true;

	// Simple loop to recompile instructions
	while (pc < s_nEndBlock && recompiling)
	{
		// Check if we have enough space
		if (recPtr >= recPtrEnd - 0x10000)
		{
			Console.Error("R5900: Out of recompile memory");
			recClear();
			return;
		}

		// Get instruction
		cpuRegs.code = memRead32(pc);
		u32 opcode = cpuRegs.code >> 26;

		// Dispatch to appropriate recompiler function
		// This is a simplified dispatch - a full implementation would use the opcode table
		switch (opcode)
		{
			case 0: // SPECIAL
				{
					u32 funct = cpuRegs.code & 0x3F;
					switch (funct)
					{
						case 0x20: recADD(0); break;
						case 0x21: recADDU(0); break;
						case 0x22: recSUB(0); break;
						case 0x23: recSUBU(0); break;
						case 0x24: recAND(0); break;
						case 0x25: recOR(0); break;
						case 0x26: recXOR(0); break;
						case 0x27: recNOR(0); break;
						case 0x2A: recSLT(0); break;
						case 0x2B: recSLTU(0); break;
						case 0x00: recSLL(0); break;
						case 0x02: recSRL(0); break;
						case 0x03: recSRA(0); break;
						case 0x04: recSLLV(0); break;
						case 0x06: recSRLV(0); break;
						case 0x07: recSRAV(0); break;
						case 0x08: recJR(0); branch = 1; break;
						case 0x09: recJALR(0); branch = 1; break;
						case 0x10: recMFHI(0); break;
						case 0x11: recMTHI(0); break;
						case 0x12: recMFLO(0); break;
						case 0x13: recMTLO(0); break;
						case 0x18: recMULT(0); break;
						case 0x19: recMULTU(0); break;
						case 0x1A: recDIV(0); break;
						case 0x1B: recDIVU(0); break;
						default: recCall(Interp::UNKNOWN); break;
					}
				}
				break;
			case 1: // REGIMM
				{
					u32 rt = (cpuRegs.code >> 16) & 0x1F;
					switch (rt)
					{
						case 0x00: recBLTZ(0); branch = 1; break;
						case 0x01: recBGEZ(0); branch = 1; break;
						case 0x02: recBLTZL(0); branch = 1; break;
						case 0x03: recBGEZL(0); branch = 1; break;
						default: recCall(Interp::UNKNOWN); break;
					}
				}
				break;
			case 2: recJ(0); branch = 1; break;
			case 3: recJAL(0); branch = 1; break;
			case 4: recBEQ(0); branch = 1; break;
			case 5: recBNE(0); branch = 1; break;
			case 6: recBLEZ(0); branch = 1; break;
			case 7: recBGTZ(0); branch = 1; break;
			case 8: recDADDI(0); break;
			case 9: recDADDIU(0); break;
			case 10: recCall(Interp::UNKNOWN); break; // TLB
			case 11: recCall(Interp::UNKNOWN); break; // TLB
			case 12: recCall(Interp::UNKNOWN); break; // TLB
			case 13: recCall(Interp::UNKNOWN); break; // TLB
			case 14: recCall(Interp::UNKNOWN); break; // TLB
			case 15: recLUI(0); break;
			case 16: recMFC0(0); break; // COP0
			case 17: recMFC1(0); break; // COP1
			case 18: recMFC2(0); break; // COP2
			case 20: recBEQL(0); branch = 1; break;
			case 21: recBNEL(0); branch = 1; break;
			case 22: recBLEZL(0); branch = 1; break;
			case 23: recBGTZL(0); branch = 1; break;
			case 24: recDADDI(0); break;
			case 25: recDADDIU(0); break;
			case 26: recLDL(0); break;
			case 27: recLDR(0); break;
			case 28: recCall(Interp::UNKNOWN); break; // TLB
			case 29: recCall(Interp::UNKNOWN); break; // TLB
			case 30: recCall(Interp::UNKNOWN); break; // TLB
			case 31: recCall(Interp::UNKNOWN); break; // TLB
			case 32: recLB(0); break;
			case 33: recLH(0); break;
			case 34: recLWL(0); break;
			case 35: recLW(0); break;
			case 36: recLBU(0); break;
			case 37: recLHU(0); break;
			case 38: recLWR(0); break;
			case 39: recLWU(0); break;
			case 40: recSB(0); break;
			case 41: recSH(0); break;
			case 42: recSWL(0); break;
			case 43: recSW(0); break;
			case 44: recSDL(0); break;
			case 45: recSDR(0); break;
			case 46: recSWR(0); break;
			case 47: recCACHE(0); break;
			case 48: recLL(0); break;
			case 49: recLWC1(0); break;
			case 50: recLWC2(0); break;
			case 51: recPREF(0); break;
			case 52: recLD(0); break;
			case 53: recLDC1(0); break;
			case 54: recLDC2(0); break;
			case 55: recLD(0); break;
			case 56: recSC(0); break;
			case 57: recSWC1(0); break;
			case 58: recSWC2(0); break;
			case 59: recCall(Interp::UNKNOWN); break;
			case 60: recSD(0); break;
			case 61: recSDC1(0); break;
			case 62: recSDC2(0); break;
			case 63: recSD(0); break;
			default: recCall(Interp::UNKNOWN); break;
		}
		
		pc += 4;

		// Simple block termination conditions
		if (branch != 0)
		{
			recompiling = false;
		}
	}

	// Flush all registers
	_flushArm64regs();
	_flushConstRegs();

	// Return to dispatcher
	armEmitJmp(DispatcherReg);

	// Update block info
	// Set the block function pointer to the start of the recompiled code
	s_pCurBlock->SetFnptr((uptr)recPtr);
}

static void dyna_block_discard(u32 start, u32 sz)
{
	// Clear the recompiled block from the LUT
	u32 page = start >> 16;
	for (u32 i = 0; i < sz; i += 4)
	{
		u32 addr = (start + i) >> 16;
		if (addr == page)
		{
			recLUT[addr] = 0;
		}
	}
}

static void dyna_page_reset(u32 start, u32 sz)
{
	// Reset the page in the LUT
	u32 page = start >> 16;
	recLUT[page] = 0;
}

static void recExitExecution()
{
	eeCpuExecuting = false;
}

static void iBranchTest(u32 newpc)
{
	// TODO: Implement branch test
}

static u32 scaleblockcycles()
{
	// TODO: Implement cycle scaling
	return s_nBlockCycles;
}

// Basic stub implementations for now - these need full implementation

void recADD(int info) { R5900::Dynarec::OpcodeImpl::recADD(info); }
void recADDU(int info) { R5900::Dynarec::OpcodeImpl::recADDU(info); }
void recDADDI(int info) { R5900::Dynarec::OpcodeImpl::recDADDI(info); }
void recDADDIU(int info) { R5900::Dynarec::OpcodeImpl::recDADDIU(info); }
void recDADD(int info) { R5900::Dynarec::OpcodeImpl::recDADD(info); }
void recDADDU(int info) { R5900::Dynarec::OpcodeImpl::recDADDU(info); }
void recSUB(int info) { R5900::Dynarec::OpcodeImpl::recSUB(info); }
void recSUBU(int info) { R5900::Dynarec::OpcodeImpl::recSUBU(info); }
void recDSUBI(int info) { R5900::Dynarec::OpcodeImpl::recDSUBI(info); }
void recDSUBIU(int info) { R5900::Dynarec::OpcodeImpl::recDSUBIU(info); }
void recDSUB(int info) { R5900::Dynarec::OpcodeImpl::recDSUB(info); }
void recDSUBU(int info) { R5900::Dynarec::OpcodeImpl::recDSUBU(info); }
void recAND(int info) { R5900::Dynarec::OpcodeImpl::recAND(info); }
void recOR(int info) { R5900::Dynarec::OpcodeImpl::recOR(info); }
void recXOR(int info) { R5900::Dynarec::OpcodeImpl::recXOR(info); }
void recNOR(int info) { R5900::Dynarec::OpcodeImpl::recNOR(info); }
void recSLT(int info) { R5900::Dynarec::OpcodeImpl::recSLT(info); }
void recSLTI(int info) { R5900::Dynarec::OpcodeImpl::recSLTI(info); }
void recSLTIU(int info) { R5900::Dynarec::OpcodeImpl::recSLTIU(info); }
void recSLTU(int info) { R5900::Dynarec::OpcodeImpl::recSLTU(info); }

void recSLL(int info) { R5900::Dynarec::OpcodeImpl::recSLL(info); }
void recSRL(int info) { R5900::Dynarec::OpcodeImpl::recSRL(info); }
void recSRA(int info) { R5900::Dynarec::OpcodeImpl::recSRA(info); }
void recSLLV(int info) { R5900::Dynarec::OpcodeImpl::recSLLV(info); }
void recSRLV(int info) { R5900::Dynarec::OpcodeImpl::recSRLV(info); }
void recSRAV(int info) { R5900::Dynarec::OpcodeImpl::recSRAV(info); }

void recMULT(int info) { R5900::Dynarec::OpcodeImpl::recMULT(info); }
void recMULTU(int info) { R5900::Dynarec::OpcodeImpl::recMULTU(info); }
void recDMULT(int info) { R5900::Dynarec::OpcodeImpl::recDMULT(info); }
void recDMULTU(int info) { R5900::Dynarec::OpcodeImpl::recDMULTU(info); }
void recDIV(int info) { R5900::Dynarec::OpcodeImpl::recDIV(info); }
void recDIVU(int info) { R5900::Dynarec::OpcodeImpl::recDIVU(info); }
void recDDIV(int info) { R5900::Dynarec::OpcodeImpl::recDDIV(info); }
void recDDIVU(int info) { R5900::Dynarec::OpcodeImpl::recDDIVU(info); }

void recLUI(int info) { REC_FUNC_DEL(LUI, _Rt_); }
void recCACHE(int info) { REC_FUNC(CACHE); }
void recLL(int info) { REC_FUNC(LL); }
void recLWC1(int info) { REC_FUNC_DEL(LWC1, _Ft_); }
void recLWC2(int info) { REC_FUNC_DEL(LWC2, _Rt_); }
void recPREF(int info) { REC_FUNC(PREF); }
void recLDC1(int info) { REC_FUNC_DEL(LDC1, _Ft_); }
void recLDC2(int info) { REC_FUNC_DEL(LDC2, _Rt_); }
void recSC(int info) { REC_FUNC(SC); }
void recSWC1(int info) { REC_FUNC_DEL(SWC1, _Ft_); }
void recSWC2(int info) { REC_FUNC(SWC2); }
void recSDC1(int info) { REC_FUNC_DEL(SDC1, _Ft_); }
void recSDC2(int info) { REC_FUNC(SDC2); }
void recLWU(int info) { REC_FUNC(LWU); }

void recMFC0(int info) { R5900::Dynarec::OpcodeImpl::recMFC0(info); }
void recMTC0(int info) { R5900::Dynarec::OpcodeImpl::recMTC0(info); }
void recCFC0(int info) { R5900::Dynarec::OpcodeImpl::recCFC0(info); }
void recCTC0(int info) { R5900::Dynarec::OpcodeImpl::recCTC0(info); }
void recERET(int info) { R5900::Dynarec::OpcodeImpl::recERET(info); }

void recMFC1(int info) { R5900::Dynarec::OpcodeImpl::recMFC1(info); }
void recMTC1(int info) { R5900::Dynarec::OpcodeImpl::recMTC1(info); }
void recCFC1(int info) { R5900::Dynarec::OpcodeImpl::recCFC1(info); }
void recCTC1(int info) { R5900::Dynarec::OpcodeImpl::recCTC1(info); }
void recLWC1(int info) { R5900::Dynarec::OpcodeImpl::recLWC1(info); }
void recSWC1(int info) { R5900::Dynarec::OpcodeImpl::recSWC1(info); }
void recLDC1(int info) { R5900::Dynarec::OpcodeImpl::recLDC1(info); }
void recSDC1(int info) { R5900::Dynarec::OpcodeImpl::recSDC1(info); }

void recMFC2(int info) { R5900::Dynarec::OpcodeImpl::recMFC2(info); }
void recMTC2(int info) { R5900::Dynarec::OpcodeImpl::recMTC2(info); }
void recCFC2(int info) { R5900::Dynarec::OpcodeImpl::recCFC2(info); }
void recCTC2(int info) { R5900::Dynarec::OpcodeImpl::recCTC2(info); }
void recLWC2(int info) { R5900::Dynarec::OpcodeImpl::recLWC2(info); }
void recSWC2(int info) { R5900::Dynarec::OpcodeImpl::recSWC2(info); }
void recLDC2(int info) { R5900::Dynarec::OpcodeImpl::recLDC2(info); }
void recSDC2(int info) { R5900::Dynarec::OpcodeImpl::recSDC2(info); }

void recBEQ(int info) { R5900::Dynarec::OpcodeImpl::recBEQ(info); }
void recBNE(int info) { R5900::Dynarec::OpcodeImpl::recBNE(info); }
void recBLTZ(int info) { R5900::Dynarec::OpcodeImpl::recBLTZ(info); }
void recBGEZ(int info) { R5900::Dynarec::OpcodeImpl::recBGEZ(info); }
void recBLEZ(int info) { R5900::Dynarec::OpcodeImpl::recBLEZ(info); }
void recBGTZ(int info) { R5900::Dynarec::OpcodeImpl::recBGTZ(info); }
void recBEQL(int info) { R5900::Dynarec::OpcodeImpl::recBEQL(info); }
void recBNEL(int info) { R5900::Dynarec::OpcodeImpl::recBNEL(info); }
void recBLTZL(int info) { R5900::Dynarec::OpcodeImpl::recBLTZL(info); }
void recBGEZL(int info) { R5900::Dynarec::OpcodeImpl::recBGEZL(info); }
void recBLEZL(int info) { R5900::Dynarec::OpcodeImpl::recBLEZL(info); }
void recBGTZL(int info) { R5900::Dynarec::OpcodeImpl::recBGTZL(info); }

void recJ(int info) { R5900::Dynarec::OpcodeImpl::recJ(info); }
void recJAL(int info) { R5900::Dynarec::OpcodeImpl::recJAL(info); }
void recJR(int info) { R5900::Dynarec::OpcodeImpl::recJR(info); }
void recJALR(int info) { R5900::Dynarec::OpcodeImpl::recJALR(info); }

void recLB(int info) { R5900::Dynarec::OpcodeImpl::recLB(info); }
void recLBU(int info) { R5900::Dynarec::OpcodeImpl::recLBU(info); }
void recLH(int info) { R5900::Dynarec::OpcodeImpl::recLH(info); }
void recLHU(int info) { R5900::Dynarec::OpcodeImpl::recLHU(info); }
void recLW(int info) { R5900::Dynarec::OpcodeImpl::recLW(info); }
void recLWL(int info) { R5900::Dynarec::OpcodeImpl::recLWL(info); }
void recLWR(int info) { R5900::Dynarec::OpcodeImpl::recLWR(info); }
void recLD(int info) { R5900::Dynarec::OpcodeImpl::recLD(info); }
void recLDL(int info) { R5900::Dynarec::OpcodeImpl::recLDL(info); }
void recLDR(int info) { R5900::Dynarec::OpcodeImpl::recLDR(info); }
void recLQ(int info) { R5900::Dynarec::OpcodeImpl::recLQ(info); }

void recSB(int info) { R5900::Dynarec::OpcodeImpl::recSB(info); }
void recSH(int info) { R5900::Dynarec::OpcodeImpl::recSH(info); }
void recSW(int info) { R5900::Dynarec::OpcodeImpl::recSW(info); }
void recSWL(int info) { R5900::Dynarec::OpcodeImpl::recSWL(info); }
void recSWR(int info) { R5900::Dynarec::OpcodeImpl::recSWR(info); }
void recSD(int info) { R5900::Dynarec::OpcodeImpl::recSD(info); }
void recSDL(int info) { R5900::Dynarec::OpcodeImpl::recSDL(info); }
void recSDR(int info) { R5900::Dynarec::OpcodeImpl::recSDR(info); }
void recSQ(int info) { R5900::Dynarec::OpcodeImpl::recSQ(info); }

void recLWC1(int info) { REC_FUNC_DEL(LWC1, _Ft_); }
void recSWC1(int info) { REC_FUNC(SWC1); }
void recMTC1(int info) { REC_FUNC(MTC1); }
void recMFC1(int info) { REC_FUNC_DEL(MFC1, _Ft_); }
void recCTC1(int info) { REC_FUNC(CTC1); }
void recCFC1(int info) { REC_FUNC_DEL(CFC1, _Ft_); }

void recMOV(int info) { R5900::Dynarec::OpcodeImpl::recMOV(info); }
void recMFHI(int info) { R5900::Dynarec::OpcodeImpl::recMFHI(info); }
void recMTHI(int info) { R5900::Dynarec::OpcodeImpl::recMTHI(info); }
void recMFLO(int info) { R5900::Dynarec::OpcodeImpl::recMFLO(info); }
void recMTLO(int info) { R5900::Dynarec::OpcodeImpl::recMTLO(info); }

void recMFBPC(int info) { REC_FUNC_DEL(MFBPC, _Rt_); }
void recMFC2(int info) { REC_FUNC_DEL(MFC2, _Rt_); }
void recMTC2(int info) { REC_FUNC(MTC2); }
void recCFC2(int info) { REC_FUNC_DEL(CFC2, _Rt_); }
void recCTC2(int info) { REC_FUNC(CTC2); }

// Constant propagation implementations
void recRecompileCodeConst0(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo)
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		constcode();
	}
	else if (GPR_IS_CONST1(_Rs_))
	{
		constscode(xmminfo);
	}
	else if (GPR_IS_CONST1(_Rt_))
	{
		consttcode(xmminfo);
	}
	else
	{
		noconstcode(xmminfo);
	}
}

void recRecompileCodeConst1(R5900FNPTR constcode, R5900FNPTR noconstcode, int xmminfo)
{
	if (GPR_IS_CONST1(_Rs_))
	{
		constcode();
	}
	else
	{
		noconstcode(xmminfo);
	}
}

void recRecompileCodeConst2(R5900FNPTR constcode, R5900FNPTR noconstcode, int xmminfo)
{
	if (GPR_IS_CONST1(_Rt_))
	{
		constcode();
	}
	else
	{
		noconstcode(xmminfo);
	}
}

void recRecompileCodeConst3(R5900FNPTR constcode, R5900FNPTR constscode, R5900FNPTR consttcode, R5900FNPTR noconstcode, int LOHI)
{
	if (GPR_IS_CONST2(_Rs_, _Rt_))
	{
		constcode();
	}
	else if (GPR_IS_CONST1(_Rs_))
	{
		constscode();
	}
	else if (GPR_IS_CONST1(_Rt_))
	{
		consttcode();
	}
	else
	{
		noconstcode();
	}
}

void _deleteEEreg(int reg, int flush)
{
	if (flush)
	{
		_eeFlushConstReg(reg);
	}
	else
	{
		GPR_DEL_CONST(reg);
	}
}

void _flushEEregs()
{
	_eeFlushConstRegs();
	_flushArm64regs();
}

void _flushConstRegs()
{
	_eeFlushConstRegs();
}

void _flushConstReg(int reg)
{
	_eeFlushConstReg(reg);
}

void _eeFlushConstReg(int reg)
{
	if (reg < 32 && GPR_IS_DIRTY_CONST(reg))
	{
		// Write back constant to memory
		armStore(PTR_CPU(cpuRegs.GPR.r[reg].UD[0]), g_cpuConstRegs[reg].UD[0]);
		g_cpuFlushedConstReg |= (1 << reg);
	}
}

void _eeFlushConstRegs()
{
	for (int i = 0; i < 32; i++)
	{
		if (GPR_IS_DIRTY_CONST(i))
		{
			armStore(PTR_CPU(cpuRegs.GPR.r[i].UD[0]), g_cpuConstRegs[i].UD[0]);
			g_cpuFlushedConstReg |= (1 << i);
		}
	}
}

void _eeMoveGPRtoR(const a64::Register& to, int fromgpr)
{
	if (GPR_IS_CONST1(fromgpr))
	{
		armAsm->Mov(to, g_cpuConstRegs[fromgpr].UL[0]);
	}
	else
	{
		armLoad(to, PTR_CPU(cpuRegs.GPR.r[fromgpr].UL[0]));
	}
}

void _eeMoveGPRtoM(const a64::MemOperand& to, int fromgpr)
{
	if (GPR_IS_CONST1(fromgpr))
	{
		armStore(to, g_cpuConstRegs[fromgpr].UL[0]);
	}
	else
	{
		armLoad(a64::XRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[fromgpr].UL[0]));
		armStore(to, a64::XRegister(EEREC_S));
	}
}

void _eeMoveGPRtoR64(const a64::Register& to, int fromgpr)
{
	if (GPR_IS_CONST1(fromgpr))
	{
		armAsm->Mov(to, g_cpuConstRegs[fromgpr].UD[0]);
	}
	else
	{
		armLoad(to, PTR_CPU(cpuRegs.GPR.r[fromgpr].UD[0]));
	}
}

void _eeDeleteReg(int reg, int flush)
{
	_deleteEEreg(reg, flush);
}

void _eeFlushCall(int flushtype)
{
	_eeFlushConstRegs();
	_flushArm64regs();
}

void _eeFlushAllDirty()
{
	_eeFlushConstRegs();
	_flushArm64regs();
}

void _eeOnWriteReg(int reg)
{
	if (reg < 32)
	{
		GPR_DEL_CONST(reg);
	}
}

void recCall(void (*func)())
{
	// Stub - needs full implementation
}

void recBranchCall(void (*func)())
{
	// Stub - needs full implementation
}

void iFlushCall(int flushtype)
{
	// Stub - needs full implementation
}

u8* recBeginThunk()
{
	return nullptr;
}

u8* recEndThunk()
{
	return nullptr;
}

bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore)
{
	return false;
}

void SaveBranchState() {}
void LoadBranchState() {}

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot)
{
	// Stub - needs full implementation
}

void SetBranchReg(u32 reg) {}
void SetBranchImm(u32 imm) {}

u32 scaleblockcycles_clear()
{
	return 0;
}

namespace R5900
{
	namespace Dynarec
	{
		void recDoBranchImm(u32 branchTo, a64::Label* jmpSkip, bool isLikely, bool swappedDelaySlot)
		{
			// Stub - needs full implementation
		}
	}
}

// EE Dynarec ARM64 CPU implementation
R5900cpu jitA64Cpu = {
	recReserve,
	recShutdown,
	recResetEE,
	recStep,
	recExecute,
	recSafeExitExecution,
	recCancelInstruction,
	recClear
};
};
