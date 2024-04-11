#pragma once
#include "CoreTypes.h"
#include "Math/Vector.h"
#include "Math/Quat.h"

class iPiBinaryReader
{
public:

	iPiBinaryReader(unsigned char* buffer, int buffer_size_bytes)
		: buffer{ buffer }
		, buffer_end{ buffer + buffer_size_bytes }
		, cur_ptr{ buffer } 
	{

	}

	template<typename T> bool Read(T& out_val)
	{
		if (CheckRemainingSize(sizeof(T)))
		{
			auto size_of_t = sizeof(T);
			out_val = *(T*)(cur_ptr);
			cur_ptr += sizeof(T);
			return true;
		}
		else
		{
			return false;
		}
	}

	bool ReadVector(FVector& out_val)
	{
		float x, y, z;
		
		if (Read(x) && Read(y) && Read(z))
		{
			out_val.X = x;
			out_val.Y = y;
			out_val.Z = z;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool ReadQuaternion(FQuat& out_val)
	{
		float x, y, z, w;

		if (Read(x) && Read(y) && Read(z) && Read(w))
		{
			out_val.X = x;
			out_val.Y = y;
			out_val.Z = z;
			out_val.W = w;
			return true;
		}
		else
		{
			return false;
		}
	}

	bool Read7bitEncodedInt(int32& out_val)
	{
		int32 result = 0;
		do
		{
			if (cur_ptr >= buffer_end)
			{
				return false;
			}
			result = (result << 7) | (*cur_ptr & 0x7F);
		} while (*cur_ptr++ & 0x80);
		out_val = result;
		return true;
	}

	bool ReadString(FString& out_val)
	{
		int32 strLength;
		if (!Read7bitEncodedInt(strLength) || !CheckRemainingSize(strLength))
		{
			return false;
		}
		out_val = FString { strLength, (const ANSICHAR*)cur_ptr };
		cur_ptr += strLength;
		return true;
	}

private:
	unsigned char* buffer;
	unsigned char* buffer_end;
	unsigned char* cur_ptr{ nullptr };

	bool CheckRemainingSize(int32 size)
	{
		return buffer_end - cur_ptr >= size;
	}
};
