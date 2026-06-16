// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* COP1 (FPU) instructions                                *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recMFC1();
	void recMTC1();
	void recCFC1();
	void recCTC1();
	void recLWC1();
	void recSWC1();
	void recLDC1();
	void recSDC1();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
