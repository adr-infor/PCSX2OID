// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Branch instructions                                    *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// Helper to compare registers
static void recCompareRegs(int info)
{
	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_S)
	{
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armLoad(a64::WRegister(EEREC_T), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), a64::WRegister(EEREC_T));
	}
}

//// BEQ (Branch if Equal)
static void recBEQ_const()
{
	if (g_cpuConstRegs[_Rs_].UL[0] == g_cpuConstRegs[_Rt_].UL[0])
	{
		// Branch taken
	}
	else
	{
		// Branch not taken
	}
}

static void recBEQ_(int info)
{
	recCompareRegs(info);
	
	// TODO: Implement branch logic
	// For now, call interpreter
	recBranchCall(Interp::BEQ);
}

void recBEQ(int info)
{
	recRecompileCodeConst0(recBEQ_const, recBEQ_const, recBEQ_const, recBEQ_, 0);
}

//// BNE (Branch if Not Equal)
static void recBNE_const()
{
	if (g_cpuConstRegs[_Rs_].UL[0] != g_cpuConstRegs[_Rt_].UL[0])
	{
		// Branch taken
	}
	else
	{
		// Branch not taken
	}
}

static void recBNE_(int info)
{
	recCompareRegs(info);
	recBranchCall(Interp::BNE);
}

void recBNE(int info)
{
	recRecompileCodeConst0(recBNE_const, recBNE_const, recBNE_const, recBNE_, 0);
}

//// BLTZ (Branch if Less Than Zero)
static void recBLTZ_const()
{
	if ((s32)g_cpuConstRegs[_Rs_].SL[0] < 0)
	{
		// Branch taken
	}
	else
	{
		// Branch not taken
	}
}

static void recBLTZ_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BLTZ);
}

void recBLTZ(int info)
{
	recRecompileCodeConst1(recBLTZ_const, recBLTZ_, 0);
}

//// BGEZ (Branch if Greater or Equal Zero)
static void recBGEZ_const()
{
	if ((s32)g_cpuConstRegs[_Rs_].SL[0] >= 0)
	{
		// Branch taken
	}
	else
	{
		// Branch not taken
	}
}

static void recBGEZ_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BGEZ);
}

void recBGEZ(int info)
{
	recRecompileCodeConst1(recBGEZ_const, recBGEZ_, 0);
}

//// BLEZ (Branch if Less or Equal Zero)
static void recBLEZ_const()
{
	if ((s32)g_cpuConstRegs[_Rs_].SL[0] <= 0)
	{
		// Branch taken
	}
	else
	{
		// Branch not taken
	}
}

static void recBLEZ_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BLEZ);
}

void recBLEZ(int info)
{
	recRecompileCodeConst1(recBLEZ_const, recBLEZ_, 0);
}

//// BGTZ (Branch if Greater Than Zero)
static void recBGTZ_const()
{
	if ((s32)g_cpuConstRegs[_Rs_].SL[0] > 0)
	{
		// Branch taken
	}
	else
	{
		// Branch not taken
	}
}

static void recBGTZ_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BGTZ);
}

void recBGTZ(int info)
{
	recRecompileCodeConst1(recBGTZ_const, recBGTZ_, 0);
}

//// BEQL (Branch if Equal Likely)
static void recBEQL_const()
{
	recBEQ_const();
}

static void recBEQL_(int info)
{
	recCompareRegs(info);
	recBranchCall(Interp::BEQL);
}

void recBEQL(int info)
{
	recRecompileCodeConst0(recBEQL_const, recBEQL_const, recBEQL_const, recBEQL_, 0);
}

//// BNEL (Branch if Not Equal Likely)
static void recBNEL_const()
{
	recBNE_const();
}

static void recBNEL_(int info)
{
	recCompareRegs(info);
	recBranchCall(Interp::BNEL);
}

void recBNEL(int info)
{
	recRecompileCodeConst0(recBNEL_const, recBNEL_const, recBNEL_const, recBNEL_, 0);
}

//// BLTZL (Branch if Less Than Zero Likely)
static void recBLTZL_const()
{
	recBLTZ_const();
}

static void recBLTZL_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BLTZL);
}

void recBLTZL(int info)
{
	recRecompileCodeConst1(recBLTZL_const, recBLTZL_, 0);
}

//// BGEZL (Branch if Greater or Equal Zero Likely)
static void recBGEZL_const()
{
	recBGEZ_const();
}

static void recBGEZL_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BGEZL);
}

void recBGEZL(int info)
{
	recRecompileCodeConst1(recBGEZL_const, recBGEZL_, 0);
}

//// BLEZL (Branch if Less or Equal Zero Likely)
static void recBLEZL_const()
{
	recBLEZ_const();
}

static void recBLEZL_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BLEZL);
}

void recBLEZL(int info)
{
	recRecompileCodeConst1(recBLEZL_const, recBLEZL_, 0);
}

//// BGTZL (Branch if Greater Than Zero Likely)
static void recBGTZL_const()
{
	recBGTZ_const();
}

static void recBGTZL_(int info)
{
	if (info & PROCESS_EE_S)
	{
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	else
	{
		armLoad(a64::WRegister(EEREC_S), PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->Cmp(a64::WRegister(EEREC_S), 0);
	}
	recBranchCall(Interp::BGTZL);
}

void recBGTZL(int info)
{
	recRecompileCodeConst1(recBGTZL_const, recBGTZL_, 0);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
