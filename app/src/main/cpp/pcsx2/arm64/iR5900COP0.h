// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* COP0 instructions                                      *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recMFC0();
	void recMTC0();
	void recCFC0();
	void recCTC0();
	void recERET();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
