// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

/*********************************************************
* Register move                                        *
* Format:  OP rd, rs                                    *
*********************************************************/

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {

	void recMOV();
	void recMFHI();
	void recMTHI();
	void recMFLO();
	void recMTLO();

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
