// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "R5900.h"
#include "R5900_Profiler.h"
#include "VU.h"
#include "VixlHelpers.h"

// Register containing a pointer to our fastmem (4GB) area
#define RFASTMEMBASE a64::x28

extern u32 maxrecmem;
extern u32 pc;             // recompiler pc
extern int g_branch;       // set for branch
extern u32 target;         // branch target
extern u32 s_nBlockCycles; // cycles of current block recompiling
extern bool s_nBlockInterlocked; // Current block has VU0 interlocking

//////////////////////////////////////////////////////////////////////////////////////////
//

#define REC_FUNC(f) \
	void rec##f() \
	{ \
		recCall(Interp::f); \
	}

#define REC_FUNC_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recCall(Interp::f); \
	}

#define REC_SYS(f) \
	void rec##f() \
	{ \
		recBranchCall(Interp::f); \
	}

#define REC_SYS_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recBranchCall(Interp::f); \
	}

extern bool g_recompilingDelaySlot;

// Used for generating backpatch thunks for fastmem.
u8* recBeginThunk();
u8* recEndThunk();

// used when processing branches
bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore);
void SaveBranchState();
void LoadBranchState();

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot);
void SetBranchReg(u32 reg);
void SetBranchImm(u32 imm);

void iFlushCall(int flushtype);
void recBranchCall(void (*func)());
void recCall(void (*func)());
u32 scaleblockcycles_clear();

namespace R5900
{
	namespace Dynarec
	{
		extern void recDoBranchImm(u32 branchTo, a64::Label* jmpSkip, bool isLikely = false, bool swappedDelaySlot = false);
	} // namespace Dynarec
} // namespace R5900

////////////////////////////////////////////////////////////////////
// Constant Propagation - From here to the end of the header!

#define GPR_IS_CONST1(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))))
#define GPR_IS_CONST2(reg1, reg2) (EE_CONST_PROP && (g_cpuHasConstReg & (1 << (reg1))) && (g_cpuHasConstReg & (1 << (reg2))))
#define GPR_IS_DIRTY_CONST(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))) && (!(g_cpuFlushedConstReg & (1 << (reg)))))
#define GPR_SET_CONST(reg) \
	{ \
		if ((reg) < 32) \
		{ \
			g_cpuHasConstReg |= (1 << (reg)); \
			g_cpuFlushedConstReg &= ~(1 << (reg)); \
		} \
	}

#define GPR_DEL_CONST(reg) \
	{ \
		if ((reg) < 32) \
			g_cpuHasConstReg &= ~(1 << (reg)); \
	}

extern GPR_reg64 g_cpuConstRegs[32];
extern u32 g_cpuHasConstReg, g_cpuFlushedConstReg;
extern bool g_cpuFlushedPC, g_cpuFlushedCode, g_maySignalException;

typedef void (*R5900FNPTR)();
typedef void (*R5900FNPTR_INFO)(int info);

bool recTryRenameReg(int to, int from, int fromx86, int other, int xmminfo);

//
// non mmx/xmm version, slower
//
// rd = rs op rt
#define RECOMPILE_CONSTCODE0(fn, info) \
	void rec##fn(void) \
	{ \
		recRecompileCodeConst0(rec##fn##_const, rec##fn##_consts, rec##fn##_constt, rec##fn##_, info); \
	}

// rt = rs op imm16
#define RECOMPILE_CONSTCODE1(fn, info) \
	void rec##fn(void) \
	{ \
		recRecompileCodeConst1(rec##fn##_const, rec##fn##_, info); \
	}

// rd = rt op sa
#define RECOMPILE_CONSTCODE2(fn, info) \
	void rec##fn(void) \
	{ \
		recRecompileCodeConst2(rec##fn##_const, rec##fn##_, info); \
	}

// [lo,hi] = rt op rs
#define RECOMPILE_CONSTCODE3(fn, LOHI) \
	void rec##fn(void) \
	{ \
		recRecompileCodeConst3(rec##fn##_const, rec##fn##_consts, rec##fn##_constt, rec##fn##_, LOHI); \
	}

// rd = rs op rt
void recRecompileCodeConst0(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo);
// rt = rs op imm16
void recRecompileCodeConst1(R5900FNPTR constcode, R5900FNPTR noconstcode, int xmminfo);
// rd = rt op sa
void recRecompileCodeConst2(R5900FNPTR constcode, R5900FNPTR noconstcode, int xmminfo);
// [lo,hi] = rt op rs
void recRecompileCodeConst3(R5900FNPTR constcode, R5900FNPTR constscode, R5900FNPTR consttcode, R5900FNPTR noconstcode, int LOHI);

void _deleteEEreg(int reg, int flush);
void _flushEEregs();
void _flushConstRegs();
void _flushConstReg(int reg);

void _eeMoveGPRtoR(const a64::Register& to, int fromgpr);
void _eeMoveGPRtoM(const a64::MemOperand& to, int fromgpr);
void _eeMoveGPRtoR64(const a64::Register& to, int fromgpr);

void _eeFlushConstReg(int reg);
void _eeFlushConstRegs();

void _eeDeleteReg(int reg, int flush);
void _eeFlushCall(int flushtype);
void _eeFlushAllDirty();

void _eeOnWriteReg(int reg);

void _eeMoveGPRtoR(const a64::Register& to, int fromgpr);
void _eeMoveGPRtoM(const a64::MemOperand& to, int fromgpr);

void recADD(int info);
void recADDU(int info);
void recDADDI(int info);
void recDADDIU(int info);
void recDADD(int info);
void recDADDU(int info);
void recSUB(int info);
void recSUBU(int info);
void recDSUBI(int info);
void recDSUBIU(int info);
void recDSUB(int info);
void recDSUBU(int info);
void recAND(int info);
void recOR(int info);
void recXOR(int info);
void recNOR(int info);
void recSLT(int info);
void recSLTI(int info);
void recSLTIU(int info);
void recSLTU(int info);

void recSLL(int info);
void recSRL(int info);
void recSRA(int info);
void recSLLV(int info);
void recSRLV(int info);
void recSRAV(int info);

void recMULT(int info);
void recMULTU(int info);
void recDMULT(int info);
void recDMULTU(int info);
void recDIV(int info);
void recDIVU(int info);
void recDDIV(int info);
void recDDIVU(int info);

void recMFC0(int info);
void recMTC0(int info);

void recBEQ(int info);
void recBNE(int info);
void recBLTZ(int info);
void recBGEZ(int info);
void recBLEZ(int info);
void recBGTZ(int info);
void recBEQL(int info);
void recBNEL(int info);
void recBLTZL(int info);
void recBGEZL(int info);
void recBLEZL(int info);
void recBGTZL(int info);

void recJ(int info);
void recJAL(int info);
void recJR(int info);
void recJALR(int info);

void recLB(int info);
void recLBU(int info);
void recLH(int info);
void recLHU(int info);
void recLW(int info);
void recLWL(int info);
void recLWR(int info);
void recLD(int info);
void recLDL(int info);
void recLDR(int info);
void recLQ(int info);

void recSB(int info);
void recSH(int info);
void recSW(int info);
void recSWL(int info);
void recSWR(int info);
void recSD(int info);
void recSDL(int info);
void recSDR(int info);
void recSQ(int info);

void recLWC1(int info);
void recSWC1(int info);
void recMTC1(int info);
void recMFC1(int info);
void recCTC1(int info);
void recCFC1(int info);

void recMOV(int info);
void recMFHI(int info);
void recMTHI(int info);
void recMFLO(int info);
void recMTLO(int info);

void recMFBPC(int info);
void recMFC2(int info);
void recMTC2(int info);
void recCFC2(int info);
void recCTC2(int info);
