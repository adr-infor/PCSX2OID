// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* Load and Store                                      *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recLB();
	void recLBU();
	void recLH();
	void recLHU();
	void recLW();
	void recLWL();
	void recLWR();
	void recLD();
	void recLDL();
	void recLDR();
	void recLQ();

	void recSB();
	void recSH();
	void recSW();
	void recSWL();
	void recSWR();
	void recSD();
	void recSDL();
	void recSDR();
	void recSQ();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
