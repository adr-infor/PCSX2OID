// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Shift arithmetic                                      *
* Format:  OP rd, rt, sa                                *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

static void recMoveTtoD(int info)
{
	if (info & PROCESS_EE_T) {
		armAsm->Mov(a64::WRegister(EEREC_D), a64::WRegister(EEREC_T));
	}
	else {
		armLoad(a64::WRegister(EEREC_D), PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
	}
}

static void recMoveTtoD64(int info)
{
	if (info & PROCESS_EE_T) {
		armAsm->Mov(a64::XRegister(EEREC_D), a64::XRegister(EEREC_T));
	}
	else {
		armLoad(a64::XRegister(EEREC_D), PTR_CPU(cpuRegs.GPR.r[_Rt_].UD[0]));
	}
}

//// SLL
static void recSLL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UL[0] << _Sa_;
}

static void recSLL_(int info)
{
	recMoveTtoD(info);
	if (_Sa_ != 0) {
		armAsm->Lsl(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), _Sa_);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSLL(int info)
{
	recRecompileCodeConst1(recSLL_const, recSLL_, 0);
}

//// SRL
static void recSRL_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u32)(g_cpuConstRegs[_Rt_].UL[0] >> _Sa_);
}

static void recSRL_(int info)
{
	recMoveTtoD(info);
	if (_Sa_ != 0) {
		armAsm->Lsr(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), _Sa_);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSRL(int info)
{
	recRecompileCodeConst1(recSRL_const, recSRL_, 0);
}

//// SRA
static void recSRA_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> _Sa_);
}

static void recSRA_(int info)
{
	recMoveTtoD(info);
	if (_Sa_ != 0) {
		armAsm->Asr(a64::WRegister(EEREC_D), a64::WRegister(EEREC_D), _Sa_);
	}
	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSRA(int info)
{
	recRecompileCodeConst1(recSRA_const, recSRA_, 0);
}

//// SLLV (variable shift)
static void recSLLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = g_cpuConstRegs[_Rt_].UL[0] << (g_cpuConstRegs[_Rs_].UL[0] & 0x1F);
}

static void recSLLV_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);
	auto regS = a64::WRegister(EEREC_S);
	auto regT = a64::WRegister(EEREC_T);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->And(regS, regS, 0x1F);
			armAsm->Lsl(reg32, regT, regS);
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->And(regS, regS, 0x1F);
			armAsm->Lsl(reg32, reg32, regS);
		}
		else
		{
			armAsm->Mov(reg32, regT);
			armAsm->And(regS, regS, 0x1F);
			armAsm->Lsl(reg32, reg32, regS);
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armLoad(regT, PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Lsl(reg32, regT, regS);
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(regS, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Lsl(reg32, regT, regS);
	}
	else
	{
		armLoad(regT, PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armLoad(regS, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Lsl(reg32, regT, regS);
	}

	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSLLV(int info)
{
	recRecompileCodeConst1(recSLLV_const, recSLLV_, 0);
}

//// SRLV (variable shift)
static void recSRLV_const()
{
	g_cpuConstRegs[_Rd_].UD[0] = (u32)(g_cpuConstRegs[_Rt_].UL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1F));
}

static void recSRLV_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);
	auto regS = a64::WRegister(EEREC_S);
	auto regT = a64::WRegister(EEREC_T);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->And(regS, regS, 0x1F);
			armAsm->Lsr(reg32, regT, regS);
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->And(regS, regS, 0x1F);
			armAsm->Lsr(reg32, reg32, regS);
		}
		else
		{
			armAsm->Mov(reg32, regT);
			armAsm->And(regS, regS, 0x1F);
			armAsm->Lsr(reg32, reg32, regS);
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armLoad(regT, PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Lsr(reg32, regT, regS);
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(regS, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Lsr(reg32, regT, regS);
	}
	else
	{
		armLoad(regT, PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armLoad(regS, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Lsr(reg32, regT, regS);
	}

	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSRLV(int info)
{
	recRecompileCodeConst1(recSRLV_const, recSRLV_, 0);
}

//// SRAV (variable shift)
static void recSRAV_const()
{
	g_cpuConstRegs[_Rd_].SD[0] = (s32)(g_cpuConstRegs[_Rt_].SL[0] >> (g_cpuConstRegs[_Rs_].UL[0] & 0x1F));
}

static void recSRAV_(int info)
{
	auto reg32 = a64::WRegister(EEREC_D);
	auto regS = a64::WRegister(EEREC_S);
	auto regT = a64::WRegister(EEREC_T);

	if ((info & PROCESS_EE_S) && (info & PROCESS_EE_T))
	{
		if (EEREC_D == EEREC_S)
		{
			armAsm->And(regS, regS, 0x1F);
			armAsm->Asr(reg32, regT, regS);
		}
		else if (EEREC_D == EEREC_T)
		{
			armAsm->And(regS, regS, 0x1F);
			armAsm->Asr(reg32, reg32, regS);
		}
		else
		{
			armAsm->Mov(reg32, regT);
			armAsm->And(regS, regS, 0x1F);
			armAsm->Asr(reg32, reg32, regS);
		}
	}
	else if (info & PROCESS_EE_S)
	{
		armLoad(regT, PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Asr(reg32, regT, regS);
	}
	else if (info & PROCESS_EE_T)
	{
		armLoad(regS, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Asr(reg32, regT, regS);
	}
	else
	{
		armLoad(regT, PTR_CPU(cpuRegs.GPR.r[_Rt_].UL[0]));
		armLoad(regS, PTR_CPU(cpuRegs.GPR.r[_Rs_].UL[0]));
		armAsm->And(regS, regS, 0x1F);
		armAsm->Asr(reg32, regT, regS);
	}

	armAsm->Sxtw(a64::XRegister(EEREC_D), a64::WRegister(EEREC_D));
}

void recSRAV(int info)
{
	recRecompileCodeConst1(recSRAV_const, recSRAV_, 0);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
