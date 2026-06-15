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

#include "common/AlignedMalloc.h"
#include "common/Perf.h"
#include "DebugTools/Breakpoints.h"

using namespace R5900;

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

	// TODO: Implement full execution loop
	// For now, just use interpreter
	return intCpu.ExecuteBlock(eeCycles);
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
	// TODO: Implement full recompilation
	// For now, this is a stub
}

static void dyna_block_discard(u32 start, u32 sz)
{
	// TODO: Implement block discard
}

static void dyna_page_reset(u32 start, u32 sz)
{
	// TODO: Implement page reset
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

void recADD(int info) { REC_FUNC_DEL(ADD, _Rt_); }
void recADDU(int info) { REC_FUNC_DEL(ADDU, _Rt_); }
void recDADDI(int info) { REC_FUNC_DEL(DADDI, _Rt_); }
void recDADDIU(int info) { REC_FUNC_DEL(DADDIU, _Rt_); }
void recDADD(int info) { REC_FUNC_DEL(DADD, _Rd_); }
void recDADDU(int info) { REC_FUNC_DEL(DADDU, _Rd_); }
void recSUB(int info) { REC_FUNC_DEL(SUB, _Rd_); }
void recSUBU(int info) { REC_FUNC_DEL(SUBU, _Rd_); }
void recDSUBI(int info) { REC_FUNC_DEL(DSUBI, _Rt_); }
void recDSUBIU(int info) { REC_FUNC_DEL(DSUBIU, _Rt_); }
void recDSUB(int info) { REC_FUNC_DEL(DSUB, _Rd_); }
void recDSUBU(int info) { REC_FUNC_DEL(DSUBU, _Rd_); }
void recAND(int info) { REC_FUNC_DEL(AND, _Rd_); }
void recOR(int info) { REC_FUNC_DEL(OR, _Rd_); }
void recXOR(int info) { REC_FUNC_DEL(XOR, _Rd_); }
void recNOR(int info) { REC_FUNC_DEL(NOR, _Rd_); }
void recSLT(int info) { REC_FUNC_DEL(SLT, _Rd_); }
void recSLTI(int info) { REC_FUNC_DEL(SLTI, _Rt_); }
void recSLTIU(int info) { REC_FUNC_DEL(SLTIU, _Rt_); }
void recSLTU(int info) { REC_FUNC_DEL(SLTU, _Rd_); }

void recSLL(int info) { REC_FUNC_DEL(SLL, _Rd_); }
void recSRL(int info) { REC_FUNC_DEL(SRL, _Rd_); }
void recSRA(int info) { REC_FUNC_DEL(SRA, _Rd_); }
void recSLLV(int info) { REC_FUNC_DEL(SLLV, _Rd_); }
void recSRLV(int info) { REC_FUNC_DEL(SRLV, _Rd_); }
void recSRAV(int info) { REC_FUNC_DEL(SRAV, _Rd_); }

void recMULT(int info) { REC_FUNC(MULT); }
void recMULTU(int info) { REC_FUNC(MULTU); }
void recDMULT(int info) { REC_FUNC(DMULT); }
void recDMULTU(int info) { REC_FUNC(DMULTU); }
void recDIV(int info) { REC_FUNC(DIV); }
void recDIVU(int info) { REC_FUNC(DIVU); }
void recDDIV(int info) { REC_FUNC(DDIV); }
void recDDIVU(int info) { REC_FUNC(DDIVU); }

void recMFC0(int info) { REC_FUNC_DEL(MFC0, _Rt_); }
void recMTC0(int info) { REC_FUNC(MTC0); }

void recBEQ(int info) { REC_SYS_DEL(BEQ, _Rt_); }
void recBNE(int info) { REC_SYS_DEL(BNE, _Rt_); }
void recBLTZ(int info) { REC_SYS_DEL(BLTZ, _Rs_); }
void recBGEZ(int info) { REC_SYS_DEL(BGEZ, _Rs_); }
void recBLEZ(int info) { REC_SYS_DEL(BLEZ, _Rs_); }
void recBGTZ(int info) { REC_SYS_DEL(BGTZ, _Rs_); }
void recBEQL(int info) { REC_SYS_DEL(BEQL, _Rt_); }
void recBNEL(int info) { REC_SYS_DEL(BNEL, _Rt_); }
void recBLTZL(int info) { REC_SYS_DEL(BLTZL, _Rs_); }
void recBGEZL(int info) { REC_SYS_DEL(BGEZL, _Rs_); }
void recBLEZL(int info) { REC_SYS_DEL(BLEZL, _Rs_); }
void recBGTZL(int info) { REC_SYS_DEL(BGTZL, _Rs_); }

void recJ(int info) { REC_SYS(J); }
void recJAL(int info) { REC_SYS(JAL); }
void recJR(int info) { REC_SYS_DEL(JR, _Rs_); }
void recJALR(int info) { REC_SYS_DEL(JALR, _Rs_); }

void recLB(int info) { REC_FUNC_DEL(LB, _Rt_); }
void recLBU(int info) { REC_FUNC_DEL(LBU, _Rt_); }
void recLH(int info) { REC_FUNC_DEL(LH, _Rt_); }
void recLHU(int info) { REC_FUNC_DEL(LHU, _Rt_); }
void recLW(int info) { REC_FUNC_DEL(LW, _Rt_); }
void recLWL(int info) { REC_FUNC_DEL(LWL, _Rt_); }
void recLWR(int info) { REC_FUNC_DEL(LWR, _Rt_); }
void recLD(int info) { REC_FUNC_DEL(LD, _Rt_); }
void recLDL(int info) { REC_FUNC_DEL(LDL, _Rt_); }
void recLDR(int info) { REC_FUNC_DEL(LDR, _Rt_); }
void recLQ(int info) { REC_FUNC_DEL(LQ, _Rt_); }

void recSB(int info) { REC_FUNC(SB); }
void recSH(int info) { REC_FUNC(SH); }
void recSW(int info) { REC_FUNC(SW); }
void recSWL(int info) { REC_FUNC(SWL); }
void recSWR(int info) { REC_FUNC(SWR); }
void recSD(int info) { REC_FUNC(SD); }
void recSDL(int info) { REC_FUNC(SDL); }
void recSDR(int info) { REC_FUNC(SDR); }
void recSQ(int info) { REC_FUNC(SQ); }

void recLWC1(int info) { REC_FUNC_DEL(LWC1, _Ft_); }
void recSWC1(int info) { REC_FUNC(SWC1); }
void recMTC1(int info) { REC_FUNC(MTC1); }
void recMFC1(int info) { REC_FUNC_DEL(MFC1, _Ft_); }
void recCTC1(int info) { REC_FUNC(CTC1); }
void recCFC1(int info) { REC_FUNC_DEL(CFC1, _Ft_); }

void recMOV(int info) { REC_FUNC_DEL(MOV, _Rd_); }
void recMFHI(int info) { REC_FUNC_DEL(MFHI, _Rd_); }
void recMTHI(int info) { REC_FUNC(MTHI); }
void recMFLO(int info) { REC_FUNC_DEL(MFLO, _Rd_); }
void recMTLO(int info) { REC_FUNC(MTLO); }

void recMFBPC(int info) { REC_FUNC_DEL(MFBPC, _Rt_); }
void recMFC2(int info) { REC_FUNC_DEL(MFC2, _Rt_); }
void recMTC2(int info) { REC_FUNC(MTC2); }
void recCFC2(int info) { REC_FUNC_DEL(CFC2, _Rt_); }
void recCTC2(int info) { REC_FUNC(CTC2); }

// Stub implementations for constant propagation
void recRecompileCodeConst0(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo)
{
	recCall(Interp::UNKNOWN);
}

void recRecompileCodeConst1(R5900FNPTR constcode, R5900FNPTR noconstcode, int xmminfo)
{
	recCall(Interp::UNKNOWN);
}

void recRecompileCodeConst2(R5900FNPTR constcode, R5900FNPTR noconstcode, int xmminfo)
{
	recCall(Interp::UNKNOWN);
}

void recRecompileCodeConst3(R5900FNPTR constcode, R5900FNPTR constscode, R5900FNPTR consttcode, R5900FNPTR noconstcode, int LOHI)
{
	recCall(Interp::UNKNOWN);
}

void _deleteEEreg(int reg, int flush) {}
void _flushEEregs() {}
void _flushConstRegs() {}
void _flushConstReg(int reg) {}

void _eeMoveGPRtoR(const a64::Register& to, int fromgpr) {}
void _eeMoveGPRtoM(const a64::MemOperand& to, int fromgpr) {}
void _eeMoveGPRtoR64(const a64::Register& to, int fromgpr) {}

void _eeFlushConstReg(int reg) {}
void _eeFlushConstRegs() {}

void _eeDeleteReg(int reg, int flush) {}
void _eeFlushCall(int flushtype) {}
void _eeFlushAllDirty() {}

void _eeOnWriteReg(int reg) {}

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
