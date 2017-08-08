/** @file CFrame.hpp
*  @brief Game's class
*
*
*  @author Tyldar (darreugne@gmail.com)
*/

#pragma once
#include "CommonHeaders.h"

namespace M2
{
	class ICFrame
	{
	public:
	};

	class C_Frame : public ICFrame
	{
	public:
		void MarkForNotify(int unk)
		{
			Mem::InvokeFunction<Mem::call_this, void>(0x14BA3D0, this, unk);
		}

		void SetName(const char *name)
		{
			Mem::InvokeFunction<Mem::call_this, void>(0x14BA350, this, name);
		}
	};
};