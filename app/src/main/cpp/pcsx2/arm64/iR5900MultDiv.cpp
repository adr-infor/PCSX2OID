// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* Multiply and Divide                                   *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// For now, multiplication and division operations call the interpreter
// because they are complex and require special handling of HI/LO registers
// These can be optimized later with ARM64 multiply/divide instructions

//// MULT (Multiply Signed)
void recMULT(int info)
{
	recCall(Interp::MULT);
}

//// MULTU (Multiply Unsigned)
void recMULTU(int info)
{
	recCall(Interp::MULTU);
}

//// DMULT (Doubleword Multiply Signed)
void recDMULT(int info)
{
	recCall(Interp::DMULT);
}

//// DMULTU (Doubleword Multiply Unsigned)
void recDMULTU(int info)
{
	recCall(Interp::DMULTU);
}

//// DIV (Divide Signed)
void recDIV(int info)
{
	recCall(Interp::DIV);
}

//// DIVU (Divide Unsigned)
void recDIVU(int info)
{
	recCall(Interp::DIVU);
}

//// DDIV (Doubleword Divide Signed)
void recDDIV(int info)
{
	recCall(Interp::DDIV);
}

//// DDIVU (Doubleword Divide Unsigned)
void recDDIVU(int info)
{
	recCall(Interp::DDIVU);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
