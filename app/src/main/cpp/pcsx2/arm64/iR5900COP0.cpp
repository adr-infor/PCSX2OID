// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "Common.h"
#include "R5900OpcodeTables.h"
#include "iR5900.h"

namespace R5900::Dynarec::OpcodeImpl
{
/*********************************************************
* COP0 instructions                                      *
*********************************************************/

namespace Interp = R5900::Interpreter::OpcodeImpl;

// For now, COP0 instructions call the interpreter
// These are complex and require special handling of system registers

//// MFC0 (Move from Coprocessor 0)
void recMFC0(int info)
{
	recCall(Interp::MFC0);
}

//// MTC0 (Move to Coprocessor 0)
void recMTC0(int info)
{
	recCall(Interp::MTC0);
}

//// CFC0 (Move Control from Coprocessor 0)
void recCFC0(int info)
{
	recCall(Interp::CFC0);
}

//// CTC0 (Move Control to Coprocessor 0)
void recCTC0(int info)
{
	recCall(Interp::CTC0);
}

//// ERET (Exception Return)
void recERET(int info)
{
	recCall(Interp::ERET);
}

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
