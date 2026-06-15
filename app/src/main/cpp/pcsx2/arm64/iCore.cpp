// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "iCore.h"
#include "iR5900.h"
#include "VixlHelpers.h"

// ARM64 GPR Register Allocation
_arm64regs arm64regs[iREGCNT_GPR];
_arm64regs s_saveArm64regs[iREGCNT_GPR];
static int s_arm64regcounter = 0;

// Q (128-bit) Register Allocation
_qregs qregs[iREGCNT_XMM];
_qregs s_saveQregs[iREGCNT_XMM];
static int s_qregcounter = 0;

void _initArm64regs()
{
	std::memset(arm64regs, 0, sizeof(arm64regs));
	s_arm64regcounter = 0;
}

void _initQregs()
{
	std::memset(qregs, 0, sizeof(qregs));
	s_qregcounter = 0;
}

bool _isAllocatableArm64reg(int arm64reg)
{
	// Scratch registers x16, x17 are not allocatable
	if (arm64reg == 16 || arm64reg == 17)
		return false;
	
	// x28 is reserved for fastmem base
	if (arm64reg == 28)
		return false;
	
	// x29 is reserved for state pointer
	if (arm64reg == 29)
		return false;
	
	return true;
}

int _getFreeArm64reg(int mode)
{
	int bestreg = -1;
	u32 bestcounter = 0xFFFFFFFF;

	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		if (!_isAllocatableArm64reg(i))
			continue;
		
		if (arm64regs[i].inuse)
			continue;
		
		if (arm64regs[i].counter < bestcounter)
		{
			bestcounter = arm64regs[i].counter;
			bestreg = i;
		}
	}

	if (bestreg == -1)
	{
		// No free register, need to flush one
		for (int i = 0; i < iREGCNT_GPR; ++i)
		{
			if (!_isAllocatableArm64reg(i))
				continue;
			
			if (arm64regs[i].inuse && !(arm64regs[i].mode & MODE_CALLEESAVED))
			{
				_writebackArm64Reg(i);
				bestreg = i;
				break;
			}
		}
	}

	return bestreg;
}

int _allocArm64reg(int type, int reg, int mode)
{
	int arm64reg = _checkArm64reg(type, reg, mode);
	
	if (arm64reg >= 0)
		return arm64reg;

	arm64reg = _getFreeArm64reg(mode);
	
	if (arm64reg < 0)
	{
		Console.Error("R5900: Register Allocation Error - no free registers");
		return -1;
	}

	arm64regs[arm64reg].inuse = 1;
	arm64regs[arm64reg].reg = reg;
	arm64regs[arm64reg].type = type;
	arm64regs[arm64reg].mode = mode;
	arm64regs[arm64reg].counter = ++s_arm64regcounter;
	arm64regs[arm64reg].needed = 1;
	arm64regs[arm64reg].extra = 0;

	return arm64reg;
}

int _checkArm64reg(int type, int reg, int mode)
{
	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		if (arm64regs[i].inuse && arm64regs[i].type == type && arm64regs[i].reg == reg)
		{
			if (mode != 0 && (arm64regs[i].mode & mode) == 0)
			{
				// Register is allocated but not in the requested mode
				// Need to update mode
				arm64regs[i].mode |= mode;
			}
			arm64regs[i].needed = 1;
			return i;
		}
	}
	return -1;
}

bool _hasArm64reg(int type, int reg, int required_mode)
{
	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		if (arm64regs[i].inuse && arm64regs[i].type == type && arm64regs[i].reg == reg)
		{
			if (required_mode == 0 || (arm64regs[i].mode & required_mode) != 0)
				return true;
		}
	}
	return false;
}

void _addNeededArm64reg(int type, int reg)
{
	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		if (arm64regs[i].inuse && arm64regs[i].type == type && arm64regs[i].reg == reg)
		{
			arm64regs[i].needed = 1;
			break;
		}
	}
}

void _clearNeededArm64regs()
{
	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		arm64regs[i].needed = 0;
	}
}

void _freeArm64reg(const a64::Register& arm64reg)
{
	_freeArm64reg(arm64reg.GetCode());
}

void _freeArm64reg(int arm64reg)
{
	if (arm64reg < 0 || arm64reg >= iREGCNT_GPR)
		return;

	if (arm64regs[arm64reg].inuse)
	{
		_writebackArm64Reg(arm64reg);
		arm64regs[arm64reg].inuse = 0;
		arm64regs[arm64reg].reg = -1;
		arm64regs[arm64reg].mode = 0;
		arm64regs[arm64reg].type = 0;
		arm64regs[arm64reg].needed = 0;
		arm64regs[arm64reg].extra = 0;
	}
}

void _freeArm64regWithoutWriteback(int arm64reg)
{
	if (arm64reg < 0 || arm64reg >= iREGCNT_GPR)
		return;

	if (arm64regs[arm64reg].inuse)
	{
		arm64regs[arm64reg].inuse = 0;
		arm64regs[arm64reg].reg = -1;
		arm64regs[arm64reg].mode = 0;
		arm64regs[arm64reg].type = 0;
		arm64regs[arm64reg].needed = 0;
		arm64regs[arm64reg].extra = 0;
	}
}

void _freeArm64regs()
{
	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		_freeArm64reg(i);
	}
}

void _writebackArm64Reg(int arm64reg)
{
	if (arm64reg < 0 || arm64reg >= iREGCNT_GPR)
		return;

	if (!arm64regs[arm64reg].inuse)
		return;

	if (!(arm64regs[arm64reg].mode & MODE_WRITE))
		return;

	int type = arm64regs[arm64reg].type;
	int reg = arm64regs[arm64reg].reg;

	switch (type)
	{
		case ARM64TYPE_GPR:
			// Write back to GPR
			armStore(PTR_CPU(cpuRegs.GPR.r[reg].UD[0]), a64::XRegister(arm64reg));
			break;
			
		case ARM64TYPE_FPRC:
			// Write back to FPR
			armStore(PTR_CPU(fpuRegs.fpr[reg].UL), a64::XRegister(arm64reg));
			break;
			
		case ARM64TYPE_VIREG:
			// Write back to VI register
			armStore(PTR_CPU(VU0.VI[reg].UL), a64::XRegister(arm64reg));
			break;
			
		default:
			break;
	}

	arm64regs[arm64reg].mode &= ~MODE_WRITE;
}

void _flushArm64regs()
{
	for (int i = 0; i < iREGCNT_GPR; ++i)
	{
		if (arm64regs[i].inuse && !arm64regs[i].needed)
		{
			_writebackArm64Reg(i);
			arm64regs[i].inuse = 0;
			arm64regs[i].reg = -1;
			arm64regs[i].mode = 0;
			arm64regs[i].type = 0;
			arm64regs[i].needed = 0;
			arm64regs[i].extra = 0;
		}
	}
}

void _flushConstRegs(bool delete_const)
{
	// TODO: Implement constant register flushing
}

void _flushConstReg(int reg)
{
	// TODO: Implement single constant register flushing
}

void _validateRegs()
{
	// Validation stub
}

// Q (128-bit) Register Allocation

int _checkQreg(int type, int reg)
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		if (qregs[i].inuse && qregs[i].type == type && qregs[i].reg == reg)
		{
			qregs[i].needed = 1;
			return i;
		}
	}
	return -1;
}

int _allocQreg(int type, int reg)
{
	int qreg = _checkQreg(type, reg);
	
	if (qreg >= 0)
		return qreg;

	// Find free Q register
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		// Skip scratch Q registers (q29-q31)
		if (i >= 29)
			continue;
		
		if (!qregs[i].inuse)
		{
			qregs[i].inuse = 1;
			qregs[i].reg = reg;
			qregs[i].type = type;
			qregs[i].mode = MODE_READ | MODE_WRITE;
			qregs[i].counter = ++s_qregcounter;
			qregs[i].needed = 1;
			return i;
		}
	}

	Console.Error("R5900: Q Register Allocation Error - no free registers");
	return -1;
}

void _clearQregs()
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		qregs[i].needed = 0;
	}
}

void _freeQreg(int qreg)
{
	if (qreg < 0 || qreg >= iREGCNT_XMM)
		return;

	if (qregs[qreg].inuse)
	{
		_writebackQreg(qreg);
		qregs[qreg].inuse = 0;
		qregs[qreg].reg = -1;
		qregs[qreg].type = 0;
		qregs[qreg].mode = 0;
		qregs[qreg].needed = 0;
	}
}

void _freeQregs()
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		_freeQreg(i);
	}
}

void _writebackQreg(int qreg)
{
	if (qreg < 0 || qreg >= iREGCNT_XMM)
		return;

	if (!qregs[qreg].inuse)
		return;

	if (!(qregs[qreg].mode & MODE_WRITE))
		return;

	int type = qregs[qreg].type;
	int reg = qregs[qreg].reg;

	switch (type)
	{
		case QTYPE_GPRREG:
			// Write back to GPR (64-bit)
			armStore(PTR_CPU(cpuRegs.GPR.r[reg].UD[0]), a64::QRegister(qreg).D());
			break;
			
		case QTYPE_FPREG:
			// Write back to FPR
			armStore(PTR_CPU(fpuRegs.fpr[reg].UL), a64::QRegister(qreg).S());
			break;
			
		case QTYPE_VFREG:
			// Write back to VF register
			armStore(PTR_CPU(VU0.VF[reg].UL), a64::QRegister(qreg).S());
			break;
			
		default:
			break;
	}

	qregs[qreg].mode &= ~MODE_WRITE;
}

void _flushQregs()
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		if (qregs[i].inuse && !qregs[i].needed)
		{
			_writebackQreg(i);
			qregs[i].inuse = 0;
			qregs[i].reg = -1;
			qregs[i].type = 0;
			qregs[i].mode = 0;
			qregs[i].needed = 0;
		}
	}
}

void _flushConstQregs()
{
	// TODO: Implement constant Q register flushing
}

// GPR allocation in Q registers
int _allocGPRtoQreg(int gprreg, int mode)
{
	int qreg = _checkQregGPR(gprreg, mode);
	
	if (qreg >= 0)
		return qreg;

	// Find free Q register
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		// Skip scratch Q registers (q29-q31)
		if (i >= 29)
			continue;
		
		if (!qregs[i].inuse)
		{
			qregs[i].inuse = 1;
			qregs[i].reg = gprreg;
			qregs[i].type = QTYPE_GPRREG;
			qregs[i].mode = mode;
			qregs[i].counter = ++s_qregcounter;
			qregs[i].needed = 1;
			
			// Load GPR into Q register
			armLoad(a64::QRegister(i).D(), PTR_CPU(cpuRegs.GPR.r[gprreg].UD[0]));
			
			return i;
		}
	}

	Console.Error("R5900: Q Register Allocation Error - no free registers");
	return -1;
}

int _checkQregGPR(int gprreg, int mode)
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		if (qregs[i].inuse && qregs[i].type == QTYPE_GPRREG && qregs[i].reg == gprreg)
		{
			qregs[i].needed = 1;
			return i;
		}
	}
	return -1;
}

void _freeQregGPR(int gprreg)
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		if (qregs[i].inuse && qregs[i].type == QTYPE_GPRREG && qregs[i].reg == gprreg)
		{
			_freeQreg(i);
			break;
		}
	}
}

void _flushQregGPR(int gprreg)
{
	for (int i = 0; i < iREGCNT_XMM; ++i)
	{
		if (qregs[i].inuse && qregs[i].type == QTYPE_GPRREG && qregs[i].reg == gprreg)
		{
			_writebackQreg(i);
			_freeQreg(i);
			break;
		}
	}
}

// COP2 register management
void mVUFreeCOP2GPR(int hostreg)
{
	// TODO: Implement COP2 GPR freeing
}

bool mVUIsReservedCOP2(int hostreg)
{
	// TODO: Implement COP2 reservation check
	return false;
}
